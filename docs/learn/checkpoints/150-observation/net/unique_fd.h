#ifndef STUDY_NET_UNIQUE_FD_H
#define STUDY_NET_UNIQUE_FD_H

#if defined(__linux__)

#include <cerrno>
#include <unistd.h>

namespace study_net {

// UniqueFd owns exactly one POSIX file descriptor (e.g. an epoll or eventfd
// handle). It is move-only: the descriptor is closed on destruction, or on
// reset()/move-assignment, and ownership transfers when the object is moved.
class UniqueFd {
 public:
  UniqueFd() noexcept = default;

  explicit UniqueFd(int fd) noexcept : fd_(fd) {}

  ~UniqueFd() { reset(); }

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}

  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  int get() const noexcept { return fd_; }

  bool valid() const noexcept { return fd_ >= 0; }

  int release() noexcept {
    const int old = fd_;
    fd_ = -1;
    return old;
  }

  void reset(int replacement = -1) noexcept {
    if (replacement == fd_) {
      return;
    }
    if (fd_ >= 0) {
      // On Linux, close() may report EINTR, but the descriptor is nevertheless
      // released; retrying could close an unrelated descriptor that reused the
      // same number. Preserve the caller's errno so RAII cleanup never clobbers
      // a diagnostic value the caller still needs.
      const int saved_errno = errno;
      ::close(fd_);
      errno = saved_errno;
    }
    fd_ = replacement;
  }

 private:
  int fd_ = -1;
};

}  // namespace study_net

#endif  // defined(__linux__)

#endif  // STUDY_NET_UNIQUE_FD_H
