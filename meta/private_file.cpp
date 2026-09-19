#include "private_file.h"
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
namespace meta::client {
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

bool write_private_file(const std::string &path, const std::string &contents) {
    namespace fs = std::filesystem;
    if (path.empty() || contents.size() > 16384)
        return false;
    const auto destination = fs::u8path(path);
    const auto parent = destination.parent_path();
    if (parent.empty())
        return false;
    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec)
        return false;
#ifdef _WIN32
    HANDLE access = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &access))
        return false;
    DWORD size = 0;
    GetTokenInformation(access, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> user(size);
    bool ok = GetTokenInformation(access, TokenUser, user.data(), size, &size) != 0;
    CloseHandle(access);
    if (!ok)
        return false;
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid, &sid))
        return false;
    const std::wstring acl = L"D:P(A;;FA;;;SY)(A;;FA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1, &descriptor,
                                                              nullptr))
        return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    fs::path temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    // CREATE_NEW prevents following an attacker-created temporary path.
    for (unsigned attempt = 0; attempt < 128 && file == INVALID_HANDLE_VALUE; ++attempt) {
        temporary = destination;
        temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                     std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt);
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
        if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS)
            break;
    }
    LocalFree(descriptor);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    ok = WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) &&
         written == contents.size() && FlushFileBuffers(file);
    if (!CloseHandle(file))
        ok = false;
    if (ok)
        ok = MoveFileExW(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok)
        DeleteFileW(temporary.c_str());
    return ok;
#else
    std::string pattern = destination.string() + ".tmp-XXXXXX";
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    const int fd = mkstemp(temporary.data());
    if (fd < 0)
        return false;
    bool ok = fchmod(fd, S_IRUSR | S_IWUSR) == 0;
    size_t written = 0;
    while (ok && written < contents.size()) {
        const auto n = write(fd, contents.data() + written, contents.size() - written);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            ok = false;
            break;
        }
        written += static_cast<size_t>(n);
    }
    if (ok && fsync(fd) != 0)
        ok = false;
    if (close(fd) != 0)
        ok = false;
    if (ok && rename(temporary.data(), destination.c_str()) != 0)
        ok = false;
    if (ok) {
        const int dir = open(parent.c_str(), O_RDONLY | O_DIRECTORY);
        if (dir < 0)
            ok = false;
        else {
            if (fsync(dir) != 0)
                ok = false;
            close(dir);
        }
    }
    if (!ok)
        unlink(temporary.data());
    return ok;
#endif
}
} // namespace meta::client
