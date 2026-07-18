#include "../server/worker_group.h"

#include <condition_variable>
#include <mutex>
#include <stdexcept>

int main()
{
    relay::WorkerGroup workers{"worker-group-test", 1};
    std::mutex mu;
    std::condition_variable cv;
    bool started = false;
    bool release = false;

    if (!workers.launch([&] {
            std::unique_lock<std::mutex> lk(mu);
            started = true;
            cv.notify_all();
            cv.wait(lk, [&] { return release; });
        })) {
        return 1;
    }

    {
        std::unique_lock<std::mutex> lk(mu);
        cv.wait(lk, [&] { return started; });
    }
    if (workers.launch([] {})) return 2;

    {
        std::lock_guard<std::mutex> lk(mu);
        release = true;
    }
    cv.notify_all();
    workers.wait();

    if (!workers.launch([] { throw std::runtime_error("expected"); })) return 3;
    workers.wait();

    workers.stopAccepting();
    if (workers.launch([] {})) return 4;
    return 0;
}
