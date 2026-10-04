#pragma once
#include "meta/json_document.h"
#include <array>
#include <filesystem>
#include <fstream>
namespace study_meta {
enum class LocalRead { missing, ready, too_large, unavailable };
struct LocalDocument { LocalRead state = LocalRead::unavailable; std::string body; };
// Read one bounded local document without following a final-component symlink.
// Application-owned folder and cooperative writers remain caller preconditions.
inline LocalDocument read_local_document(const std::filesystem::path& path) {
    try {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error == std::errc::no_such_file_or_directory ||
            (!error && status.type() == std::filesystem::file_type::not_found))
            return {LocalRead::missing, {}};
        if (error || !std::filesystem::is_regular_file(status)) return {};
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return {};
        std::array<char, kJsonBodyBytes + 1> bytes{};
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (stream.bad() || (stream.fail() && !stream.eof())) return {};
        const auto count = static_cast<std::size_t>(stream.gcount());
        if (count > kJsonBodyBytes) return {LocalRead::too_large, {}};
        return {LocalRead::ready, std::string(bytes.data(), count)};
    } catch (const std::exception&) { return {}; }
}
} // namespace study_meta
