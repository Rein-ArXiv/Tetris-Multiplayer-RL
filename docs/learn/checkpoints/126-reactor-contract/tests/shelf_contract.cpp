#include "text/shelf.h"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main(){
 std::uint32_t rng=17;unsigned accepted=0,rejected=0;
 for(int w=3;w<=40;++w)for(int h=3;h<=30;++h){
  study_atlas::Shelf shelf(w,h);std::vector<bool> occupied(std::size_t(w)*h);
  for(int step=0;step<80;++step){
   rng=rng*1664525u+1013904223u;int iw=1+int(rng%19);rng=rng*1664525u+1013904223u;int ih=1+int(rng%17);
   auto before=shelf;auto p=shelf.insert(iw,ih);
   if(!p){
    ++rejected;auto expected=before.insert(1,1),actual=shelf.insert(1,1);
    CHECK(bool(expected)==bool(actual));
    if(actual){CHECK(actual->outer.x==expected->outer.x&&actual->outer.y==expected->outer.y);shelf=before;}
    continue;
   }
   ++accepted;const auto r=p->outer,i=p->ink;
   CHECK(r.x>=0&&r.y>=0&&r.x+r.w<=w&&r.y+r.h<=h);
   CHECK(i.x==r.x+1&&i.y==r.y+1&&i.w==iw&&i.h==ih&&r.w==iw+2&&r.h==ih+2);
   for(int y=r.y;y<r.y+r.h;++y)for(int x=r.x;x<r.x+r.w;++x){const auto at=std::size_t(y)*w+x;CHECK(!occupied[at]);occupied[at]=true;}
  }
  shelf.clear();auto first=shelf.insert(w-2,h-2);CHECK(first&&first->outer.x==0&&first->outer.y==0&&!shelf.insert(1,1));
 }
 for(int bad:{-1,0,1,2,4097,std::numeric_limits<int>::max()}){study_atlas::Shelf shelf(bad,16);CHECK(!shelf.valid()&&!shelf.insert(1,1));}
 study_atlas::Shelf shelf(4096,4096);CHECK(!shelf.insert(std::numeric_limits<int>::max(),1)&&!shelf.insert(0,1));CHECK(shelf.insert(4094,4094));
 std::printf("Shelf: %u accepted, %u rejected; bounds, disjoint padded regions, transactional failure, clear and integer limits passed\n",accepted,rejected);
}
