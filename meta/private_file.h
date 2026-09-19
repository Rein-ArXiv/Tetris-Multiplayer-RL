#pragma once
#include <string>
#include <cstdint>
namespace meta::client {
// Cross-process lock for one account folder. The OS releases it on process exit;
// the persistent lock file contains no credential and is not a lock-state flag.
class AccountFileLock {
  public:
    explicit AccountFileLock(const std::string &path);
    ~AccountFileLock();
    bool locked() const {
        return handle_ != -1;
    }
    AccountFileLock(const AccountFileLock &) = delete;
    AccountFileLock &operator=(const AccountFileLock &) = delete;

  private:
    intptr_t handle_ = -1;
};
// Atomic replacement in the same directory. POSIX: 0600 + fsync(file, directory).
// Windows: owner/SYSTEM-only ACL + FlushFileBuffers + MoveFileEx WRITE_THROUGH.
bool write_private_file(const std::string &path, const std::string &contents);
} // namespace meta::client
