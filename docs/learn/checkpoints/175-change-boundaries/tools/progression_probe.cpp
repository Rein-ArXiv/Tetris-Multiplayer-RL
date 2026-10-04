#include "meta/sqlite_results.h"
#include "meta/progression_levels.h"
#include <charconv>
#include <cstring>
#include <iostream>
int main(int argc,char** argv) {
    try {
        if(argc!=3)return 2;
        std::uint64_t key=0;const auto end=argv[2]+std::strlen(argv[2]);
        const auto parsed=std::from_chars(argv[2],end,key);
        if(parsed.ec!=std::errc{} || parsed.ptr!=end || key==0)return 2;
        study_meta::SqliteResults db(argv[1]);db.seed_demo();
        const study_net::MatchRecord record{key,1,101,202,11,100,0,0,0,study_net::MatchRecord::a};
        if(db.put(record).status!=study_meta::SqlStatus::accepted)return 1;
        const auto saved=db.progress(key);
        if(!saved)throw std::runtime_error("progress receipt missing");
        std::cout<<"match="<<key<<" A RP="<<saved->a.before<<"->"<<saved->a.after
                 <<" XP+"<<saved->a.xp<<" policy="<<saved->a.policy<<'\n';
        for(const auto player:{101u,202u}) {
            const auto career=db.career(player);const auto p=study_meta::levels::progress(career.xp);
            std::cout<<player<<" RP="<<career.rp<<" XP="<<career.xp<<" BP="<<db.balance(player)
                     <<" level="<<p.level<<" progress="<<p.into<<'/'<<p.need<<'\n';
        }
    }catch(const std::exception& e){std::cerr<<"progression: "<<e.what()<<'\n';return 1;}
}
