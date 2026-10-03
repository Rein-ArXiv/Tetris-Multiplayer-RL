#include "net/first_admission.h"
#include <cstdio>
#include <vector>
#include <limits>
using namespace study_net;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(false)
std::vector<std::uint8_t> bytes(std::uint8_t type,std::initializer_list<std::uint8_t> payload){Frame f;f.type=type;f.size=payload.size();std::size_t n=0;for(auto b:payload)f.payload[n++]=b;EncodedFrame e;if(!encode_frame(f,e))return {};return {e.bytes.begin(),e.bytes.begin()+e.size};}
bool same(const AdmissionRequest&a,const AdmissionRequest&b){return a.route==b.route&&a.room==b.room;}
int main(){
 const AdmissionRequest sentinel{AdmissionRoute::join,{'Z','Z','Z','Z','Z'}};
 for(unsigned type=0;type<256;++type)for(unsigned size=0;size<40;++size)for(unsigned route=0;route<256;++route){
  Frame f;f.type=type;f.size=size;f.payload[0]=route;f.payload[1]=0;auto out=sentinel;
  const bool expected=type==50&&size==2&&(route==1||route==2);
  CHECK(decode_admission(f,out)==expected);if(!expected)CHECK(same(out,sentinel));
 }
 Frame join;join.type=50;join.size=7;join.payload={3,5,'A','B','C','D','9'};
 for(unsigned pos=0;pos<5;++pos)for(unsigned raw=0;raw<256;++raw){auto f=join;f.payload[pos+2]=raw;auto out=sentinel;const bool valid=(raw>='A'&&raw<='Z')||(raw>='0'&&raw<='9');CHECK(decode_admission(f,out)==valid);if(!valid)CHECK(same(out,sentinel));}
 for(unsigned n=0;n<256;++n){auto f=join;f.payload[1]=n;auto out=sentinel;CHECK(decode_admission(f,out)==(n==5));if(n!=5)CHECK(same(out,sentinel));}
 CHECK(!FirstAdmission::create(0,0));
 auto command=bytes(50,{1,0}),follow=bytes(60,{9}),last=bytes(61,{8});
 std::vector<std::uint8_t> stream=command;stream.insert(stream.end(),follow.begin(),follow.end());stream.insert(stream.end(),last.begin(),last.end());
 // Every partition of the13-byte pipeline, followed by parser/socket ownership handoff.
 for(unsigned cuts=0;cuts<(1u<<(stream.size()-1));++cuts){
  auto a=*FirstAdmission::create(0,10);AdmissionHandoff out;std::size_t begin=0;bool taken=false;
  for(std::size_t end=1;end<=stream.size();++end)if(end==stream.size()||(cuts&(1u<<(end-1)))){
   if(!taken){const auto state=a.feed(stream.data()+begin,end-begin,1);CHECK(state==AdmissionState::waiting||state==AdmissionState::routed);if(state==AdmissionState::routed){CHECK(a.take(out));taken=true;}}
   else CHECK(out.parser.append(stream.data()+begin,end-begin));
   begin=end;
  }
  CHECK(taken&&a.poll(1)==AdmissionState::handed_off&&a.read_capacity()==0);
  Frame x;CHECK(out.parser.next(x)==ParseStatus::frame&&x.type==60&&x.payload[0]==9);CHECK(out.parser.next(x)==ParseStatus::frame&&x.type==61&&x.payload[0]==8);CHECK(out.parser.pending_bytes()==0);
  out.request=sentinel;CHECK(!a.take(out)&&same(out.request,sentinel));
 }
 auto a=*FirstAdmission::create(100,10);CHECK(a.feed(command.data(),1,101)==AdmissionState::waiting);CHECK(a.feed(command.data()+1,4,110)==AdmissionState::timed_out);CHECK(a.read_capacity()==0);
 a=*FirstAdmission::create(100,10);CHECK(a.poll(105)==AdmissionState::waiting);CHECK(a.eof(104)==AdmissionState::clock_error);CHECK(a.poll(105)==AdmissionState::waiting);CHECK(a.feed(command.data(),5,104)==AdmissionState::clock_error);CHECK(a.feed(command.data(),5,109)==AdmissionState::routed);CHECK(a.eof(109)==AdmissionState::routed);AdmissionHandoff out;CHECK(a.take(out));
 auto unknown=bytes(90,{});a=*FirstAdmission::create(0,10);
 for(unsigned i=1;i<=4;++i)CHECK(a.feed(unknown.data(),unknown.size(),i)==AdmissionState::waiting);
 CHECK(a.feed(unknown.data(),unknown.size(),5)==AdmissionState::ignored_limit);
 a=*FirstAdmission::create(0,10);CHECK(a.feed(nullptr,0,9)==AdmissionState::waiting);CHECK(a.poll(10)==AdmissionState::timed_out);
 a=*FirstAdmission::create(0,10);CHECK(a.feed(nullptr,1,0)==AdmissionState::protocol_error);
 a=*FirstAdmission::create(0,10);CHECK(a.feed(command.data(),17,0)==AdmissionState::byte_limit);
 const std::uint8_t bad[]={0,0};a=*FirstAdmission::create(0,10);CHECK(a.feed(bad,2,0)==AdmissionState::protocol_error);
 a=*FirstAdmission::create(0,10);CHECK(a.eof(0)==AdmissionState::peer_closed);
 a=*FirstAdmission::create(0,10);CHECK(a.feed(command.data(),1,0)==AdmissionState::waiting);CHECK(a.eof(1)==AdmissionState::truncated);
 // Three maximal unknown frames plus a partial fourth exhaust byte budget before count budget.
 Frame large;large.type=90;large.size=32;EncodedFrame e;CHECK(encode_frame(large,e));std::vector<std::uint8_t> flood;
 for(unsigned i=0;i<4;++i)flood.insert(flood.end(),e.bytes.begin(),e.bytes.begin()+e.size);
 a=*FirstAdmission::create(0,10);for(unsigned i=0;i<8;++i){const auto state=a.feed(flood.data()+i*16,16,0);CHECK(state==(i==7?AdmissionState::byte_limit:AdmissionState::waiting));}
 CHECK(a.read_capacity()==0);
 // Four unknown frames totaling123 bytes, then a valid5-byte request at128.
 std::vector<std::uint8_t> exact;
 for(unsigned wire:{35u,35u,35u,18u}){Frame f;f.type=90;f.size=wire-3;EncodedFrame enc;CHECK(encode_frame(f,enc));exact.insert(exact.end(),enc.bytes.begin(),enc.bytes.begin()+enc.size);}
 exact.insert(exact.end(),command.begin(),command.end());CHECK(exact.size()==128);
 a=*FirstAdmission::create(0,10);for(unsigned i=0;i<8;++i)CHECK(a.feed(exact.data()+i*16,16,1)==(i==7?AdmissionState::routed:AdmissionState::waiting));
 CHECK(a.take(out)&&out.parser.pending_bytes()==0);
 a=*FirstAdmission::create(0,10);CHECK(a.feed(unknown.data(),unknown.size(),9)==AdmissionState::waiting);CHECK(a.poll(10)==AdmissionState::timed_out);
 const auto malformed=bytes(50,{1,1});a=*FirstAdmission::create(0,10);CHECK(a.feed(malformed.data(),malformed.size(),1)==AdmissionState::protocol_error);
 // A malformed later header is inspected by the next phase, after routing.
 auto later=command;later.push_back(0);later.push_back(0);
 a=*FirstAdmission::create(0,10);CHECK(a.feed(later.data(),later.size(),1)==AdmissionState::routed);CHECK(a.take(out));Frame invalid;CHECK(out.parser.next(invalid)==ParseStatus::error);
 const auto max=(std::numeric_limits<std::uint64_t>::max)();a=*FirstAdmission::create(max-10,10);CHECK(a.poll(max-1)==AdmissionState::waiting&&a.poll(max)==AdmissionState::timed_out);
 a=*FirstAdmission::create(max-2,10);CHECK(a.poll(max)==AdmissionState::waiting&&a.eof(0)==AdmissionState::clock_error);
 std::puts("admission: exact command domain, all4096 pipeline partitions, one-shot tail handoff, deadline, budgets, EOF and clock preservation");
}
