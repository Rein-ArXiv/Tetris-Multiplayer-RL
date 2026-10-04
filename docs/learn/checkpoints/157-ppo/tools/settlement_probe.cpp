#include "meta/settlement_wire.h"
#include "httplib.h"
#include <charconv>
#include <cstring>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    unsigned port=0;std::uint64_t key=0;
    auto parse=[](const char* text,auto& value){
        const auto end=text+std::strlen(text);const auto p=std::from_chars(text,end,value);
        return p.ec==std::errc{} && p.ptr==end && value!=0;
    };
    if(!parse(argv[1],port) || port>65535 || !parse(argv[2],key))return 2;
    const study_net::MatchRecord record{key,1,101,202,11,100,0,0,0,study_net::MatchRecord::a};
    httplib::Client http("127.0.0.1",static_cast<int>(port));
    http.set_connection_timeout(2,0);http.set_read_timeout(2,0);http.set_write_timeout(2,0);
    const auto response=http.Post("/study/v2/matches",study_meta::record_json(record).dump(),"application/json");
    if(!response){std::cerr<<"unconfirmed\n";return 3;}
    if(response->status!=200){std::cerr<<"HTTP "<<response->status<<'\n';return response->status==409 ? 4 : 3;}
    const auto saved=study_meta::parse_settlement(response->body);
    if(!saved || saved->match.key!=record.key || saved->match.player_a!=record.player_a || saved->match.player_b!=record.player_b) {
        std::cerr<<"unconfirmed response\n";return 3;
    }
    std::cout<<"row="<<saved->match.row<<" awards="<<saved->bp_a<<','<<saved->bp_b<<" policy="<<saved->policy<<'\n';
}
