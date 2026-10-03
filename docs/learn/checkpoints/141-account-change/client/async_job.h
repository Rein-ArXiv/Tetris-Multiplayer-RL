#pragma once
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace study_jobs {
// One UI thread owns this object; only the job calls Worker. The destination
// is fixed by Worker construction. Worker must not borrow short-lived UI data.
template<class Worker>
class Job final {
public:
    using Result=std::invoke_result_t<Worker&>;
    explicit Job(std::unique_ptr<Worker> worker):worker_(std::move(worker)) {
        if(!worker_)throw std::invalid_argument("worker required");
    }
    Job(const Job&)=delete;
    Job& operator=(const Job&)=delete;
    bool valid()const {return future_.valid();}
    bool ready()const {
        return valid() && future_.wait_for(std::chrono::seconds(0))==std::future_status::ready;
    }
    bool start() {
        if(valid())return false;
        auto* worker=worker_.get();
        future_=std::async(std::launch::async,[worker]{return (*worker)();});
        return true;
    }
    Result take() {
        if(!ready())throw std::logic_error("job is not ready");
        return future_.get();
    }
private:
    std::unique_ptr<Worker> worker_;
    // Reverse destruction waits for the async job before destroying its Worker.
    std::future<Result> future_;
};
} // namespace study_jobs
