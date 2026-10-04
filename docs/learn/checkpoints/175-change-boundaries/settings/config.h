#pragma once
// Settings parsing: a tiny, all-or-nothing text format.
// Only content/characters.h is included; no UI or game headers.
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "content/characters.h"

namespace study_settings {

inline constexpr std::size_t kMaxFileBytes = 4096;
inline constexpr std::size_t kMaxLineBytes = 256;

struct Config {
    bool decorations = true;
    std::string character = std::string(study_character_art::default_id);
};

enum class ParseError {
    none,
    too_large,
    line_too_long,
    malformed,
    unknown_key,
    duplicate_key,
    unsupported_version,
    invalid_value,
    missing_version,
};

struct Parsed {
    Config config;
    ParseError error = ParseError::none;
    std::size_t line = 0; // one-based; 0 for success or a whole-file size error

    explicit operator bool() const noexcept { return error == ParseError::none; }
};

namespace detail {

inline bool is_trim(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r';
}

inline std::string_view trim(std::string_view s) noexcept {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_trim(s[b])) ++b;
    while (e > b && is_trim(s[e - 1])) --e;
    return s.substr(b, e - b);
}

inline bool all_digits(std::string_view s) noexcept {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

} // namespace detail

// Parses the whole buffer at once. On any error the returned config is the
// default; diagnostics carry the one-based line where the problem was found.
inline Parsed parse(std::string_view text) {
    Parsed result;

    if (text.size() > kMaxFileBytes) {
        result.error = ParseError::too_large;
        return result;
    }

    // A NUL byte anywhere makes the file malformed.
    const std::size_t nul = text.find('\0');
    if (nul != std::string_view::npos) {
        std::size_t line = 1;
        for (std::size_t i = 0; i < nul; ++i) {
            if (text[i] == '\n') ++line;
        }
        result.error = ParseError::malformed;
        result.line = line;
        return result;
    }

    Config cfg;
    bool seen_version = false;
    bool seen_decorations = false;
    bool seen_character = false;

    std::size_t line_no = 0;
    std::size_t start = 0;
    const std::size_t n = text.size();

    while (start <= n) {
        std::size_t end = text.find('\n', start);
        const bool last = (end == std::string_view::npos);
        if (last) end = n;
        // A trailing newline does not create an extra line.
        if (last && start == n) break;

        ++line_no;
        std::string_view raw = text.substr(start, end - start);

        if (raw.size() > kMaxLineBytes) {
            result.error = ParseError::line_too_long;
            result.line = line_no;
            return result;
        }

        // Drop the comment first, then trim, then ignore blank lines.
        const std::size_t hash = raw.find('#');
        if (hash != std::string_view::npos) raw = raw.substr(0, hash);
        const std::string_view line = detail::trim(raw);

        if (!line.empty()) {
            const std::size_t eq = line.find('=');
            if (eq == std::string_view::npos ||
                line.find('=', eq + 1) != std::string_view::npos) {
                result.error = ParseError::malformed;
                result.line = line_no;
                return result;
            }

            const std::string_view key = detail::trim(line.substr(0, eq));
            const std::string_view value = detail::trim(line.substr(eq + 1));

            if (key.empty()) {
                result.error = ParseError::malformed;
                result.line = line_no;
                return result;
            }

            if (key == "version") {
                if (seen_version) {
                    result.error = ParseError::duplicate_key;
                    result.line = line_no;
                    return result;
                }
                seen_version = true;
                if (value == "1") {
                    // supported
                } else if (detail::all_digits(value)) {
                    result.error = ParseError::unsupported_version;
                    result.line = line_no;
                    return result;
                } else {
                    result.error = ParseError::invalid_value;
                    result.line = line_no;
                    return result;
                }
            } else if (key == "decorations") {
                if (seen_decorations) {
                    result.error = ParseError::duplicate_key;
                    result.line = line_no;
                    return result;
                }
                seen_decorations = true;
                if (value == "1") {
                    cfg.decorations = true;
                } else if (value == "0") {
                    cfg.decorations = false;
                } else {
                    result.error = ParseError::invalid_value;
                    result.line = line_no;
                    return result;
                }
            } else if (key == "character") {
                if (seen_character) {
                    result.error = ParseError::duplicate_key;
                    result.line = line_no;
                    return result;
                }
                seen_character = true;
                const study_character_art::Character* found =
                    study_character_art::find(value);
                if (found == nullptr) {
                    result.error = ParseError::invalid_value;
                    result.line = line_no;
                    return result;
                }
                // Copy the canonical id; never keep a view into the input.
                cfg.character.assign(found->id.data(), found->id.size());
            } else {
                result.error = ParseError::unknown_key;
                result.line = line_no;
                return result;
            }
        }

        start = end + 1;
    }

    if (!seen_version) {
        result.error = ParseError::missing_version;
        result.line = (line_no == 0) ? 1 : line_no;
        return result;
    }

    result.config = cfg;
    return result;
}

// A config is valid when its character id exists in the catalog.
inline bool valid(const Config& config) noexcept {
    return study_character_art::find(config.character) != nullptr;
}

// Canonical serialization; nullopt when the config is not valid.
inline std::optional<std::string> encode(const Config& config) {
    if (!valid(config)) return std::nullopt;
    std::string out;
    out.reserve(64);
    out += "version=1\n";
    out += "decorations=";
    out += config.decorations ? '1' : '0';
    out += "\ncharacter=";
    out += config.character;
    out += "\n";
    return out;
}

} // namespace study_settings
