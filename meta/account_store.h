#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace meta::client {
std::optional<std::string> read_account_file(const std::string &path);
bool account_file_exists(const std::string &path);

// Paths and durable credentials for one canonical API origin. Folder hashes are
// locators only: every credential document also checks its exact origin before use.
class AccountStore {
  public:
    explicit AccountStore(std::string origin);
    const std::string &origin() const {
        return origin_;
    }
    std::string path(const char *name) const;
    std::string token_path() const {
        return path("account.json");
    }
    std::string recovery_path() const {
        return path("account-recovery.json");
    }
    std::string pending_path() const {
        return path("account-change.pending.json");
    }
    std::string lock_path() const {
        return path("account-change.lock");
    }
    std::string load_token() const;
    bool save_token(const std::string &token) const;
    bool owns_existing_files() const;
    bool has_saved_account() const;
    bool has_legacy_account() const;
    std::string legacy_path(const char *name) const;

  private:
    std::string origin_;
    std::filesystem::path folder_;
};
} // namespace meta::client
