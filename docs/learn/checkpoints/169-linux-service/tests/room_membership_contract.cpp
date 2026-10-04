#include "net/room_membership.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <thread>
using namespace study_net;
void require(bool ok) {if(!ok)throw std::runtime_error("room membership contract");}
struct Peer { std::function<void()> destroy; ~Peer(){if(destroy)destroy();} };
using Room=RoomMembership<Peer>;
std::shared_ptr<Peer> peer() {return std::make_shared<Peer>();}
int main() {
    static_assert(!std::is_copy_constructible_v<Room>);
    static_assert(!std::is_move_constructible_v<Room>);
    {
        auto host=peer(),guest=peer();Room room(7,host);
        auto joined=room.join_guest(guest);require(joined.status==RoomJoinStatus::joined);
        const auto old=*joined.ticket;auto old_notice=*room.snapshot_to(0);
        auto left=room.leave(old);require(left.status==RoomLeaveStatus::removed && left.departed==guest && !left.empty);
        auto replacement=room.join_guest(guest);require(replacement.status==RoomJoinStatus::joined && *replacement.ticket!=old);
        require(room.leave(old).status==RoomLeaveStatus::stale && room.size()==2);
        unsigned sent=0;
        auto sender=[&](const std::shared_ptr<Peer>&,std::size_t) noexcept {++sent;return true;};
        require(!room.deliver(old_notice,sender) && !room.deliver(*left.notice,sender) && sent==0);
        auto fresh=*room.snapshot_to(0);require(room.deliver(fresh,sender) && sent==1);
        require(!room.deliver(fresh,[](const std::shared_ptr<Peer>&,std::size_t) noexcept {return false;}));
        require(room.size()==2); // Send failure is reported, not silently promoted to success.
        auto forged=fresh;forged.present=1;require(!room.deliver(forged,sender));
        auto drained=room.close();require(drained[0]==host && drained[1]==guest && room.closed());
        require(!room.deliver(fresh,sender) && !room.close()[0]);
    }
    for(unsigned n=0;n<128;++n) {
        Room room(100+n,peer());auto guest=room.join_guest(peer());
        const auto ticket=*guest.ticket;
        std::array<Room::LeaveResult,2> results;
        std::atomic<bool> go{false};
        auto leave=[&](unsigned i){while(!go.load())std::this_thread::yield();results[i]=room.leave(ticket);};
        std::thread a(leave,0),b(leave,1);go=true;a.join();b.join();
        require((results[0].status==RoomLeaveStatus::removed)+(results[1].status==RoomLeaveStatus::removed)==1);
        require(room.size()==1);room.close();
    }
    for(unsigned n=0;n<128;++n) {
        Room room(400+n,peer());const auto host=*room.host_ticket();const auto guest=*room.join_guest(peer()).ticket;
        std::array<Room::LeaveResult,2> results;std::atomic<bool> go{false};
        std::thread a([&]{while(!go.load())std::this_thread::yield();results[0]=room.leave(host);});
        std::thread b([&]{while(!go.load())std::this_thread::yield();results[1]=room.leave(guest);});
        go=true;a.join();b.join();
        require(results[0].status==RoomLeaveStatus::removed && results[1].status==RoomLeaveStatus::removed);
        require(results[0].empty!=results[1].empty && room.size()==0 && room.closed());
        unsigned calls=0;
        for(const auto& r:results)if(r.notice)require(!room.deliver(*r.notice,[&](const std::shared_ptr<Peer>&,std::size_t) noexcept {++calls;return true;}));
        require(calls==0);
    }
    {
        // The callback holds the send gate, but leave can still acquire state.
        Room room(900,peer());auto guest=room.join_guest(peer());const auto notice=*room.snapshot_to(0);
        std::mutex mu;std::condition_variable cv;bool entered=false,release=false;
        std::atomic<int> phase{0};bool delivered=false;Room::JoinResult rejoined;
        std::thread sender([&]{delivered=room.deliver(notice,[&](const std::shared_ptr<Peer>&,std::size_t count) noexcept {
            if(count!=2)std::terminate();
            std::unique_lock<std::mutex> lock(mu);entered=true;phase=1;cv.notify_all();
            cv.wait(lock,[&]{return release;});phase=2;return true;
        });});
        {std::unique_lock<std::mutex> lock(mu);cv.wait(lock,[&]{return entered;});}
        auto left=room.leave(*guest.ticket);require(left.status==RoomLeaveStatus::removed && room.size()==1);
        std::thread joiner([&]{rejoined=room.join_guest(peer());if(phase.load()!=2)std::terminate();});
        {std::lock_guard<std::mutex> lock(mu);release=true;cv.notify_all();}
        sender.join();joiner.join();require(delivered && rejoined.status==RoomJoinStatus::joined);
        require(!room.deliver(*left.notice,[](const std::shared_ptr<Peer>&,std::size_t) noexcept {return true;}));
        room.close();
    }
    {
        // Destruction can query the still-live room, proving it is outside state_.
        Room room(901,peer());auto guest=peer();bool destroyed=false;
        guest->destroy=[&]{require(room.size()==1);destroyed=true;};
        auto ticket=room.join_guest(guest);std::weak_ptr<Peer> weak=guest;guest.reset();
        auto left=room.leave(*ticket.ticket);require(!weak.expired() && !destroyed);
        left.departed.reset();require(weak.expired() && destroyed);room.close();
    }
    {
        const auto max=std::numeric_limits<std::uint64_t>::max();
        Room revisions(902,peer(),max);const auto host=*revisions.host_ticket();
        require(revisions.join_guest(peer()).status==RoomJoinStatus::exhausted);
        require(revisions.leave(host).status==RoomLeaveStatus::exhausted && revisions.size()==1);
        require(bool(revisions.close()[0]) && revisions.closed());
        Room members(903,peer(),1,max);auto last=members.join_guest(peer());
        require(last.status==RoomJoinStatus::joined && last.ticket->member==max);
        members.leave(*last.ticket);require(members.join_guest(peer()).status==RoomJoinStatus::exhausted);members.close();
        Room no_host(904,peer());auto g=no_host.join_guest(peer());
        no_host.leave(*no_host.host_ticket());require(no_host.join_guest(peer()).status==RoomJoinStatus::no_host);
        require(no_host.leave(*g.ticket).empty);
        Room other(905,peer());require(other.leave(host).status==RoomLeaveStatus::stale);other.close();
    }
    std::puts("membership: stale tickets/notices, 128 duplicate + 128 simultaneous leaves, send gate, destruction, exhaustion");
}
