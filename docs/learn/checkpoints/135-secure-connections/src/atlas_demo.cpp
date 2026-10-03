#include "text/shelf.h"
#include <cstdio>
int main(){
 study_atlas::Shelf page(16,12);
 for(auto size:{study_atlas::Shelf::Rect{0,0,5,3},{0,0,6,4},{0,0,3,2},{0,0,12,2},{0,0,1,1}}){
  const auto p=page.insert(size.w,size.h);
  if(!p){std::printf("ink %dx%d: no room (cursor unchanged)\n",size.w,size.h);continue;}
  std::printf("ink %dx%d: outer=(%d,%d,%d,%d) ink=(%d,%d) UV=(%.4f,%.4f)-(%.4f,%.4f)\n",
   size.w,size.h,p->outer.x,p->outer.y,p->outer.w,p->outer.h,p->ink.x,p->ink.y,
   p->ink.x/16.,p->ink.y/12.,(p->ink.x+p->ink.w)/16.,(p->ink.y+p->ink.h)/12.);
 }
}
