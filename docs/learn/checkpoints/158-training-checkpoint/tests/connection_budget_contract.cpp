#include "net/connection_budget.h"
#include "net/first_admission.h"
#include <atomic>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
using Budget=study_net::ConnectionBudget<2>;
using Status=Budget::AdmissionStatus;
void require(bool ok) {if(!ok)throw std::runtime_error("connection budget contract");}
int main() {
    static_assert(!std::is_copy_constructible_v<Budget::Lease>);
    static_assert(std::is_nothrow_move_constructible_v<Budget::Lease>);
    // Each zero means denial, including global caps with nonzero source caps.
    Budget zero_sessions({0,2,2,2,10,10});require(!zero_sessions.admit(1).lease);
    Budget zero_handshakes({2,2,0,2,10,10});require(!zero_handshakes.admit(1).lease);
    Budget zero_source({2,0,2,2,10,10});require(!zero_source.admit(1).lease);
    Budget zero_source_hs({2,2,2,0,10,10});require(!zero_source_hs.admit(1).lease);
    Budget zero_bytes({2,2,2,2,0,0});auto z=zero_bytes.admit(1);
    require(z.lease && z.lease.reserve_bytes(0) && !z.lease.reserve_bytes(1));
    {
        Budget b({4,2,3,1,18,10});auto a=b.admit(0);
        require(a.status==Status::admitted && b.admit(0).status==Status::source_handshakes);
        require(a.lease.finish_handshake() && !a.lease.finish_handshake());
        auto c=b.admit(0);require(c.lease && b.admit(0).status==Status::source_sessions);
        auto d=b.admit(1);require(d.lease && b.admit(2).status==Status::key_capacity);
        require(a.lease.reserve_bytes(10) && !a.lease.reserve_bytes(1));
        require(c.lease.reserve_bytes(8) && !c.lease.reserve_bytes(1));
        require(!c.lease.release_bytes(9) && c.lease.release_bytes(3) && d.lease.reserve_bytes(3));
        a.lease=std::move(c.lease);require(!c.lease && a.lease.bytes()==5);
        auto& self=a.lease;a.lease=std::move(self);require(a.lease.bytes()==5);
        auto s=b.snapshot();require(s.sessions==2 && s.handshakes==2 && s.bytes==8 && s.keys==2);
        b.stop_accepting();require(b.admit(2).status==Status::stopped);
        a.lease.reset();a.lease.reset();d.lease.reset();require(b.snapshot().sessions==0 && b.snapshot().bytes==0);
    }
    {
        const auto max=std::numeric_limits<std::size_t>::max();Budget b({3,3,3,3,max,max});
        auto a=b.admit(1),c=b.admit(1);require(a.lease.reserve_bytes(max));
        require(!a.lease.reserve_bytes(1) && !c.lease.reserve_bytes(1));
        require(a.lease.release_bytes(max) && c.lease.reserve_bytes(max));
    }
    {
        Budget b({1,1,1,1,64,64});auto held=b.admit(1);
        auto phase=*study_net::FirstAdmission::create(1000,100);
        require(phase.poll(1099)==study_net::AdmissionState::waiting);
        require(phase.poll(1100)==study_net::AdmissionState::timed_out);
        require(b.snapshot().sessions==1); // A deadline observation does not destroy owners.
        held.lease.reset();require(b.snapshot().sessions==0 && b.snapshot().handshakes==0);
    }
    Budget::Lease survivor;
    {Budget b({1,1,1,1,8,8});survivor=std::move(b.admit(1).lease);require(survivor.reserve_bytes(8));}
    require(survivor.finish_handshake());survivor.reset(); // Last State owner releases mutex before destruction.
    {
        Budget b({3,3,3,3,100,100});std::array<Budget::Lease,24> leases;
        std::array<std::thread,24> threads;std::atomic<bool> go{false};
        for(std::size_t i=0;i<threads.size();++i) threads[i]=std::thread([&,i]{while(!go.load())std::this_thread::yield();leases[i]=std::move(b.admit(9).lease);});
        go=true;for(auto& t:threads)t.join();unsigned winners=0;for(auto& l:leases)winners+=bool(l);
        require(winners==3 && b.snapshot().sessions==3);
        for(std::size_t i=0;i<threads.size();++i)threads[i]=std::thread([&,i]{leases[i].reset();});
        for(auto& t:threads)t.join();
        require(b.snapshot().sessions==0 && b.snapshot().keys==0);
    }
    // Independent record model: derive all totals from live per-connection rows.
    struct Row {bool live=false,hs=false;std::uint64_t key=0;std::size_t bytes=0;};
    std::array<Row,6> rows{};std::array<Budget::Lease,6> leases;Budget b({4,2,3,1,18,10});
    std::uint32_t rng=77;
    auto random=[&]{rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
    for(unsigned n=0;n<12000;++n) {
        auto i=random()%6;auto op=random()%5;auto key=random()%3;auto amount=random()%13;
        std::size_t count=0,hs=0,bytes=0,same=0,same_hs=0,keys=0;std::array<bool,3> present{};
        for(const auto& r:rows)if(r.live){++count;hs+=r.hs;bytes+=r.bytes;present[r.key]=true;if(r.key==key){++same;same_hs+=r.hs;}}
        for(bool p:present)keys+=p;
        auto& row=rows[i];auto& lease=leases[i];
        if(op==0 && !row.live) {
            const bool expect=count<4 && hs<3 && same<2 && same_hs<1 && (present[key] || keys<2);
            auto got=b.admit(key);require(bool(got.lease)==expect);
            if(expect){row={true,true,key,0};lease=std::move(got.lease);}
        } else if(op==1){lease.reset();row={};}
        else if(op==2){require(lease.finish_handshake()==(row.live && row.hs));row.hs=false;}
        else if(op==3){bool expect=row.live && amount<=10-row.bytes && amount<=18-bytes;require(lease.reserve_bytes(amount)==expect);if(expect)row.bytes+=amount;}
        else if(op==4){bool expect=row.live && amount<=row.bytes;require(lease.release_bytes(amount)==expect);if(expect)row.bytes-=amount;}
        count=hs=bytes=keys=0;present={};for(const auto& r:rows)if(r.live){++count;hs+=r.hs;bytes+=r.bytes;present[r.key]=true;}for(bool p:present)keys+=p;
        const auto snap=b.snapshot();require(snap.sessions==count && snap.handshakes==hs && snap.bytes==bytes && snap.keys==keys);
    }
    std::puts("budget: zero/exact/overflow/move/lifetime/24-thread caps and 12000 independent-model steps");
}
