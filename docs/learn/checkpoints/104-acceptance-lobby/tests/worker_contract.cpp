#include "net/worker_group.h"
using Group=study_net::WorkerGroup;
#define GROUP(name,limit) Group name{limit}
#define STOP(g) (g).stop_accepting()
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
struct Gate {
 std::mutex mutex;std::condition_variable cv;bool entered=false,released=false,done=false;
 void enter_and_wait(){std::unique_lock<std::mutex> l(mutex);entered=true;cv.notify_all();cv.wait(l,[&]{return released;});}
 void wait_entered(){std::unique_lock<std::mutex> l(mutex);cv.wait(l,[&]{return entered;});}
 void release(){std::lock_guard<std::mutex> l(mutex);released=true;cv.notify_all();}
 void finish(){std::lock_guard<std::mutex> l(mutex);done=true;cv.notify_all();}
 void wait_done(){std::unique_lock<std::mutex> l(mutex);cv.wait(l,[&]{return done;});}
};
struct Cleanup {
 Gate* gate;
 ~Cleanup(){gate->enter_and_wait();gate->finish();}
};
struct ThrowMove {
 ThrowMove()=default;
 ThrowMove(const ThrowMove&)=delete;
 ThrowMove(ThrowMove&&){throw std::runtime_error("expected task construction");}
 void operator()(){}
};
int main(){
 Gate cleanup;GROUP(group,1);
 auto owned=std::make_unique<Cleanup>();owned->gate=&cleanup;
 CHECK(group.launch([owned=std::move(owned)]{}));
 cleanup.wait_entered();
 // No sleep/timing inference: the last capture destructor is blocked now.
 const bool admitted_while_cleaning=group.launch([]{});
 cleanup.release();cleanup.wait_done();group.wait();
 CHECK(!admitted_while_cleaning);
 ThrowMove bad;CHECK(!group.launch(std::move(bad)));
 CHECK(group.launch([]{throw 17;}));group.wait();
 CHECK(group.launch([]{throw std::runtime_error("expected body");}));group.wait();
 CHECK(group.failed_tasks()==2&&group.active()==0);
 std::atomic<unsigned> ran{0};
 CHECK(group.launch([p=std::make_unique<unsigned>(7),&ran]{ran.store(*p);}));group.wait();CHECK(ran.load()==7);
 STOP(group);CHECK(!group.launch([]{}));group.wait();
 GROUP(zero,0);CHECK(!zero.launch([]{}));
 Gate jobs;GROUP(bounded,2);std::atomic<unsigned> accepted{0};std::vector<std::thread> producers;
 for(unsigned i=0;i<12;++i)producers.emplace_back([&]{if(bounded.launch([&]{jobs.enter_and_wait();}))++accepted;});
 for(auto& t:producers)t.join();
 CHECK(accepted==2);STOP(bounded);CHECK(!bounded.launch([]{}));jobs.release();bounded.wait();
 std::puts("worker lifetime: capture destruction precedes slot release; construction rollback, body exceptions, move-only work, concurrent cap and closed admission");
}
