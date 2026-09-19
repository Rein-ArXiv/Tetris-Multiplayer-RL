#pragma once
#include <string>

namespace meta::credentials {
inline bool hex(const std::string &value, size_t length) {
    return value.size() == length && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline bool account(const std::string &value) {
    return hex(value, 32);
}
inline bool recovery(const std::string &value) {
    return value.size() == 68 && value.compare(0, 4, "rc1.") == 0 && hex(value.substr(4), 64);
}
// Domain separation prevents a digest in one credential role being used in another.
// These are uniformly random bearer secrets, not user-chosen passwords.
std::string digest(const std::string &purpose, const std::string &value);
} // namespace meta::credentials
