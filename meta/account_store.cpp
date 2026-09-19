#include "account_store.h"
#include "credentials.h"
#include "private_file.h"
#include "protocol.h"
#include "../platform/user_data.h"
#include <fstream>
#include <iomanip>
#include <sstream>

namespace meta::client {
std::optional<std::string> read_account_file(const std::string &path) {
    if (path.empty())
        return std::nullopt;
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file)
        return std::nullopt;
    std::string contents(16385, '\0');
    file.read(contents.data(), contents.size());
    contents.resize(static_cast<size_t>(file.gcount()));
    if (contents.size() > 16384 || file.bad())
        return std::nullopt;
    return contents;
}
bool account_file_exists(const std::string &path) {
    if (path.empty())
        return true; // Unavailable directory must not look like a new account.
    std::error_code error;
    const bool exists = std::filesystem::exists(std::filesystem::u8path(path), error);
    return exists || bool(error);
}
AccountStore::AccountStore(std::string origin) : origin_(std::move(origin)) {
    const auto root = platform::user_data_directory();
    if (root.empty() || origin_.empty())
        return;
    // FNV is a short portable path, NOT an authentication hash. Collision safety
    // comes from checking the full origin on every read and before replacement.
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : origin_) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream name;
    name << std::hex << std::setfill('0') << std::setw(16) << hash;
    folder_ = root / "accounts" / name.str();
}
std::string AccountStore::path(const char *name) const {
    return folder_.empty() ? "" : (folder_ / name).u8string();
}
std::string AccountStore::legacy_path(const char *name) const {
    const auto root = platform::user_data_directory();
    return root.empty() ? "" : (root / name).u8string();
}
std::string AccountStore::load_token() const {
    const auto saved = read_account_file(token_path());
    if (!saved || proto::find_string(*saved, "api_url") != origin_)
        return {};
    const auto token = proto::find_string(*saved, "token");
    return credentials::account(token) ? token : "";
}
bool AccountStore::save_token(const std::string &token) const {
    if (!credentials::account(token) || !owns_existing_files())
        return false;
    return write_private_file(token_path(),
                              json_input::Json{{"api_url", origin_}, {"token", token}}.dump() + "\n");
}
bool AccountStore::owns_existing_files() const {
    if (folder_.empty())
        return false;
    // Check all documents before a write or a credential-bearing request. A path
    // collision must not overwrite another server's backup or pending operation.
    for (const auto &path : {token_path(), recovery_path(), pending_path()}) {
        if (!account_file_exists(path))
            continue;
        const auto saved = read_account_file(path);
        if (!saved || proto::find_string(*saved, "api_url") != origin_)
            return false;
    }
    return true;
}
bool AccountStore::has_saved_account() const {
    return account_file_exists(token_path()) || account_file_exists(recovery_path()) ||
           account_file_exists(pending_path());
}
bool AccountStore::has_legacy_account() const {
    return account_file_exists(legacy_path("token")) ||
           account_file_exists(legacy_path("account-recovery.json")) ||
           account_file_exists(legacy_path("account-change.pending.json"));
}
} // namespace meta::client
