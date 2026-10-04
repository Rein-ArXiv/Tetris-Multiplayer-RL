#include "meta/ticket_wire.h"
#include <iostream>
int main() {
    using namespace study_meta;
    // Explicit experiment conditions, independent of the service's capacity/TTL.
    AdmissionTickets<2> store;using Clock=AdmissionTickets<2>::Clock;
    const auto now=Clock::time_point{};const auto life=std::chrono::seconds(5);
    const Admission account{42,3};
    auto first=new_ticket();auto second=new_ticket();
    if(!first||!second||*first==*second)return 1;
    if(store.issue(*first,account,now,life)!=IssueTicket::issued)return 2;
    if(store.issue(*second,account,now,life)!=IssueTicket::issued)return 3;
    if(store.consume(*first,now))return 4;
    auto redeemed=store.consume(*second,now);
    if(!redeemed||redeemed->player!=account.player||redeemed->epoch!=account.epoch)return 5;
    // Simulate losing the successful response. A retry cannot redeem it again.
    if(store.consume(*second,now))return 6;
    auto expiring=new_ticket();if(!expiring)return 7;
    if(store.issue(*expiring,account,now,life)!=IssueTicket::issued)return 8;
    if(store.consume(*expiring,now+life))return 9;
    std::cout<<"old replaced; current consumed once; lost response retry rejected; expiry boundary rejected\n";
}
