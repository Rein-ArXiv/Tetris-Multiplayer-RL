#pragma once
#include "meta/account_file_lock.h"
#include "meta/issued_wire.h"
#include "settings/private_file.h"
#include <filesystem>
#include <fstream>
#include <array>
#include <utility>
namespace study_meta {
enum class AccountLoad { missing, ready, invalid, wrong_origin, io_error };
struct SavedAccount {
    AccountLoad status = AccountLoad::io_error;
    IssuedAccount account;
};
using PrivateWriter = bool (*)(const std::string&, const std::string&);
// The instance owns the cooperative process lock for its entire lifetime.
// The caller supplies an application-owned local folder, not a shared drop box.
class LocalAccountStore {
public:
    LocalAccountStore(std::filesystem::path folder, std::string origin,
                      PrivateWriter writer = study_files::write_private_file)
        : origin_(std::move(origin)), file_(folder / "account.json"),
          lock_((folder / "account.lock").u8string()), writer_(writer) {}
    bool available() const { return lock_.locked() && !origin_.empty(); }
    const std::string& origin() const { return origin_; }
    SavedAccount load() const {
        if (!available()) return {};
        try {
            std::error_code error;
            const auto status = std::filesystem::symlink_status(file_, error);
            if (error == std::errc::no_such_file_or_directory ||
                (!error && status.type() == std::filesystem::file_type::not_found))
                return {AccountLoad::missing, {}};
            if (error || !std::filesystem::is_regular_file(status)) return {};
            std::ifstream stream(file_, std::ios::binary);
            if (!stream) return {};
            std::array<char, kJsonBodyBytes + 1> bytes{};
            stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (stream.bad() || (stream.fail() && !stream.eof())) return {};
            const auto body = std::string(bytes.data(), static_cast<std::size_t>(stream.gcount()));
            const auto j = object(body, 3);
            if (!j) return {AccountLoad::invalid, {}};
            const auto origin = text(*j, "origin"), token = text(*j, "token");
            const auto id = number(*j, "player_id");
            if (!origin || !token || !id || !*id || !credential_hex(*token, 32))
                return {AccountLoad::invalid, {}};
            if (*origin != origin_) return {AccountLoad::wrong_origin, {}};
            return {AccountLoad::ready, {*id, *token}};
        } catch (const std::exception&) { return {}; }
    }
    bool save(const IssuedAccount& account) const {
        if (!available() || !account.player_id || !credential_hex(account.token,32)) return false;
        try {
            const auto old = load();
            if (old.status != AccountLoad::missing &&
                !(old.status == AccountLoad::ready && old.account.player_id == account.player_id &&
                  old.account.token == account.token)) return false;
            const auto body = Json{{"origin",origin_},{"player_id",account.player_id},
                                   {"token",account.token}}.dump() + "\n";
            return body.size() <= kJsonBodyBytes && writer_(file_.u8string(), body);
        } catch (const std::exception&) { return false; }
    }
private:
    std::string origin_;
    std::filesystem::path file_;
    AccountFileLock lock_;
    PrivateWriter writer_;
};
} // namespace study_meta
