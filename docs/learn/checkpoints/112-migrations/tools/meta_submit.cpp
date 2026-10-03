#include "meta/http_sender.h"
#include "simulation/duel.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <limits>
// A trusted local simulation fixture supplies a result; no client win claim is read.
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    int port=0;std::uint64_t key=0;
    auto p=std::from_chars(argv[1],argv[1]+std::strlen(argv[1]),port);
    auto k=std::from_chars(argv[2],argv[2]+std::strlen(argv[2]),key);
    if(p.ec!=std::errc{} || *p.ptr || k.ec!=std::errc{} || *k.ptr || port<1 || port>65535 || key==0)return 2;
    const auto board=study_round::Round::create_seeded(study_grid::Grid{},77);if(!board)return 1;
    study_combat::Duel game(*board,*board);
    study_input::Intent left{},right{};left.hard_drop=true;
    std::uint64_t ticks=0;
    while(!game.left().finished() && !game.right().finished()) {
        if(ticks>=1000 || !game.tick(left,right))return 1;
        ++ticks;
    }
    const auto& a=game.left();const auto& b=game.right();
    if(a.total_lines()>std::numeric_limits<std::uint32_t>::max() ||
       b.total_lines()>std::numeric_limits<std::uint32_t>::max())return 1;
    const auto winner=a.finished()==b.finished() ? study_net::MatchRecord::draw
        : a.finished() ? study_net::MatchRecord::b : study_net::MatchRecord::a;
    study_net::MatchRecord record{key,1,101,202,ticks,a.score(),b.score(),
        static_cast<std::uint32_t>(a.total_lines()),static_cast<std::uint32_t>(b.total_lines()),winner};
    study_net::MatchSubmission submission(1);
    if(!submission.prepare(record))return 1;
    const auto step=submission.submit(study_meta::HttpSender(port));
    if(step!=study_net::MatchSubmission::SubmitStep::confirmed){std::puts("unconfirmed");return 3;}
    std::printf("confirmed key=%llu row=%llu ticks=%llu\n",static_cast<unsigned long long>(key),
        static_cast<unsigned long long>(submission.receipt()->row),static_cast<unsigned long long>(ticks));
}
