#include "meta/bot_challenges.h"
#include "tests/bot_fixture.h"
#include <iostream>
#include <thread>
#include <limits>
using namespace study_meta;
using namespace study_bot_test;
using Status=BotClaimStatus;
int main(int argc,char** argv){try {
    if(argc!=2)return 2;
    const study_bot::Opponent opponent{"dropper-v1",1,1};
    const auto inputs=proof(77,opponent);const auto encoded=hex(inputs);
    require(study_bot::verify(77,opponent,inputs,1000));
    auto bad=inputs;bad.pop_back();require(!study_bot::verify(77,opponent,bad,1000));
    bad=inputs;bad.push_back(0);require(!study_bot::verify(77,opponent,bad,1000));
    bad=inputs;bad[0]=255;require(!study_bot::verify(77,opponent,bad,1000));
    bad=inputs;for(auto& v:bad)v=study_input::drop;require(!study_bot::verify(77,opponent,bad,1000));
    require(!study_bot::verify(77,{"wrong",2,1},inputs,1000));
    std::vector<std::uint8_t> decoded{19};
    for(auto text:{"","0","ff","GG","001f20"}) {
        require(!study_bot::decode(text,1000,decoded));require(decoded==std::vector<std::uint8_t>{19});
    }
    require(study_bot::decode(encoded,1000,decoded) && decoded==inputs);
    require(bot_award({7,12},11)==1 && bot_award({7,12},12)==0);
    require(bot_award({INT32_MAX,INT32_MAX},INT64_MAX)==0);
    bool rejected=false;try{bot_award({7,12},-1);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    SqliteResults store(argv[1]);store.seed_demo();
    const int before=store.balance(101);
    const auto career_before=store.career(101);
    BotChallenges service(store,2,1000000000ULL,{1000,0,1000},{7,12});
    const auto now=static_cast<std::int64_t>(inputs.size())*1000000;
    auto issue=[&](unsigned char n,std::uint64_t player=101) {
        return service.issue({key(n),player,77,opponent,0});
    };
    require(issue(1));require(!service.issue({key(1),202,9,opponent,0}));
    require(service.claim(202,key(1),encoded,now,3).status==Status::unknown);
    require(service.claim(101,key(1),encoded,0,3).status==Status::too_fast);
    require(service.claim(101,key(1),"ff",now,3).status==Status::malformed);
    require(service.claim(101,key(1),encoded,now,3).awarded==7);
    const auto retry=service.claim(101,key(1),"",now,4);
    require(retry.status==Status::confirmed && retry.awarded==7 && store.balance(101)==before+7);
    require(!issue(1));require(issue(2));
    auto partial=service.claim(101,key(2),encoded,now,3);
    require(partial.status==Status::confirmed && partial.awarded==5);
    require(issue(3));auto capped=service.claim(101,key(3),encoded,now,3);
    require(capped.status==Status::confirmed && capped.awarded==0 && store.bot_receipt(key(3)).has_value());
    require(issue(4));require(service.claim(101,key(4),encoded,now,4).awarded==7);
    require(issue(5));require(service.claim(101,key(5),encoded,1000000000,4).status==Status::expired);
    require(issue(6));require(service.claim(101,key(6),"00",now,4).status==Status::not_verified);
    require(service.claim(101,key(6),encoded,now,4).status==Status::unknown);
    require(issue(7));require(issue(8));require(service.claim(101,key(7),encoded,now,4).status==Status::unknown);
    auto superseded=service.claim(101,key(8),encoded,now,4,[&](auto seed,const auto& p,const auto& in,auto max){
        require(issue(9));return study_bot::verify(seed,p,in,max);
    });require(superseded.status==Status::superseded && !store.bot_receipt(key(8)));
    require(issue(10,202));
    require(!issue(14,303)); // Fixed admission capacity leaves both live entries intact.
    auto first=service.claim(101,key(9),encoded,now,4,[&](auto seed,const auto& p,const auto& in,auto max){
        // Another request arrives while this verifier owns the only permit.
        std::thread other([&]{require(service.claim(202,key(10),encoded,now,4).status==Status::busy);});other.join();
        return study_bot::verify(seed,p,in,max);
    });require(first.status==Status::confirmed);
    require(service.claim(202,key(10),encoded,now,4).status==Status::confirmed);
    const auto balance=store.balance(101);
    for(auto point:{BotRewardPoint::after_receipt,BotRewardPoint::after_wallet,BotRewardPoint::before_commit}) {
        bool failed=false;try{store.award_bot(key(11),101,opponent.id,1,5,{7,12},[&](auto p){if(p==point)throw std::runtime_error("injected");});}
        catch(const std::exception&){failed=true;}
        require(failed && !store.bot_receipt(key(11)) && store.balance(101)==balance);
    }
    // Two SQLite connections arbitrate the same durable ticket with BEGIN IMMEDIATE.
    SqliteResults second(argv[1]);BotReceipt a{},b{};
    std::thread one([&]{a=store.award_bot(key(11),101,opponent.id,1,5,{7,12});});
    std::thread two([&]{b=second.award_bot(key(11),101,opponent.id,1,5,{7,12});});one.join();two.join();
    require(a.awarded==7 && b.awarded==7 && store.balance(101)==balance+7);
    {SqliteResults reopened(argv[1]);require(reopened.bot_receipt(key(11))->awarded==7);}
    rejected=false;try{store.award_bot(key(11),202,opponent.id,1,5,{7,12});}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    // Spending affects the shared wallet, not already-earned receipts.
    {SqliteDb db(argv[1]);db.exec("UPDATE wallets SET bp=0 WHERE player_id='101'");}
    require(store.award_bot(key(12),101,opponent.id,1,5,{7,12}).awarded==5);
    require(store.balance(101)==5);
    {SqliteDb db(argv[1]);db.exec("UPDATE wallets SET bp=2147483647 WHERE player_id='101'");}
    rejected=false;try{store.award_bot(key(13),101,opponent.id,1,6,{7,12});}catch(const std::exception&){rejected=true;}
    require(rejected && !store.bot_receipt(key(13)) && store.balance(101)==INT32_MAX);
    require(store.career(101).rp==career_before.rp && store.career(101).xp==career_before.xp && store.count()==0);
    std::cout<<"actual replay, pacing, ownership, supersession, busy, cap, shared wallet, atomic rollback, concurrent durable retry and reopen passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
