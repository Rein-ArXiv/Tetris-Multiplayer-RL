#include "../meta/database.h"
#include <cstdio>
#include <thread>
#include <vector>
int main() {
    meta::Database db(":memory:");
    auto p=db.registerGuest("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    auto other=db.registerGuest("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    if(!p || !other)return 1;
    // Concurrent duplicate claims must award once; independent wins share one daily cap.
    std::vector<std::thread> workers;
    for(int i=0;i<12;++i)workers.emplace_back([&]{db.saveBotWin(p->id,"first","lumen");});
    for(auto& t:workers)t.join();
    if(db.getByToken("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")->bp!=p->bp+10)return 2;
    for(int i=0;i<12;++i)if(!db.saveBotWin(p->id,"match"+std::to_string(i),"lumen"))return 3;
    auto after=db.getByToken("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    if(!after || after->bp!=p->bp+100 || after->elo!=p->elo || after->xp!=p->xp || after->wins!=p->wins)return 4;
    if(db.saveBotWin(other->id,"first","lumen") || db.botReward(other->id,"first"))return 5;
    if(db.botReward(p->id,"first")!=10 || db.botReward(p->id,"match11")!=0)return 6;
    std::optional<meta::Player> buyer;
    if(db.purchaseIcon("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","ruby",buyer)!=meta::IconPurchaseResult::Ok || !buyer || buyer->bp!=p->bp)return 7;
    // Spending BP does not replenish the daily earning allowance.
    if(db.saveBotWin(p->id,"after-purchase","lumen")!=0 || db.getByToken("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")->bp!=p->bp)return 8;
    return 0;
}
