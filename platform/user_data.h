#pragma once
#include <filesystem>
#include <string>

namespace platform {
// Writable application data, independent of HTTP, rendering, or the working directory.
std::filesystem::path user_data_directory();
std::string settings_file_path();
} // namespace platform
