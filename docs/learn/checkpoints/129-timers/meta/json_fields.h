#ifndef META_JSON_FIELDS_H
#define META_JSON_FIELDS_H

// Typed field extraction; callers check allowed keys and domain ranges.
#include "meta/json_document.h"

#include <cstdint>
#include <optional>
#include <string>

namespace study_meta {

// Extract an integer field as std::uint64_t.
//
// Requirements:
//   * object must be a JSON object
//   * name must exist as a key
//   * the value must be a JSON integer (is_number_integer())
//
// Strings, booleans, nulls and floating-point numbers are rejected rather than
// coerced. Unsigned values are read as std::uint64_t so UINT64_MAX is
// preserved exactly. Signed values are read as std::int64_t and negatives are
// rejected before any unsigned cast.
inline std::optional<std::uint64_t> number(const Json& object, const char* name) {
    if (!object.is_object() || name == nullptr) {
        return std::nullopt;
    }

    const auto it = object.find(name);
    if (it == object.end()) {
        return std::nullopt;
    }

    const Json& value = *it;
    if (!value.is_number_integer()) {
        return std::nullopt;
    }

    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>();
    }

    const std::int64_t signed_value = value.get<std::int64_t>();
    if (signed_value < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(signed_value);
}

// Extract a string field as std::string.
//
// Requirements:
//   * object must be a JSON object
//   * name must exist as a key
//   * the value must be a JSON string (is_string())
//
// The returned string keeps its full length, including an empty value and any
// embedded NUL produced by decoding an escape sequence. Missing keys and
// non-string values return std::nullopt.
inline std::optional<std::string> text(const Json& object, const char* name) {
    if (!object.is_object() || name == nullptr) {
        return std::nullopt;
    }

    const auto it = object.find(name);
    if (it == object.end()) {
        return std::nullopt;
    }

    if (!it->is_string()) {
        return std::nullopt;
    }

    return it->get<std::string>();
}

}  // namespace study_meta

#endif  // META_JSON_FIELDS_H
