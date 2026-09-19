#include "user_data.h"
#include <cstdlib>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif

namespace platform {
std::filesystem::path user_data_directory() {
    namespace fs = std::filesystem;
    fs::path root;
#ifdef _WIN32
    // Windows environment paths are UTF-16. Narrow getenv would corrupt profile
    // roots containing characters outside the active system code page.
    if (const wchar_t *overrideRoot = _wgetenv(L"TETRIS_USER_DATA_ROOT")) {
        root = overrideRoot;
#else
    if (const char *overrideRoot = std::getenv("TETRIS_USER_DATA_ROOT")) {
        root = fs::u8path(overrideRoot);
#endif
        return root.is_absolute() ? root / "Tetris" : fs::path{};
    }
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buffer)))
        root = buffer;
#elif defined(__APPLE__)
    if (const char *home = std::getenv("HOME"))
        root = fs::path(home) / "Library" / "Application Support";
#else
    if (const char *xdg = std::getenv("XDG_DATA_HOME"))
        root = xdg;
    else if (const char *home = std::getenv("HOME"))
        root = fs::path(home) / ".local" / "share";
#endif
    return root.is_absolute() ? root / "Tetris" : fs::path{};
}
std::string settings_file_path() {
    const auto root = user_data_directory();
    return root.empty() ? "" : (root / "settings.cfg").u8string();
}
} // namespace platform
