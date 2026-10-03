#include "net/input_message.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"input admission line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
#define NET_WARN(x) do{}while(false)
using namespace net;
std::mutex inMu;std::unordered_map<std::uint32_t,std::uint8_t> remoteInputs;
std::atomic<std::uint32_t> lastRemoteTick{0};std::atomic_bool connectionFailed{false},quit{false};unsigned acknowledgements=0;
void pushSend(std::vector<std::uint8_t>&&){++acknowledgements;}
void handle(const Frame& f){switch(f.type){
#include "input_admission.inc"
 default:break;}}
Frame input(std::uint32_t from,unsigned count,std::uint8_t mask=0){Frame f{MsgType::INPUT,{}};le_write_u32(f.payload,from);le_write_u16(f.payload,static_cast<std::uint16_t>(count));f.payload.insert(f.payload.end(),count,mask);return f;}
void fill(unsigned n){remoteInputs.clear();for(unsigned i=0;i<n;++i)remoteInputs[i]=0;lastRemoteTick=n-1;connectionFailed=false;quit=false;acknowledgements=0;}
int main(){fill(8191);handle(input(8191,2));CHECK(remoteInputs.size()==8191);CHECK(lastRemoteTick==8190);CHECK(connectionFailed&&quit&&acknowledgements==0);
#ifndef BEFORE
 fill(8190);handle(input(8190,2));CHECK(remoteInputs.size()==8192&&!quit&&lastRemoteTick==8191&&acknowledgements==1);
 handle(input(8190,2));CHECK(remoteInputs.size()==8192&&!quit&&acknowledgements==2);
 fill(8191);handle(input(8190,2));CHECK(remoteInputs.size()==8192&&!quit&&lastRemoteTick==8191);
 // Existing distance policy still discards far-away ticks. This is not a full
 // redesign of sequence windows, conflicting repeats or round identifiers.
 fill(8191);handle(input(20000,2));CHECK(remoteInputs.size()==8191&&!quit&&lastRemoteTick==8190);
 fill(8191);auto bad=input(8191,2);bad.payload.pop_back();handle(bad);CHECK(remoteInputs.size()==8191&&!quit&&acknowledgements==0);
#endif
 std::puts("actual INPUT: preflight capacity rejects whole eligible batch without ACK; exact fit/duplicates preserved");}
