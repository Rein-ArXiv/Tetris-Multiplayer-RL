#include "../meta/game_tickets.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#define CHECK(x) do { if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);} } while(false)
int main() {
    using Book=meta::GameTickets;
    Book tickets;const auto now=Book::Clock::now();
    CHECK(tickets.issue("one",1,7,now));
    std::atomic<int> wins{0};std::vector<std::thread> threads;
    for(int i=0;i<16;++i)threads.emplace_back([&]{
        if(auto admission=tickets.consume("one",now)){
            CHECK(admission->player==1&&admission->epoch==7);++wins;
        }
    });
    for(auto& thread:threads)thread.join();
    CHECK(wins==1);CHECK(!tickets.consume("one",now));
    CHECK(tickets.issue("expires",1,0,now));
    CHECK(!tickets.consume("expires",now+Book::lifetime));
    CHECK(!tickets.consume("expires",now)); // Expired consumption also burns it.
    CHECK(tickets.issue("just-before",1,0,now));
    CHECK(tickets.consume("just-before",now+Book::lifetime-Book::Clock::duration(1)));
    CHECK(tickets.issue("old",1,0,now));CHECK(tickets.issue("new",1,0,now));
    CHECK(!tickets.consume("old",now));CHECK(tickets.consume("new",now));
    CHECK(tickets.issue("preserve",1,3,now));CHECK(tickets.issue("occupied",2,4,now));
    CHECK(!tickets.issue("occupied",1,9,now));
    auto preserved=tickets.consume("preserve",now);auto other=tickets.consume("occupied",now);
    CHECK(preserved&&preserved->player==1&&preserved->epoch==3);
    CHECK(other&&other->player==2&&other->epoch==4);
    CHECK(tickets.issue("same",1,0,now));CHECK(!tickets.issue("same",1,1,now));
    auto same=tickets.consume("same",now);CHECK(same&&same->epoch==0);
    for(std::size_t i=0;i<Book::max_pending;++i)
        CHECK(tickets.issue("slot-"+std::to_string(i),static_cast<int64_t>(i),0,now));
    CHECK(!tickets.issue("overflow",static_cast<int64_t>(Book::max_pending),0,now));
    CHECK(tickets.issue("replacement-at-capacity",0,2,now));
    CHECK(!tickets.consume("slot-0",now));
    CHECK(!tickets.issue("overflow-again",static_cast<int64_t>(Book::max_pending),0,now));
    auto replaced=tickets.consume("replacement-at-capacity",now);CHECK(replaced&&replaced->epoch==2);
    CHECK(tickets.issue("collected",9000,0,now+Book::lifetime));
    CHECK(!tickets.issue("clock-overflow",9000,0,Book::Clock::time_point::max()));
    CHECK(tickets.consume("collected",now+Book::lifetime));
    std::cout<<"ticket single-use, expiry, collision preservation, replacement and capacity passed\n";
}
