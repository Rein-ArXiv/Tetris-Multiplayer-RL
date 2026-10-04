#include "simulation/catalog.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"catalog line %d: %s\n",__LINE__,#x);return 1;}}while(false)
using namespace study_catalog;
static_assert(valid_catalog(definitions));
static_assert(find(Kind::I)->kind==Kind::I && definitions[0].kind==Kind::I);
static_assert(find_id(1)->name=="L" && find_name("T")->kind==Kind::T);
static unsigned mask(const Shape& shape){unsigned m=0;for(auto c:shape)m|=1u<<(c.row*4+c.column);return m;}
// Independent 4x4 bit-board reachability. Bit shifts cannot wrap across rows.
static bool connected(unsigned cells){
    unsigned reached=cells&(~cells+1u);
    for(;;){
        unsigned neighbors=(reached<<4)|(reached>>4)|((reached&0x7777u)<<1)|((reached&0xEEEEu)>>1);
        unsigned next=reached|(neighbors&cells);if(next==reached)return reached==cells;reached=next;
    }
}
int main(){
    const int ids[]={3,2,1,4,5,6,7};const char* names[]={"I","J","L","O","S","T","Z"};
    const unsigned shapes[]={0xF0,0x71,0x74,0x33,0x36,0x72,0x63};
    for(std::size_t i=0;i<7;++i){
        const auto& d=definitions[i];CHECK(static_cast<int>(d.kind)==ids[i]&&d.name==names[i]);
        CHECK(mask(d.cells)==shapes[i]);CHECK(find(d.kind)==&d&&find_id(ids[i])==&d&&find_name(names[i])==&d);
        auto piece=make_piece(d.kind);CHECK(piece);CHECK(piece->origin.row==0&&piece->origin.column==(i==3?4:3));
        const auto copy=make_piece(d.kind);piece->origin.row=7;piece->local[0].row=3;
        CHECK(copy->origin.row==0&&d.cells[0].row!=3&&copy->local[0].row==d.cells[0].row);
    }
    for(int id:{INT_MIN,-1,0,8,255,257,INT_MAX})CHECK(!find_id(id));
    CHECK(!find(static_cast<Kind>(0))&&!make_piece(static_cast<Kind>(255)));
    for(auto name:{"","t"," T","T ","TT","--help","X"})CHECK(!find_name(name));
    auto bad=definitions;bad[0].kind=Kind::J;CHECK(!valid_catalog(bad));
    bad=definitions;bad[0].name="J";CHECK(!valid_catalog(bad));
    bad=definitions;bad[0].kind=static_cast<Kind>(8);CHECK(!valid_catalog(bad));
    bad=definitions;bad[0].name="";CHECK(!valid_catalog(bad));
    bad=definitions;bad[0].name="i";CHECK(!valid_catalog(bad));
    bad=definitions;bad[0].cells[0]=bad[0].cells[1];CHECK(!valid_catalog(bad));
    for(int extreme:{INT_MIN,-1,4,INT_MAX}){
        auto s=definitions[0].cells;s[0].row=extreme;CHECK(!valid_shape(s));
        s=definitions[0].cells;s[0].column=extreme;CHECK(!valid_shape(s));
    }
    for(Origin o:{Origin{INT_MAX,0},{0,INT_MAX},{-1,0},{0,-1},{19,3},{0,8}}){
        bad=definitions;bad[0].spawn=o;CHECK(!valid_catalog(bad));
    }
    // Structure cannot certify that an ID is attached to its intended shape.
    bad=definitions;bad[5].cells=definitions[1].cells;
    CHECK(valid_catalog(bad)&&mask(bad[5].cells)!=shapes[5]);
    unsigned sets=0,permutations=0;
    for(unsigned bits=1;bits<65536;++bits){
        unsigned n=0;for(unsigned x=bits;x;x>>=1)n+=x&1u;if(n!=4)continue;
        Shape shape{};std::size_t k=0;
        for(int bit=0;bit<16;++bit)if(bits&(1u<<bit))shape[k++]={bit/4,bit%4};
        std::array<int,4> order{{0,1,2,3}};const bool expected=connected(bits);
        do{Shape permuted{};for(int i=0;i<4;++i)permuted[i]=shape[order[i]];
            CHECK(valid_shape(permuted)==expected);++permutations;
        }while(std::next_permutation(order.begin(),order.end()));
        ++sets;
    }
    CHECK(sets==1820&&permutations==43680);
    std::printf("catalog: 7 identities, 1820 sets / 43680 orders, raw IDs, malformed records and independent copies passed\n");
}
