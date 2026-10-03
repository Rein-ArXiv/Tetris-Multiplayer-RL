#include "account_file_lock.h"
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <cstdio>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace study_meta {
AccountFileLock::AccountFileLock(const std::string &path) {
    if (path.empty())
        return;
    const auto file = std::filesystem::u8path(path);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec)
        return;
#ifdef _WIN32
    const auto handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE)
        handle_ = reinterpret_cast<intptr_t>(handle);
#else
    const auto fd = open(file.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    if (fd >= 0) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0)
            handle_ = fd;
        else
            close(fd);
    }
#endif
}
AccountFileLock::~AccountFileLock() {
    if (handle_ == -1)
        return;
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
    close(static_cast<int>(handle_));
#endif
}

} // namespace study_meta
