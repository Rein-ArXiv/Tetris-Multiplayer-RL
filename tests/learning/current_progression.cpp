#include "meta/elo.h"
#include "meta/levels.h"
#include <climits>
#include <cmath>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const std::string mode=argv[1];
    if(mode=="rating-add") {
        const auto r=elo::update(INT_MAX,INT_MAX);
        std::cout<<r.new_winner<<','<<r.new_loser<<'\n';
        return r.new_winner==INT_MAX && r.new_loser==INT_MAX-8 ? 0 : 1;
    }
    if(mode=="rating-difference") {
        const auto high=elo::expected(INT_MAX,INT_MIN),low=elo::expected(INT_MIN,INT_MAX);
        return high>=0.999 && low<=0.001 ? 0 : 1;
    }
    if(mode=="negative-xp") {
        int into=99,need=99;meta::levels::level_progress(INT_MIN,into,need);
        std::cout<<into<<','<<need<<'\n';return into==0 && need==100 ? 0 : 1;
    }
    if(mode=="level-overflow") {
        return meta::levels::total_xp_for_level(INT_MAX)==40120 ? 0 : 1;
    }
    return 2;
}
