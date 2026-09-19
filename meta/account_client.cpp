#include "account_client.h"
#include "credentials.h"
#include "private_file.h"
#include "protocol.h"
#include <filesystem>
#include <sstream>
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#include <openssl/rand.h>
#endif

namespace meta::client {
namespace {
AccountOperation failed(const char *message, bool pending = false) {
    return {false, pending, message, {}, {}};
}
std::optional<std::string> random_hex(size_t size) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    unsigned char bytes[32];
    if (size > sizeof(bytes) || RAND_bytes(bytes, static_cast<int>(size)) != 1)
        return std::nullopt;
    static constexpr char hex[] = "0123456789abcdef";
    std::string value;
    for (size_t i = 0; i < size; ++i) {
        value += hex[bytes[i] >> 4];
        value += hex[bytes[i] & 15];
    }
    return value;
#else
    (void)size;
    return std::nullopt;
#endif
}
bool same_server(const std::string &saved, const MetaClient &api) {
    MetaClient prior(proto::find_string(saved, "api_url"));
    return prior.valid() && prior.baseUrl() == api.baseUrl();
}
AccountOperation resume_locked(MetaClient &api, const AccountStore &store) {
    if (!store.owns_existing_files())
        return failed("Account files are damaged or belong to another server. Keep them for recovery.",
                      account_file_exists(store.pending_path()));
    if (!account_file_exists(store.pending_path()))
        return {true, false, "No pending change.", {}, {}};
    const auto saved = read_account_file(store.pending_path());
    if (!saved)
        return failed("Cannot read pending change. Keep the file and retry.", true);
    if (!same_server(*saved, api))
        return failed("Pending change belongs to another server.", true);
    const auto operation = proto::find_string(*saved, "operation");
    const auto credential = proto::find_string(*saved, "credential");
    const auto token = proto::find_string(*saved, "next_token");
    const auto recovery = proto::find_string(*saved, "next_recovery");
    if ((operation != "backup" && operation != "rotate" && operation != "recover") ||
        !(operation == "recover" ? credentials::recovery(credential) : credentials::account(credential)) ||
        !credentials::account(token) || !credentials::recovery(recovery))
        return failed("Pending change is damaged. Keep the file.", true);

    int status = 0;
    const auto player = api.change_account(operation, credential, token, recovery, &status);
    if (!player) {
        // Preserve both candidate secrets whenever the server outcome is uncertain.
        // A definitive rejection can also mean a later operation superseded the receipt.
        if (status == 400 || status == 401 || status == 409) {
            std::error_code error;
            std::filesystem::remove(std::filesystem::u8path(store.pending_path()), error);
            return failed("Access key rejected. Use your latest recovery file.", bool(error));
        }
        return failed("Connection interrupted. Use Retry; keep pending file.", true);
    }
    const auto backup = json_input::Json{{"api_url", api.baseUrl()},
                                         {"player_id", player->player_id},
                                         {"recovery_code", recovery}}
                            .dump() +
                        "\n";
    if (!write_private_file(store.recovery_path(), backup) || !store.save_token(token))
        return failed("Server updated. Retry to finish saving on this device.", true);
    // Both active files are durable before removing the only retry journal.
    std::error_code error;
    std::filesystem::remove(std::filesystem::u8path(store.pending_path()), error);
    if (error)
        return failed("Keys saved. Retry to finish local cleanup.", true);
    return {true, false, "Saved. Keep the new recovery file somewhere safe.", token, player};
}
} // namespace

AccountOperation resume_account_change(MetaClient &api) {
    AccountStore store(api.baseUrl());
    AccountFileLock lock(store.lock_path());
    if (!lock.locked())
        return failed("Another account action is running, or the folder is unavailable.", true);
    return resume_locked(api, store);
}

AccountOperation bootstrap_account(MetaClient &api, bool create_separate) {
    if (!api.valid())
        return failed("Configure an HTTPS account server first.");
    AccountStore store(api.baseUrl());
    AccountFileLock lock(store.lock_path());
    if (!lock.locked())
        return failed("Account folder unavailable or in use. Retry before playing online.");
    const auto resumed = resume_locked(api, store);
    if (!resumed.ok || resumed.player)
        return resumed;
    const auto token = store.load_token();
    if (!token.empty()) {
        MetaClient::VerifyOutcome outcome;
        const auto player = api.verify_token(token, 3, &outcome);
        if (player)
            return {true, false, "Account connected.", token, player};
        if (outcome == MetaClient::VerifyOutcome::UnknownToken)
            return failed("Saved access key rejected. Restore your recovery file.");
        return {true, false, "Server unavailable. Your saved key is safe; retry later.", token, {}};
    }
    if (store.has_saved_account())
        return failed("Saved account needs recovery. Check the account folder.");
    if (!create_separate && store.has_legacy_account())
        return failed("Older account found. Confirm its server in Account & Recovery.");

    const auto guest = api.request_guest();
    if (!guest)
        return failed("Account server unavailable. Retry connecting later.");
    const AuthInfo player{guest->player_id, "", guest->elo, guest->bp, guest->xp, guest->selected_icon_id};
    if (!store.save_token(guest->token))
        return {false,        false,  "Account NOT saved. Retry saving before closing the game.",
                guest->token, player, true};
    return {true, false, "Account saved. Create a recovery file to keep your progress.", guest->token,
            player};
}

