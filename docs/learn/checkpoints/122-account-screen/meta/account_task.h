#pragma once
#include "meta/account_bootstrap.h"
#include <utility>
#include <memory>
namespace study_meta {
struct AccountOutcome {
    std::optional<BootstrapResult> result;
    bool has_unsaved_key = false;
};
// Confined to one job at a time. Session identity and any unsaved key survive jobs.
class AccountTask final {
public:
    AccountTask(int port, std::filesystem::path folder)
        : api_(port), folder_(std::move(folder)) {}
    AccountOutcome operator()() {
        try {
            // Acquire on the worker, then retain the lock between calls as well.
            if (!store_ || !store_->available())
                store_ = std::make_unique<LocalAccountStore>(folder_, api_.origin());
            const auto result = session_.connect(*store_, api_);
            return {result, session_.has_unsaved_key()};
        } catch (const std::exception&) {
            return {{}, session_.has_unsaved_key()};
        }
    }
private:
    AccountHttp api_;
    std::filesystem::path folder_;
    AccountBootstrap session_;
    std::unique_ptr<LocalAccountStore> store_;
};
} // namespace study_meta
