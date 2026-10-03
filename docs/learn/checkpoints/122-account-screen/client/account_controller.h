#pragma once
#include "client/account_view.h"
#include "meta/account_task.h"
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
    explicit Controller(std::unique_ptr<Worker> worker) : worker_(std::move(worker)) {
        if (!worker_) throw std::invalid_argument("account worker required");
    }
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    const View& view() const { return view_; }
    bool busy() const { return job_.valid(); }
    bool request() {
        if (busy() || !may_connect(view_)) return false;
        try {
            auto* worker = worker_.get();
            job_ = std::async(std::launch::async, [worker] { return (*worker)(); });
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
        if (!busy() || job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return false;
        try {
            const auto outcome = job_.get(); // consumes exactly once; publishes worker writes
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
    std::unique_ptr<Worker> worker_;
    View view_;
    // Destroyed FIRST: async future joins before worker_ and its secret session die.
    // Destruction can block; the frame's poll uses only a zero-duration wait.
    std::future<study_meta::AccountOutcome> job_;
};
} // namespace study_account_ui
