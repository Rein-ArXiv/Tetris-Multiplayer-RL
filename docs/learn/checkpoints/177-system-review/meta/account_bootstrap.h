#pragma once
#include "meta/bootstrap_types.h"
#include "meta/local_account_store.h"
#include "meta/account_http.h"
namespace study_meta {
// One session keeps a newly issued key until its private file is confirmed saved.
// Store/API are ports; tests inject failures without disk/network side effects.
class AccountBootstrap {
public:
    AccountBootstrap() = default;
    AccountBootstrap(const AccountBootstrap&) = delete;
    AccountBootstrap& operator=(const AccountBootstrap&) = delete;
    bool has_unsaved_key() const { return pending_.has_value(); }
    template<class Store, class Api>
    BootstrapResult connect(Store& store, Api& api) {
        if (!store.available() || !api.valid() || store.origin() != api.origin())
            return {BootstrapState::storage_blocked, {}};
        if (!origin_.empty() && origin_ != api.origin())
            return {BootstrapState::storage_blocked, {}};
        origin_ = api.origin();
        // A save retry must never create a second account.
        if (pending_) return save_pending(store, api);
        const auto saved = store.load();
        if (saved.status == AccountLoad::ready) return authenticate(api, saved.account);
        if (saved.status != AccountLoad::missing) return {BootstrapState::storage_blocked, {}};
        if (creation_attempted_) return {BootstrapState::creation_unconfirmed, {}};
        creation_attempted_ = true;
        pending_ = api.issue();
        if (!pending_) return {BootstrapState::creation_unconfirmed, {}};
        return save_pending(store, api);
    }
private:
    template<class Store, class Api>
    BootstrapResult save_pending(Store& store, Api& api) {
        if (!store.save(*pending_)) return {BootstrapState::unsaved, {}};
        const auto saved = *pending_;
        pending_.reset();
        return authenticate(api, saved);
    }
    template<class Api>
    BootstrapResult authenticate(Api& api, const IssuedAccount& account) {
        const auto result = api.verify(account.token);
        if (result.state == VerifyState::rejected) return {BootstrapState::key_rejected, {}};
        if (result.state != VerifyState::accepted || !result.profile ||
            result.profile->player_id != account.player_id)
            return {BootstrapState::offline_saved, {}};
        return {BootstrapState::online, result.profile};
    }
    std::string origin_; // binds any in-memory issued key to its service
    std::optional<IssuedAccount> pending_;
    bool creation_attempted_ = false;
};
} // namespace study_meta
