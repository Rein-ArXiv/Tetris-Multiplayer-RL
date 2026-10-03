#include "simulation/input_mask.h"
#include "simulation/pending_controls.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#x);std::exit(1);} }while(false)
using namespace study_input;
static bool bit(unsigned value,unsigned position){return (value/(1u<<position))%2!=0;}
static void same(Intent a,Intent b){CHECK(a.horizontal==b.horizontal&&a.clockwise==b.clockwise&&a.soft_drop==b.soft_drop&&a.hard_drop==b.hard_drop);}
static void capture(PendingControls& p,unsigned v){p.capture(bit(v,0),bit(v,1),bit(v,3),bit(v,2),bit(v,4));}
int main(){
    CHECK(left==1&&right==2&&down==4&&rotate==8&&drop==16&&known==31);
    for(unsigned value=0;value<65536;++value){
        CHECK(valid(value)==(value<32));
        const auto intent=decode(value);CHECK(bool(intent)==(value<32));
        if(!intent)continue;
        same(*intent,{int(bit(value,1))-int(bit(value,0)),bit(value,3),bit(value,2),bit(value,4)});
        const auto encoded=encode(*intent);CHECK(encoded);
        const auto canonical=value%4==3?value-3:value;
        CHECK(*encoded==canonical);same(*decode(*encoded),*intent);
    }
    for(unsigned value:{65536u,65537u,std::numeric_limits<unsigned>::max()})CHECK(!decode(value)&&!valid(value));
    for(int h:{-1,0,1})for(unsigned flags=0;flags<8;++flags){
        Intent input{h,bit(flags,0),bit(flags,1),bit(flags,2)};
        CHECK(encode(input));same(*decode(*encode(input)),input);
    }
    for(int h:{-2,2,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})CHECK(!encode({h,false}));
    for(unsigned value=0;value<256;++value)for(unsigned query=0;query<256;++query){
        unsigned overlap=0,requested=0;
        for(unsigned i=0;i<8;++i){overlap+=bit(value,i)&&bit(query,i);requested+=bit(query,i);}
        CHECK(has_any(static_cast<Mask>(value),static_cast<Mask>(query))==(overlap>0));
        CHECK(has_all(static_cast<Mask>(value),static_cast<Mask>(query))==(overlap==requested));
    }
    // Three render captures then two tick consumes: scalar Boolean oracle.
    for(unsigned a=0;a<32;++a)for(unsigned b=0;b<32;++b)for(unsigned c=0;c<32;++c){
        PendingControls input,raw;
        for(auto frame:{a,b,c}){capture(input,frame);capture(raw,frame);}
        const bool l=bit(a,0)||bit(b,0)||bit(c,0),r=bit(a,1)||bit(b,1)||bit(c,1);
        const bool cw=bit(a,3)||bit(b,3)||bit(c,3),hard=bit(a,4)||bit(b,4)||bit(c,4),soft=bit(c,2);
        const Intent expected{int(r)-int(l),cw,soft,hard};
        same(input.consume(),expected);
        CHECK(raw.consume_mask()==unsigned(l)+2u*unsigned(r)+4u*unsigned(soft)+8u*unsigned(cw)+16u*unsigned(hard));
        same(input.consume(),{0,false,soft,false});CHECK(raw.consume_mask()==(soft?4:0));
        capture(input,c);input.clear();same(input.consume(),{0,false,false,false});
    }
    std::puts("Mask contract: wide values, 24 intents, all bit queries, 32,768 frame histories, consume and clear passed");
}
