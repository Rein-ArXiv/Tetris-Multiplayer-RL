#ifndef STUDY_CREDENTIALS_CREDENTIAL_LAB_H
#define STUDY_CREDENTIALS_CREDENTIAL_LAB_H

// Teaching lab for credential representation. Local lab only:
// no network, no database, and no printing secrets. Not production.

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <limits>
#include <utility>

namespace study_credentials {

// The account wire format matches the cumulative account service.
inline constexpr std::size_t account_bytes = 16;
inline constexpr std::size_t hex_per_byte = 2;
inline constexpr std::size_t sha256_bytes = 32;

enum class Purpose { account, recovery, operation };

// Domain-separation prefix per purpose. Throws on an invalid enum value.
inline std::string_view prefix(Purpose purpose) {
    switch (purpose) {
    case Purpose::account:
        return "study-account-v1:";
    case Purpose::recovery:
        return "study-recovery-v1:";
    case Purpose::operation:
        return "study-operation-v1:";
    }
    throw std::invalid_argument("study_credentials: invalid Purpose");
}

inline char hex_digit(unsigned value) {
    return static_cast<char>(value < 10 ? ('0' + value) : ('a' + (value - 10)));
}

// Hex encoding of unsigned bytes, lower-case.
inline std::string hex_encode(const unsigned char* data, std::size_t size) {
    std::string out;
    if (size > out.max_size() / hex_per_byte)
        throw std::length_error("hex output too large");
    if (size != 0 && data == nullptr)
        throw std::invalid_argument("null hex input");
    out.resize(size * hex_per_byte);
    for (std::size_t i = 0; i < size; ++i) {
        out[2 * i] = hex_digit((static_cast<unsigned>(data[i]) >> 4) & 0x0Fu);
        out[2 * i + 1] = hex_digit(static_cast<unsigned>(data[i]) & 0x0Fu);
    }
    return out;
}

inline bool is_lower_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

// Exactly account_bytes*hex_per_byte lower-case hexadecimal characters.
inline bool valid_account(std::string_view value) {
    if (value.size() != account_bytes * hex_per_byte) return false;
    for (char c : value) {
        if (!is_lower_hex(c)) return false;
    }
    return true;
}

// Injectable RNG, for local deterministic testing only.
using RandomBytes = int (*)(unsigned char*, int);

// Obtains a fixed array. A null callback is rejected. Success is ONLY when the
// callback returns 1; any other result throws. No fallback entropy source.
inline std::string generate_account(RandomBytes random = RAND_bytes) {
    if (random == nullptr) {
        throw std::invalid_argument("study_credentials: null RandomBytes");
    }
    std::array<unsigned char, account_bytes> bytes{};
    static_assert(account_bytes <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    const int rc = random(bytes.data(), static_cast<int>(bytes.size()));
    if (rc != 1) {
        throw std::runtime_error("study_credentials: RNG failure");
    }
    return hex_encode(bytes.data(), bytes.size());
}

// Hashes the full explicit-length bytes prefix+value, including embedded NUL
// bytes. EVP success and the output size are both checked. Returns hex.
inline std::string digest(Purpose purpose, std::string_view value) {
    std::string input(prefix(purpose));
    if (!value.empty()) input.append(value.data(), value.size());
    std::array<unsigned char, EVP_MAX_MD_SIZE> out{};
    unsigned int out_len = 0;
    if (EVP_Digest(input.data(), input.size(), out.data(), &out_len,
                   EVP_sha256(), nullptr) != 1 || out_len != sha256_bytes) {
        throw std::runtime_error("study_credentials: digest failure");
    }
    return hex_encode(out.data(), out_len);
}

// Validates account format, then digests with the account purpose.
inline std::string account_digest(std::string_view token) {
    if (!valid_account(token)) {
        throw std::invalid_argument("study_credentials: invalid account format");
    }
    return digest(Purpose::account, token);
}

// Equality, not ordering. The length check makes length observable; unequal
// lengths return false before any constant-time comparison.
inline bool equal_secret(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    if (a.empty()) return true;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

// Single-threaded, unbounded lab index. NOT a production credential store.
// Members retain digest keys and IDs. The caller still owns its token.
class CredentialIndex {
public:
    // Validates and computes account_digest before try_emplace; an existing
    // digest is never replaced on conflict.
    bool insert(std::string_view token, std::uint64_t id) {
        std::string key = account_digest(token);
        return map_.try_emplace(std::move(key), id).second;
    }

    // Invalid format returns nullopt; otherwise computes the digest and looks up.
    std::optional<std::uint64_t> find(std::string_view token) const {
        if (!valid_account(token)) return std::nullopt;
        std::string key = account_digest(token);
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        return it->second;
    }

    std::size_t size() const { return map_.size(); }

private:
    std::unordered_map<std::string, std::uint64_t> map_;
};

}  // namespace study_credentials

#endif  // STUDY_CREDENTIALS_CREDENTIAL_LAB_H
