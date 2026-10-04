#include "meta/bot_challenges.h"
#include "tests/bot_fixture.h"
#include <iostream>
int main(int argc,char** argv){try {
    if(argc!=2)return 2;
    study_meta::SqliteResults store(argv[1]);store.seed_demo();
    const auto ticket=study_bot_test::key(90);
    study_meta::BotChallenges service(store,4,1000000000,{1000,0,1000},{7,12});
    // Fixture account/time/seed. A public adapter must authenticate and issue these.
    const study_bot::Opponent opponent{"dropper-v1",1,1};
    const auto inputs=study_bot_test::proof(77,opponent);
    if(!store.bot_receipt(ticket))
        study_bot_test::require(service.issue({ticket,101,77,opponent,0}));
    const auto result=service.claim(101,ticket,study_bot_test::hex(inputs),inputs.size()*1000000,0);
    study_bot_test::require(result.status==study_meta::BotClaimStatus::confirmed);
    std::cout<<"verified settlement="<<result.awarded<<" shared BP="<<store.balance(101)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
