#include "platform/utf8_arguments.h"
#include <iostream>
#include <stdexcept>

static void require(bool result) {
    if (!result) throw std::runtime_error("UTF-8 argument contract failed");
}

int main() {
    const wchar_t* arguments[] = {L"meta.exe", L"", L"a b", L"\uD55C\uAE00\U0001F3AE", L"C:\\data\\"};
    const auto result = platform::utf8_arguments(5, arguments);
    require(result == std::vector<std::string>{"meta.exe", "", "a b",
        "\xed\x95\x9c\xea\xb8\x80\xf0\x9f\x8e\xae", "C:\\data\\"});
    require(platform::utf8_arguments(0, nullptr).empty());
    const wchar_t invalid[] = {static_cast<wchar_t>(0xd800), 0};
    const wchar_t* malformed[] = {invalid};
    const wchar_t* missing[] = {nullptr};
    auto rejects = [](int count, const wchar_t* const* values) {
        try { platform::utf8_arguments(count, values); }
        catch (const std::runtime_error&) { return true; }
        return false;
    };
    require(rejects(-1, nullptr));
    require(rejects(1, nullptr));
    require(rejects(1, missing));
    require(rejects(1, malformed));
    std::cout << "Windows wide arguments -> exact UTF-8 bytes passed\n";
}
