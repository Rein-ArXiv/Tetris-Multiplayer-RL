#ifndef PLATFORM_UTF8_ARGUMENTS_H
#define PLATFORM_UTF8_ARGUMENTS_H

#include <string>
#include <vector>

#if defined(_WIN32)

namespace platform {

// Converts the wide-character argv received by wmain into UTF-8 encoded
// arguments. Throws std::runtime_error on failure.
std::vector<std::string> utf8_arguments(int argc, const wchar_t* const* argv);

}  // namespace platform

#endif  // _WIN32

#endif  // PLATFORM_UTF8_ARGUMENTS_H
