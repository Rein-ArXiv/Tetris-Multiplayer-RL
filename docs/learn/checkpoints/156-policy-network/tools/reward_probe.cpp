#include "meta/sqlite_results.h"
#include <charconv>
#include <cstring>
#include <iostream>
int main(int argc,char** argv) {
    try {
        if(argc<3 || argc>4)return 2;
        std::uint64_t key=0;const auto end=argv[2]+std::strlen(argv[2]);
        const auto parsed=std::from_chars(argv[2],end,key);
        if(parsed.ec!=std::errc{} || parsed.ptr!=end || key==0)return 2;
        const std::string mode=argc==4 ? argv[3] : "";
        if(!mode.empty() && mode!="pause-before" && mode!="pause-after")return 2;
        auto pause=[](const char* message){
            std::cout<<message<<'\n'<<std::flush;
            std::string input;if(!std::getline(std::cin,input))throw std::runtime_error("input closed");
        };
        study_meta::SqliteResults store(argv[1]);store.seed_demo();
        const study_net::MatchRecord record{key,1,101,202,11,100,0,0,0,study_net::MatchRecord::a};
        const auto result=store.put(record,[&](study_meta::RewardPoint point){
            if(mode=="pause-before" && point==study_meta::RewardPoint::before_commit)pause("PENDING");
        });
        if(result.status!=study_meta::SqlStatus::accepted)return 1;
        if(mode=="pause-after")pause("COMMITTED");
        const auto awards=store.awards(key);
        if(!awards)throw std::runtime_error("awards missing");
        std::cout<<"row="<<result.receipt.row<<" awards="<<awards->a<<','<<awards->b
                 <<" balances="<<store.balance(101)<<','<<store.balance(202)<<'\n';
    }catch(const std::exception& e){std::cerr<<"reward: "<<e.what()<<'\n';return 1;}
}
