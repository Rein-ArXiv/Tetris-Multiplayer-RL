#include "net/hash_audit.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
using namespace study_net;
int main() {
    StateStamp s{4,UINT64_C(0x0807060504030201),UINT64_C(0x1817161514131211)};
    Frame f;CHECK(encode_stamp(s,f));CHECK(f.type==21 && f.size==24);
    CHECK(f.payload[0]==4);
    for(unsigned i=1;i<8;++i)CHECK(f.payload[i]==0);
    for(unsigned i=0;i<8;++i)CHECK(f.payload[8+i]==i+1 && f.payload[16+i]==i+17);
    StateStamp out{8,9,10};CHECK(decode_stamp(f,out)&&same_stamp(s,out));
    const auto good=f;
    for(std::size_t n=0;n<=40;++n)if(n!=24) {
        f=good;f.size=n;out={8,9,10};CHECK(!decode_stamp(f,out)&&same_stamp(out,{8,9,10}));
    }
    for(unsigned type=0;type<256;++type)if(type!=21) {
        f=good;f.type=static_cast<uint8_t>(type);CHECK(!decode_stamp(f,out));
    }
    for(auto tick:{UINT64_C(1),UINT64_C(3),kMaxStateTick+1,UINT64_MAX}) {
        f=good;CHECK(!encode_stamp({tick,0,0},f));CHECK(f.payload==good.payload&&f.size==good.size&&f.type==good.type);
        ByteWriter w(f.payload.data(),f.payload.size());CHECK(w.u64(tick));out={8,9,10};CHECK(!decode_stamp(f,out)&&same_stamp(out,{8,9,10}));
    }
    for(auto tick:{UINT64_C(0),kMaxStateTick}) {
        CHECK(encode_stamp({tick,0,UINT64_MAX},f));CHECK(decode_stamp(f,out)&&same_stamp(out,{tick,0,UINT64_MAX}));
    }
    HashAudit audit;Comparison c{77,true,false};
    CHECK(audit.record_remote({4,10,20})==AuditPut::stored);
    CHECK(audit.record_local({4,10,20})==AuditPut::stored);
    CHECK(!audit.poll(c)&&c.tick==77&&c.host_equal&&!c.peer_equal&&audit.next_tick()==0);
    CHECK(audit.record_remote({0,0,0})==AuditPut::stored);
    CHECK(audit.record_remote({0,0,0})==AuditPut::duplicate);
    CHECK(audit.record_remote({0,1,0})==AuditPut::conflict);
    CHECK(!audit.poll(c));CHECK(audit.record_local({0,0,0})==AuditPut::stored);
    CHECK(audit.poll(c)&&c.tick==0&&c.matched());
    CHECK(audit.poll(c)&&c.tick==4&&c.matched());
    CHECK(audit.record_remote({0,0,0})==AuditPut::stale);
    CHECK(audit.record_remote({40,0,0})==AuditPut::too_far);
    CHECK(audit.record_remote({9,0,0})==AuditPut::invalid);
    for(unsigned unequal=0;unequal<4;++unequal) {
        HashAudit a;
        CHECK(a.record_local({0,11,29})==AuditPut::stored);
        CHECK(a.record_remote({0,unequal&1?12u:11u,unequal&2?30u:29u})==AuditPut::stored);
        CHECK(a.poll(c)&&c.host_equal==!(unequal&1)&&c.peer_equal==!(unequal&2));
        CHECK(c.matched()==(unequal==0)&&a.next_tick()==4); // Mismatch is still one comparison.
    }
    // Out-of-order arrivals must not move the comparison frontier.
    for(unsigned shift=0;shift<8;++shift) {
        HashAudit a;
        for(unsigned i=0;i<8;++i) {
            const unsigned t=((i+shift)%8)*4;
            CHECK(a.record_remote({t,t,t+1})==AuditPut::stored);
        }
        CHECK(a.record_remote({32,32,33})==AuditPut::too_far && a.next_tick()==0);
        for(unsigned i=8;i>0;--i)CHECK(a.record_local({(i-1)*4,(i-1)*4,(i-1)*4+1})==AuditPut::stored);
        for(unsigned i=0;i<8;++i)CHECK(a.poll(c)&&c.tick==i*4&&c.matched());
        CHECK(!a.poll(c));CHECK(a.record_local({32,32,33})==AuditPut::stored);
        CHECK(a.record_remote({32,32,33})==AuditPut::stored&&a.poll(c)&&c.matched());
        a=HashAudit{};CHECK(a.next_tick()==0 && !a.poll(c));
    }
    std::puts("HASH_CODEC_AUDIT_OK");
}
