#pragma once
#include "meta/http_retry_policy.h"
#include "net/match_submission.h"
#include <algorithm>
#include <chrono>
#include <type_traits>
namespace study_meta {
enum class RetryExit { no_request, confirmed, stopped, budget_exhausted };

// now() is monotonic; wait(ms) returns when its wait completes. Neither callback
// may re-enter submission. Attempts and immutable data remain owned by it.
template<class Sender, class Now, class Wait>
RetryExit retry_submission(study_net::MatchSubmission& submission, Sender&& sender,
                           Now&& now, Wait&& wait, std::chrono::milliseconds budget) {
    static_assert(std::is_nothrow_invocable_r_v<study_net::Reply, Sender&,
                  const study_net::MatchRecord&, std::chrono::milliseconds>,
                  "Sender must return Reply without throwing");
    using State = study_net::MatchSubmission::SubmissionState;
    using Step = study_net::MatchSubmission::SubmitStep;
    using Millis = std::chrono::milliseconds;
    const auto start = now();
    for (;;) {
        if (submission.state() == State::empty) return RetryExit::no_request;
        if (submission.state() == State::confirmed) return RetryExit::confirmed;
        if (submission.state() == State::stopped) return RetryExit::stopped;
        if (submission.attempts() >= submission.max_attempts())
            return RetryExit::budget_exhausted;
        if (budget.count() <= 0) return RetryExit::budget_exhausted;
        auto left = budget - std::chrono::duration_cast<Millis>(now() - start);
        if (left.count() <= 0) return RetryExit::budget_exhausted;
        if (submission.attempts() != 0) {
            const auto delay = std::max(retry_backoff(submission.attempts()),
                                        submission.retry_after());
            // Do not shorten the server's minimum wait to fit our budget.
            if (delay >= left) return RetryExit::budget_exhausted;
            wait(delay);
            left = budget - std::chrono::duration_cast<Millis>(now() - start);
            if (left.count() <= 0) return RetryExit::budget_exhausted;
        }
        const auto phase_limit = std::min(left, Millis{2000});
        const auto step = submission.submit([&](const study_net::MatchRecord& r) noexcept {
            return sender(r, phase_limit);
        });
        if (step == Step::confirmed) return RetryExit::confirmed;
        if (step == Step::stopped) return RetryExit::stopped;
        // A started I/O call can finish after budget; its valid receipt still counts.
    }
}
} // namespace study_meta
