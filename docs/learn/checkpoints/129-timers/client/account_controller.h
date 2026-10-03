#pragma once
#include "client/account_view.h"
#include "meta/account_outcome.h"
#include "client/async_job.h"
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
namespace study_account_ui {
// Main thread alone calls request/poll/view. Worker must return AccountOutcome.
// One fixed server/folder per controller; visibility does not change identity.
template<class Worker>
class Controller final {
public:
    explicit Controller(std::unique_ptr<Worker> worker) : job_(std::move(worker)) {}
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    const View& view() const { return view_; }
    bool busy() const { return job_.valid(); }
    bool request() {
        if (busy() || !may_connect(view_)) return false;
        try {
            if (!job_.start()) return false;
            view_.status = Status::busy;
            view_.profile.reset();
            return true;
        } catch (const std::exception&) {
            view_.status = view_.has_unsaved_key ? Status::unsaved : Status::failed;
            view_.profile.reset();
            return false;
        }
    }
    bool poll() {
        if (!job_.ready())
            return false;
        try {
            const auto outcome = job_.take(); // consumes exactly once; publishes worker writes
            view_ = outcome.result ? present(*outcome.result, outcome.has_unsaved_key)
                                   : View{outcome.has_unsaved_key ? Status::unsaved : Status::failed,
                                          outcome.has_unsaved_key, {}};
        } catch (const std::exception&) {
            view_.status = view_.has_unsaved_key ? Status::unsaved : Status::failed;
            view_.profile.reset();
        }
        return true;
    }
private:
    View view_;
    study_jobs::Job<Worker> job_; // owns worker and completion together
};
} // namespace study_account_ui
