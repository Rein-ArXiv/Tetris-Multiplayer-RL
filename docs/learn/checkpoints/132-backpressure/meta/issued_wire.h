#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "meta/account_crypto.h"
#include "meta/json_fields.h"

namespace study_meta {

// Parses the issued-credential response shape only:
//   { "player_id": <uint64>, "token": <32-char lowercase hex> }
// Rejects missing, wrong-typed, or extra fields through the json_fields helpers.
inline std::optional<IssuedAccount> parse_issued(const std::string& body) {
    const auto root = object(body, 2);
    if (!root) {
        return std::nullopt;
    }

    const auto player_id = number(*root, "player_id");
    if (!player_id || *player_id == 0) {
        return std::nullopt;
    }

    const auto token = text(*root, "token");
    if (!token || !credential_hex(*token, 32)) {
        return std::nullopt;
    }

    return IssuedAccount{*player_id, *token};
}

}  // namespace study_meta
