#pragma once
#include "meta/sqlite_results.h"
#include "net/authoritative_match.h"
#include "tests/authority_fixture.h"
namespace study_review_test {
inline study_net::MatchRecord match(std::uint64_t key,bool swapped=false) {
    using namespace study_net;
    AuthoritativeMatch game(9,swapped?202:101,swapped?101:202,77,1000);
    for(std::uint32_t tick=0;game.result().state==MatchState::incomplete;++tick) {
        study_authority_test::require(tick<1000);
        game.submit(101,study_authority_test::frame(tick,{0}));
        game.submit(202,study_authority_test::frame(tick,{study_input::drop}));
    }
    auto record=game.record(key);study_authority_test::require(record.has_value());return *record;
}
}
