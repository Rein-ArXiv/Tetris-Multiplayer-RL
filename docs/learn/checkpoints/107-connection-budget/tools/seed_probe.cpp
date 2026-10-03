#include "net/seed_handshake.h"
#include "simulation/state_hash.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <limits>
namespace {
bool number(const char* s,uint64_t& out) {
 const char* end=s+std::strlen(s);auto r=std::from_chars(s,end,out);
 return r.ec==std::errc{} && r.ptr==end;
}
}
int main(int argc,char** argv) {
 if(argc==2 && std::strcmp(argv[1],"--help")==0){std::puts("seed_probe listen PORT SEED | connect PORT");return 0;}
 if(argc<3)return 2;
 const bool host=std::strcmp(argv[1],"listen")==0;
 if((host&&argc!=4)||(!host&&(std::strcmp(argv[1],"connect")!=0||argc!=3)))return 2;
 uint64_t port=0,seed=0;if(!number(argv[2],port)||port>65535||(!host&&port==0)||(host&&!number(argv[3],seed)))return 2;
 study_net::Runtime runtime;if(!runtime.ready())return 1;
 study_net::Connection connection;int error=0;
 if(host) {
  uint16_t bound=0;auto listener=study_net::listen_loopback(uint16_t(port),bound,error);if(!listener.valid())return 1;
  std::printf("LISTEN %u\n",unsigned(bound));std::fflush(stdout);
  auto accepted=study_net::accept_one(listener,error);if(!connection.adopt(accepted))return 1;
 } else if(connection.start(uint16_t(port)).state!=study_net::Connection::StartState::started)return 1;
 const study_net::Config sentinel{9,10,11};auto agreed=sentinel;
 const study_net::Config proposed{seed,120,2};
 const auto result=host?study_net::negotiate_host(connection,proposed,agreed):study_net::negotiate_peer(connection,agreed);
 if(result!=study_net::HandshakeResult::ready) {
  std::printf("FAILED result=%d unchanged=%d active=%d\n",int(result),study_net::equal_config(agreed,sentinel),connection.active());return 1;
 }
 // The negotiated common config initializes the existing pure simulation.
 auto round=study_round::Round::create_seeded(study_grid::Grid{},agreed.seed);
 if(!round)return 1;
 const auto hash=study_hash::state_hash(*round);
 if(!hash)return 1;
 std::printf("READY role=%s seed=%llu countdown=%u delay=%u hash=%llu\n",host?"host":"peer",static_cast<unsigned long long>(agreed.seed),unsigned(agreed.countdown_ticks),unsigned(agreed.input_delay),static_cast<unsigned long long>(*hash));
 connection.close();
}
