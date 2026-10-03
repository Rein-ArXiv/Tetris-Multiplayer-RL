#include "net/relay_cost.h"
#include <cstdio>
#include <limits>
#include <initializer_list>
using namespace study_net;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
bool same(const RelayTraffic&a,const RelayTraffic&b){return a.connected_sockets==b.connected_sockets&&a.ingress_bytes_per_second==b.ingress_bytes_per_second&&a.egress_bytes_per_second==b.egress_bytes_per_second&&a.queue_capacity_bytes==b.queue_capacity_bytes;}
int main(){
 const auto max=(std::numeric_limits<std::uint64_t>::max)();std::uint64_t out=77;
 CHECK(!checked_cost_add(max,1,out)&&out==77);CHECK(checked_cost_add(max,0,out)&&out==max);
 out=77;CHECK(!checked_cost_mul(max,2,out)&&out==77);CHECK(checked_cost_mul(max,0,out)&&out==0);
 CHECK(estimate_direction({10000,2000,1000,15000},out)&&out==28000);
 std::uint64_t back;CHECK(estimate_direction({16000,0,1000,12000},back)&&back==29000);
 CHECK(checked_cost_add(out,back,out)&&out==57000);
 CHECK(!estimate_direction({max,0,0,1},out)&&out==57000);
 CHECK(estimate_direction({0,max,0,0},out)&&out==max);
 RelayTraffic result;
 CHECK(estimate_traffic({100,18,60,4096},result));
 CHECK(result.connected_sockets==200&&result.ingress_bytes_per_second==216000&&result.egress_bytes_per_second==216000&&result.queue_capacity_bytes==819200);
 const RelayTraffic sentinel{11,22,33,44};
 for(auto plan:{RelayTrafficPlan{max,0,0,0},{1,max,1,0},{1,2,max,0},{1,0,0,max}}){result=sentinel;CHECK(!estimate_traffic(plan,result)&&same(result,sentinel));}
 CHECK(estimate_traffic({0,max,max,max},result)&&same(result,{}));
 CHECK(estimate_traffic({1,max,0,0},result)&&result.connected_sockets==2&&result.ingress_bytes_per_second==0);
 CHECK(estimate_traffic({1,0,max,0},result)&&result.egress_bytes_per_second==0);
 CHECK(estimate_traffic({max/2,1,1,1},result)&&result.connected_sockets==max-1&&result.queue_capacity_bytes==max-1);
 for(unsigned m=0;m<20;++m)for(unsigned b=0;b<20;++b)for(unsigned f=0;f<20;++f)for(unsigned q=0;q<20;++q){
  CHECK(estimate_traffic({m,b,f,q},result));CHECK(result.connected_sockets==2*m&&result.ingress_bytes_per_second==2*m*b*f&&result.egress_bytes_per_second==2*m*b*f&&result.queue_capacity_bytes==2*m*q);
 }
 std::puts("ASSUMED path: forward=28000us reverse=29000us total=57000us; no measured RTT");
 std::puts("ASSUMED 100 matches: sockets=200 ingress=216000B/s egress=216000B/s queue budgets=819200B");
 std::puts("cost contracts: direction asymmetry, dimensional counts, zero factors, overflow and output preservation");
}
