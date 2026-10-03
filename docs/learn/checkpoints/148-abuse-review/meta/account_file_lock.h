#pragma once
#include <string>
#include <cstdint>
namespace study_meta {
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
} // namespace study_meta
