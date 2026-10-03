// Scheduling hook observes the starter's false wait predicate under its mutex.
#include "room.h"
#include "relay.h"
#include "log.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <thread>
namespace relay { void startPump(Match,meta::client::MetaClient*) {} }
std::mutex event_mu;std::condition_variable event_cv;bool waiting=false,done=false;
void room_wait_observed() {std::lock_guard<std::mutex> lock(event_mu);waiting=true;event_cv.notify_all();}
struct Transport final : net::StreamTransport {
    bool eof=false;std::atomic<bool> closed{false};
    bool alive() const override {return !closed.load();}
    bool send(const void*,size_t) override {return !closed.load();}
    bool receive(std::vector<uint8_t>&) override {return !eof && !closed.load();}
    void close() override {closed=true;}
};
int main() {
    relay::set_log_level(relay::LogLevel::Warn);
    relay::RoomRegistry registry;
    auto host=std::make_shared<Transport>(),guest=std::make_shared<Transport>();guest->eof=true;
    net::TcpSocket a,b;a.transport=host;b.transport=guest;
    registry.study_seed(a,b,true);
    std::thread starter([&]{registry.study_run(true,a);std::lock_guard<std::mutex> lock(event_mu);done=true;event_cv.notify_all();});
    bool reached=false;
    {std::unique_lock<std::mutex> lock(event_mu);reached=event_cv.wait_for(lock,std::chrono::seconds(2),[]{return waiting;});}
    if (reached) registry.study_run(false,b);
    bool woke=false;
    {std::unique_lock<std::mutex> lock(event_mu);woke=event_cv.wait_for(lock,std::chrono::milliseconds(300),[]{return done;});}
    registry.shutdown();starter.join();
    if (!reached || !woke) {std::fprintf(stderr,"room exit wakeup: reached=%d woke_without_shutdown=%d\n",reached,woke);return 1;}
    std::puts("room exit wakes starter after peer presence changes, without shutdown assistance");
}
