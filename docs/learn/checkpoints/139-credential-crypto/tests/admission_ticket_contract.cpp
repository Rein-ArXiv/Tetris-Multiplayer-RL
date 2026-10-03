#include "meta/ticket_wire.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
using namespace study_meta;
Ticket key(unsigned char value){Ticket t{};t.fill(value);return t;}
int failed_random(unsigned char*,int){return 0;}
int known_random(unsigned char* p,int n){for(int i=0;i<n;++i)p[i]=static_cast<unsigned char>(i);return 1;}
int main() {
    using Book=AdmissionTickets<2>;using Clock=Book::Clock;
    const auto now=Clock::time_point{};const auto life=std::chrono::seconds(5);
    Book store;
    CHECK(store.issue(key(1),{1,7},now,life)==IssueTicket::issued);
    CHECK(store.issue(key(2),{2,8},now,life)==IssueTicket::issued);
    CHECK(store.issue(key(3),{3,9},now,life)==IssueTicket::full);
    CHECK(store.issue(key(2),{1,99},now,life)==IssueTicket::collision);
    CHECK(store.issue(key(1),{1,99},now,life)==IssueTicket::collision);
    auto a=store.consume(key(1),now);CHECK(a&&a->player==1&&a->epoch==7);
    auto b=store.consume(key(2),now);CHECK(b&&b->player==2&&b->epoch==8);
    CHECK(!store.consume(key(1),now));
    CHECK(store.issue(key(1),{1,0},now,life)==IssueTicket::issued);
    CHECK(store.issue(key(2),{2,0},now,life)==IssueTicket::issued);
    CHECK(store.issue(key(3),{1,1},now,life)==IssueTicket::issued);
    CHECK(!store.consume(key(1),now));
    CHECK(store.issue(key(4),{1,1},Clock::time_point::max(),life)==IssueTicket::invalid);
    CHECK(store.issue(key(4),{0,1},now,life)==IssueTicket::invalid);
    CHECK(store.issue(key(4),{1,1},now,Clock::duration::zero())==IssueTicket::invalid);
    CHECK(store.issue(key(4),{1,1},now,-life)==IssueTicket::invalid);
    a=store.consume(key(3),now+life-Clock::duration(1));CHECK(a&&a->epoch==1);
    CHECK(!store.consume(key(2),now+life));CHECK(!store.consume(key(2),now));
    CHECK(store.issue(key(4),{3,2},now+life,life)==IssueTicket::issued);
    // Epoch rejection is a later account decision; the consumed ticket stays burned.
    a=store.consume(key(4),now+life);const std::uint64_t current_epoch=3;
    CHECK(a&&a->epoch!=current_epoch);CHECK(!store.consume(key(4),now+life));
    Book restarted;CHECK(!restarted.consume(key(4),now));
    // Expired slots free capacity even without an attempted redemption.
    CHECK(store.issue(key(5),{1,0},now,life)==IssueTicket::issued);
    CHECK(store.issue(key(6),{2,0},now,life)==IssueTicket::issued);
    CHECK(store.issue(key(7),{3,0},now+life,life)==IssueTicket::issued);
    CHECK(!store.consume(key(5),now));CHECK(!store.consume(key(6),now));
    // Simultaneous callers all start from one issued credential.
    CHECK(store.issue(key(8),{4,10},now+life,life)==IssueTicket::issued);
    std::atomic<int> ready{0},wins{0};std::atomic<bool> go{false};std::vector<std::thread> threads;
    for(int i=0;i<12;++i)threads.emplace_back([&]{
        ++ready;while(!go.load())std::this_thread::yield();
        if(auto admitted=store.consume(key(8),now+life)){CHECK(admitted->player==4&&admitted->epoch==10);++wins;}
    });
    while(ready!=12)std::this_thread::yield();
    go=true;
    for(auto& t:threads)t.join();
    CHECK(wins==1);
    // A fixed key in a test is not a credential generator. Removed keys can reissue.
    CHECK(store.issue(key(8),{4,11},now+life,life)==IssueTicket::issued);
    CHECK(store.consume(key(8),now+life));
    for(unsigned value=0;value<256;++value){auto k=key(static_cast<unsigned char>(value));CHECK(parse_ticket(ticket_text(k))==k);}
    const auto good=ticket_text(key(10));
    CHECK(!parse_ticket(good.substr(1)));CHECK(!parse_ticket(good+'0'));
    auto bad=good;bad.back()='G';CHECK(!parse_ticket(bad));bad=good;bad.back()='\0';CHECK(!parse_ticket(bad));
    CHECK(!parse_ticket("gt1."+good.substr(kTicketPrefix.size())));
    CHECK(!new_ticket(failed_random));CHECK(!new_ticket(nullptr));
    auto generated=new_ticket(known_random);CHECK(generated);
    for(std::size_t i=0;i<generated->size();++i)CHECK((*generated)[i]==i);
    CHECK(new_ticket()); // Runtime availability, not a proof of entropy strength.
    std::cout<<"ticket state, atomic consumption, time, generation boundary and wire contract passed\n";
}
