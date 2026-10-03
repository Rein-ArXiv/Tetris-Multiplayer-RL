#include "net/hash_mailbox.h"
#include <atomic>
#include <thread>
#include <cstdio>
int main(){
    // Every access is atomic, yet this legal interleaving mixes two publications.
    std::atomic<unsigned> tick{4};
    std::atomic<unsigned long long> hash{111};
    std::atomic<unsigned> phase{0};
    std::thread writer([&]{
        tick.store(8);
        phase.store(1);
        while(phase.load()!=2)std::this_thread::yield();
        hash.store(222);
    });
    while(phase.load()!=1)std::this_thread::yield();
    const auto observed_tick=tick.load();
    const auto observed_hash=hash.load();
    phase.store(2);
    writer.join();
    std::printf("split atomics: tick=%u hash=%llu (mixed publication)\n",observed_tick,observed_hash);
    if(observed_tick!=8||observed_hash!=111)return 1;

    study_net::HashMailbox<4,2> box;
    if(box.record_remote(4,111)!=study_net::HashPut::stored||
       box.record_remote(8,222)!=study_net::HashPut::stored)return 1;
    study_net::HashSample latest;
    if(!box.latest_remote(latest))return 1;
    std::printf("latest arrival: tick=%u hash=%llu\n",latest.tick,static_cast<unsigned long long>(latest.hash));
    if(box.record_local(4,111)!=study_net::HashPut::stored||
       box.record_local(8,222)!=study_net::HashPut::stored)return 1;
    study_net::HashComparison pair;
    unsigned count=0;
    while(box.poll(pair)){
        if(pair.local!=pair.remote)return 1;
        std::printf("consume pair: tick=%u\n",pair.tick);
        ++count;
    }
    return count==2?0:1;
}
