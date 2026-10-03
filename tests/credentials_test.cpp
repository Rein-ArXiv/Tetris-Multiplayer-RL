#include "meta/credentials.h"
#include <iostream>
#include <stdexcept>
#include <string>
static void require(bool value) { if (!value) throw std::runtime_error("credential contract"); }
int main() {
    using namespace meta::credentials;
    const std::string token(32, 'a'); // Public deterministic fixture, never a real credential.
    require(account(token));
    require(!account(token + "a") && !account(std::string(32, 'A')));
    require(!account(std::string(31, 'a') + '\0'));
    const auto hash = digest("account", token);
    require(hash.size() == 64 && !account(hash));
    require(hash == "d1f3194d9eb80b8875c4c02a134ac4c78a745c8e1ce4610248d948990fa56785");
    require(hash == digest("account", token));
    require(hash != digest("recovery", token));
    require(digest("account", std::string("x\0y", 3)) != digest("account", "x"));
    require(equal_secret("", ""));
    require(equal_secret(token, token));
    require(!equal_secret(token, token + "a"));
    for (std::size_t i = 0; i < token.size(); ++i) {
        auto different = token; different[i] = 'b';
        require(!equal_secret(token, different));
    }
    require(equal_secret(std::string("a\0b", 3), std::string("a\0b", 3)));
    require(!equal_secret(std::string("a\0b", 3), std::string("a\0c", 3)));
    std::cout << "credential format, domain and equality contracts passed\n";
}
