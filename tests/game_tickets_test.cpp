#include "../meta/game_tickets.h"
#include <atomic>
#include <thread>
#include <vector>
int main() {
    meta::GameTickets tickets;
    auto now=meta::GameTickets::Clock::now();
    if(!tickets.issue("one",1,0,now))return 1;
    std::atomic<int> wins{0};std::vector<std::thread> threads;
    for(int i=0;i<16;++i)threads.emplace_back([&]{if(tickets.consume("one",now))++wins;});
    for(auto& thread:threads)thread.join();
    if(wins!=1)return 2;
    tickets.issue("expires",1,0,now);
    if(tickets.consume("expires",now+meta::GameTickets::lifetime))return 3;
    tickets.issue("old",1,0,now);tickets.issue("new",1,0,now);
    if(tickets.consume("old",now) || !tickets.consume("new",now))return 4;
    for(int i=0;i<4096;++i)if(!tickets.issue(std::to_string(i),i,0,now))return 5;
    if(tickets.issue("overflow",5000,0,now))return 6;
    if(!tickets.issue("collected",5000,0,now+meta::GameTickets::lifetime))return 7;
    return 0;
}
