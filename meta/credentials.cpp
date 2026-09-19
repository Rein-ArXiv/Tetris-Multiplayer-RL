#include "credentials.h"
#include <openssl/evp.h>
#include <stdexcept>

namespace meta::credentials {
std::string digest(const std::string &purpose, const std::string &value) {
    const auto input = "tetris-" + purpose + "-v1:" + value;
    unsigned char bytes[EVP_MAX_MD_SIZE];
    unsigned length = 0;
    if (EVP_Digest(input.data(), input.size(), bytes, &length, EVP_sha256(), nullptr) != 1 || length != 32)
        throw std::runtime_error("credential digest unavailable");
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (unsigned i = 0; i < length; ++i) {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 15];
    }
    return result;
}
} // namespace meta::credentials
