#pragma once
#include "meta/journal_flow.h"
#include "meta/account_change.h"
#include "meta/issued_wire.h"
namespace study_meta {
inline bool valid_pending(const PendingChange& p) {
    if (p.origin.empty() || !p.expected_player_id ||
        !study_credentials::valid_account(p.next_token) || !valid_recovery(p.next_recovery)) return false;
    if (p.operation == "recover") return valid_recovery(p.credential) && p.credential != p.next_recovery;
    if (!study_credentials::valid_account(p.credential)) return false;
    if (p.operation == "backup") return p.credential == p.next_token;
    if (p.operation == "rotate") return p.credential != p.next_token;
    return false;
}
inline std::optional<PendingChange> parse_pending(const std::string& body, const std::string& origin) {
    const auto j = object(body, 6);
    if (!j) return {};
    const auto saved = text(*j,"origin"), op = text(*j,"operation"), proof = text(*j,"credential");
    const auto token = text(*j,"next_token"), code = text(*j,"next_recovery");
    const auto id = number(*j,"expected_player_id");
    if (!saved || !op || !proof || !token || !code || !id || *saved != origin) return {};
    PendingChange p{*saved,*op,*proof,*token,*code,*id};
    return valid_pending(p) ? std::optional<PendingChange>{p} : std::nullopt;
}
inline bool idle_journal(const std::string& body, const std::string& origin) {
    const auto j=object(body,2);
    return j && text(*j,"origin")==std::optional<std::string>{origin} &&
        text(*j,"state")==std::optional<std::string>{"idle"};
}
inline std::string pending_json(const PendingChange& p) {
    return Json{{"origin",p.origin},{"operation",p.operation},{"credential",p.credential},
        {"next_token",p.next_token},{"next_recovery",p.next_recovery},
        {"expected_player_id",p.expected_player_id}}.dump()+"\n";
}
} // namespace study_meta
