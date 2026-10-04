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
// Publish a complete temporary file in an application-owned local directory.
// POSIX: same-directory rename, 0600, fsync(file and immediate parent).
// Windows: current-user/SYSTEM DACL, FlushFileBuffers, MoveFileEx WRITE_THROUGH.
// false means completion was not confirmed, NOT that the old bytes remain:
// parent-directory synchronization can fail after the new file is visible.
// No writer serialization or multi-file transaction; lock the entire update.
// Newly created ancestor directories are not recursively synchronized.
// Allocation/path conversion may throw; callers own the exception boundary.
bool write_private_file(const std::string &path, const std::string &contents);
} // namespace meta::client
