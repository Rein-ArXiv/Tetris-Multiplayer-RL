#include "meta/credential_lab.h"
#include "meta/account_crypto.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace study_credentials;
static void require(bool value) { if (!value) throw std::runtime_error("credential lab contract"); }
template<class Error, class F> static void throws(F fn) {
    try { fn(); } catch (const Error&) { return; }
    throw std::runtime_error("expected failure was accepted");
}
static int calls = 0;
static int known_bytes(unsigned char* out, int size) {
    ++calls;
    for (int i = 0; i < size; ++i) out[i] = static_cast<unsigned char>(i);
    return 1;
}
static int failed_bytes(unsigned char* out, int size) {
    ++calls;
    if (size > 0) out[0] = 0xab; // Partial output still fails.
    return 0;
}
static int unsupported_bytes(unsigned char*, int) { ++calls; return -1; }
static int unexpected_bytes(unsigned char*, int) { ++calls; return 2; }
int main() {
    const auto known = generate_account(known_bytes);
    require(known == "000102030405060708090a0b0c0d0e0f");
    require(calls == 1);
    throws<std::runtime_error>([]{ generate_account(failed_bytes); });
    throws<std::runtime_error>([]{ generate_account(unsupported_bytes); });
    throws<std::runtime_error>([]{ generate_account(unexpected_bytes); });
    require(calls == 4);
    throws<std::invalid_argument>([]{ generate_account(nullptr); });
    const auto generated = generate_account();
    require(valid_account(generated)); // Wiring/format only, not a randomness proof.
    const std::string token(account_bytes * hex_per_byte, 'a');
    require(!valid_account(std::string(token.size(), 'A')));
    require(!valid_account(token + "0"));
    auto nul = token; nul.back() = '\0'; require(!valid_account(nul));
    const auto hash = account_digest(token);
    require(hash == "2f22d4e3ef3f3c3cd2ff7e908ce56e5f4e4a714f53ba32cab0ed8ec22cee43b2");
    require(hash == study_meta::account_hash(token));
    require(hash != digest(Purpose::recovery, token));
    require(!valid_account(hash));
    require(digest(Purpose::account, std::string_view("a\0b", 3)) != digest(Purpose::account, "a"));
    throws<std::invalid_argument>([]{ digest(static_cast<Purpose>(999), "sample"); });
    throws<std::invalid_argument>([&]{ account_digest(nul); });
    throws<std::invalid_argument>([]{ hex_encode(nullptr, 1); });
    throws<std::length_error>([]{ hex_encode(nullptr, std::numeric_limits<std::size_t>::max()); });
    require(hex_encode(nullptr, 0).empty());
    CredentialIndex index;
    require(index.insert(token, 7));
    require(!index.insert(token, 99));
    require(index.size() == 1 && index.find(token) == 7);
    require(!index.find(hash) && !index.find(nul));
    auto unknown = token; unknown.front() = 'b'; require(!index.find(unknown));
    require(!index.find("rc1." + hash));
    throws<std::invalid_argument>([&]{ index.insert(hash, 8); });
    require(index.size() == 1 && index.find(token) == 7);
    require(equal_secret(token, token) && equal_secret({}, {}));
    require(!equal_secret(token, token + "a"));
    for (std::size_t i = 0; i < token.size(); ++i) {
        auto different = token; different[i] = 'b';
        require(!equal_secret(token, different));
    }
    require(!equal_secret(std::string_view("a\0b", 3), std::string_view("a\0c", 3)));
    std::cout << "generation failure, encoding, domain, lookup and equality contracts passed\n";
}