AccountOperation save_created_account(MetaClient &api, const std::string &token) {
    AccountStore store(api.baseUrl());
    AccountFileLock lock(store.lock_path());
    if (store.has_saved_account() && store.load_token() != token)
        return {false, false, "Another account is saved here. Keep this session open and check the folder.",
                token, {},    true};
    if (!lock.locked() || !store.save_token(token))
        return {false, false, "Account NOT saved. Check the folder and retry before closing.",
                token, {},    true};
    return {true, false, "Access key saved. Reconnect to refresh your profile.", token,
            api.verify_token(token)};
}

AccountOperation import_legacy_account(MetaClient &api) {
    // Only the explicit UI action may send an origin-less old token to this server.
    // Preserve the original files; a failed import never destroys the recovery source.
    AccountStore store(api.baseUrl());
    AccountFileLock oldLock(store.legacy_path("account-change.lock"));
    AccountFileLock lock(store.lock_path());
    if (!api.valid() || !oldLock.locked() || !lock.locked())
        return failed("Account folder unavailable or in use.");
    if (store.has_saved_account())
        return failed("This server already has saved account data. Use Restore or Retry.");
    for (const char *name : {"account-change.pending.json", "account-recovery.json"}) {
        if (!account_file_exists(store.legacy_path(name)))
            continue;
        const auto document = read_account_file(store.legacy_path(name));
        if (!document || !same_server(*document, api))
            return failed("Older account files belong to another server or are damaged.");
    }
    const auto pending = read_account_file(store.legacy_path("account-change.pending.json"));
    if (pending) {
        auto imported = *json_input::object(*pending);
        imported["api_url"] = api.baseUrl();
        if (!write_private_file(store.pending_path(), imported.dump() + "\n"))
            return failed("Could not copy pending change. Original kept.");
        return resume_locked(api, store);
    }
    const auto raw = read_account_file(store.legacy_path("token"));
    std::string token;
    if (raw) {
        std::istringstream input(*raw);
        input >> token;
    }
    const auto backup = read_account_file(store.legacy_path("account-recovery.json"));
    if (backup) {
        auto imported = *json_input::object(*backup);
        imported["api_url"] = api.baseUrl();
        if (!write_private_file(store.recovery_path(), imported.dump() + "\n"))
            return failed("Could not import recovery file. Original kept.");
    }
    if (!credentials::account(token))
        return failed("No older access key. Use Restore with the imported recovery file.");
    const auto player = api.verify_token(token);
    if (!player)
        return failed("Older key was not accepted. Check the selected server; original files kept.");
    if (!store.save_token(token))
        return failed("Could not save imported key. Original files kept.");
    return {true, false, "Older account imported for this server. Originals kept.", token, player};
}

AccountOperation change_saved_account(MetaClient &api, const std::string &operation) {
    if (!api.valid())
        return failed("Configure an HTTPS account server first.");
    AccountStore store(api.baseUrl());
    AccountFileLock lock(store.lock_path());
    if (!lock.locked())
        return failed("Another account action is running, or the folder is unavailable.", true);
    if (account_file_exists(store.pending_path()))
        return failed("Finish the pending change with Retry first.", true);
    if (!store.owns_existing_files())
        return failed("Account files are damaged or belong to another server. Keep them for recovery.");
    if (operation != "backup" && operation != "rotate" && operation != "recover")
        return failed("Unknown account action.");
    std::string credential = store.load_token();
    if (operation == "recover") {
        const auto saved = read_account_file(store.recovery_path());
        if (!saved || !same_server(*saved, api))
            return failed("Put this server's recovery file in the account folder.");
        credential = proto::find_string(*saved, "recovery_code");
        if (!credentials::recovery(credential))
            return failed("Recovery file is damaged.");
    } else if (!credentials::account(credential)) {
        return failed("No saved account. Connect to the server first.");
    }
    const auto token = operation == "backup" ? std::optional<std::string>(credential) : random_hex(16);
    const auto recovery = random_hex(32);
    if (!token || !recovery)
        return failed("Secure key generation unavailable in this build.");
    const auto journal = json_input::Json{{"api_url", api.baseUrl()},
                                          {"operation", operation},
                                          {"credential", credential},
                                          {"next_token", *token},
                                          {"next_recovery", "rc1." + *recovery}}
                             .dump() +
                         "\n";
    if (!write_private_file(store.pending_path(), journal))
        return failed("Cannot save keys. The server was not changed.");
    return resume_locked(api, store);
}
} // namespace meta::client
