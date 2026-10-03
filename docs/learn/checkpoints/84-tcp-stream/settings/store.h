#pragma once
// Loading and saving settings files. Session gates saves with Loaded::writable();
// malformed, future-version, or IO-tainted files are left untouched so the
// user can fix them by hand (rename/copy the broken file away and restart).
#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "settings/config.h"
#include "settings/private_file.h"

namespace study_settings {

enum class LoadStatus { loaded, missing, invalid, io_error };

struct Loaded {
    Config config;
    LoadStatus status = LoadStatus::io_error;
    ParseError error = ParseError::none;
    std::size_t line = 0;

    // A missing file may be created; an invalid or unreadable one must not be
    // silently overwritten.
    bool writable() const noexcept {
        return status == LoadStatus::loaded || status == LoadStatus::missing;
    }
};

// Inspects, then reads at most kMaxFileBytes + 1 bytes. Missing files yield
// defaults; parse failures yield defaults plus diagnostics.
inline Loaded load(const std::filesystem::path& path) {
    Loaded result;

    std::error_code ec;
    const std::filesystem::file_status st = std::filesystem::status(path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            result.status = LoadStatus::missing;
            return result;
        }
        result.status = LoadStatus::io_error;
        return result;
    }
    if (st.type() == std::filesystem::file_type::not_found) {
        result.status = LoadStatus::missing;
        return result;
    }
    if (!std::filesystem::is_regular_file(st)) {
        result.status = LoadStatus::io_error;
        return result;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.status = LoadStatus::io_error;
        return result;
    }

    std::array<char, kMaxFileBytes + 1> buffer{};
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize got = in.gcount();

    if (in.bad()) {
        result.status = LoadStatus::io_error;
        return result;
    }
    if (got > static_cast<std::streamsize>(kMaxFileBytes)) {
        result.status = LoadStatus::invalid;
        result.error = ParseError::too_large;
        return result;
    }
    if (in.fail() && !in.eof()) {
        result.status = LoadStatus::io_error;
        return result;
    }

    const Parsed parsed =
        parse(std::string_view(buffer.data(), static_cast<std::size_t>(got)));
    if (!parsed) {
        result.config = Config{};
        result.status = LoadStatus::invalid;
        result.error = parsed.error;
        result.line = parsed.line;
        return result;
    }

    result.config = parsed.config;
    result.status = LoadStatus::loaded;
    return result;
}

// Encodes first; refuses empty paths; returns false on any failure. A false
// result does not guarantee the old bytes survived: the directory sync that
// follows the rename can fail after the new file is already published.
inline bool save(const std::filesystem::path& path, const Config& config) {
    try {
        const auto encoded = encode(config);
        if (!encoded || path.empty() || path.filename().empty()) return false;
        const auto normalized = path.parent_path().empty()
            ? std::filesystem::path(".") / path : path;
        return study_files::write_private_file(normalized.u8string(), *encoded);
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace study_settings
