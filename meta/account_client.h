#pragma once
#include "http_client.h"
#include "account_store.h"

namespace meta::client {
struct AccountOperation {
    bool ok = false;
    bool pending = false;
    std::string message;
    std::string token;
    std::optional<AuthInfo> player;
    bool unsaved = false; // Keep this key in memory for a local save retry; do not spend/earn online.
};
// Startup and UI share the same account workflow. All disk mutations hold the
// profile lock. Call from one worker; presentation only consumes the result.
AccountOperation bootstrap_account(MetaClient &api, bool create_separate = false);
AccountOperation save_created_account(MetaClient &api, const std::string &token);
AccountOperation import_legacy_account(MetaClient &api);
AccountOperation change_saved_account(MetaClient &api, const std::string &operation);
AccountOperation resume_account_change(MetaClient &api);
} // namespace meta::client
