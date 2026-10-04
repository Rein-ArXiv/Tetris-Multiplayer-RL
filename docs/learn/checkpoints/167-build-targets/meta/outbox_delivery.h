#pragma once
#include "meta/local_outbox.h"
#include "meta/retry_submission.h"
namespace study_meta {
enum class DeliveryState { empty, storage_blocked, unconfirmed, stopped, confirmed };
struct DeliveryResult {
    DeliveryState state = DeliveryState::storage_blocked;
    bool local_saved = false;
    std::optional<study_net::Receipt> receipt;
};
// Store must outlive this session and retain its lock. One owner, one operation.
// A manual deliver call has its own bounded automatic retry budget.
template<class Store>
class OutboxDelivery {
public:
    explicit OutboxDelivery(Store& store) : store_(store) {}
    OutboxDelivery(const OutboxDelivery&) = delete;
    OutboxDelivery& operator=(const OutboxDelivery&) = delete;
    template<class Api, class Now, class Wait>
    DeliveryResult deliver(Api& api, Now now, Wait wait, std::chrono::milliseconds budget) {
        if (!document_) {
            const auto loaded = store_.load();
            if (loaded.state == OutboxLoad::missing) return {DeliveryState::empty, false, {}};
            if (loaded.state != OutboxLoad::ready || !loaded.document) return {};
            document_ = loaded.document;
            dirty_ = true; // Visible after a failed prior save is not yet a durability confirmation.
        }
        if (document_->status != OutboxStatus::pending) return finish();
        if (!api.valid() || api.origin() != document_->origin) return {};
        if (dirty_ && !flush()) return {}; // No HTTP before the durable request barrier.
        study_net::MatchSubmission submission(3);
        if (!submission.prepare(document_->request)) return {};
        const auto result = retry_submission(submission, api, now, wait, budget);
        if (result == RetryExit::confirmed) {
            document_->status = OutboxStatus::confirmed;
            document_->receipt = *submission.receipt();
        } else if (result == RetryExit::stopped) {
            document_->status = OutboxStatus::stopped;
        } else {
            return {DeliveryState::unconfirmed, true, {}};
        }
        dirty_ = true;
        return finish();
    }
private:
    bool flush() {
        if (!dirty_) return true;
        if (!store_.save(*document_)) return false;
        dirty_ = false;
        return true;
    }
    DeliveryResult finish() {
        const bool saved = flush();
        const auto state = document_->status == OutboxStatus::confirmed
            ? DeliveryState::confirmed : DeliveryState::stopped;
        return {state, saved, document_->receipt};
    }
    Store& store_;
    std::optional<OutboxDocument> document_;
    bool dirty_ = false;
};
} // namespace study_meta
