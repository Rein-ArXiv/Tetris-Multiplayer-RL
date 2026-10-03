#include "renderer/image_geometry.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using namespace study_image_quad;
bool near(double a,double b){return std::abs(a-b)<0.0001;}
int main(){Draw d;auto v=make_vertices(d);CHECK(v&&v->size()==6);
 CHECK(near((*v)[0].x,-.875)&&near((*v)[0].y,5.0/6)&&(*v)[0].u==0&&(*v)[0].v==0);
 CHECK(near((*v)[2].x,-.575)&&near((*v)[2].y,1-68./120)&&(*v)[2].u==1&&(*v)[2].v==1);
 CHECK((*v)[0].x==(*v)[3].x&&(*v)[2].x==(*v)[4].x);
 d.rect={10,20,20,10};d.angle_degrees=90;d.pivot_x=d.pivot_y=0;
 v=make_vertices(d);CHECK(v&&near(((*v)[5].x+1)*160,10)&&near((1-(*v)[5].y)*120,40));
 CHECK(near(((*v)[1].x+1)*160,0)&&near((1-(*v)[1].y)*120,20));
 for(float angle:{-180.f,-90.f,0.f,27.f,90.f,179.f})for(float px:{0.f,.5f,1.f})for(float py:{0.f,.5f,1.f}){
  d={};d.angle_degrees=angle;d.pivot_x=px;d.pivot_y=py;auto a=make_vertices(d);CHECK(a);
  const double cx=d.rect.x+px*d.rect.width,cy=d.rect.y+py*d.rect.height;
  for(int i=0;i<6;++i){double x=((*a)[i].x+1)*160.,y=(1-(*a)[i].y)*120.;
   double localx=((*a)[i].u-px)*d.rect.width,localy=((*a)[i].v-py)*d.rect.height;
   CHECK(std::abs((x-cx)*(x-cx)+(y-cy)*(y-cy)-(localx*localx+localy*localy))<.02);
  }
  d.angle_degrees+=720;auto b=make_vertices(d);CHECK(b);
  for(int i=0;i<6;++i)CHECK(near((*a)[i].x,(*b)[i].x)&&near((*a)[i].y,(*b)[i].y));
 }
 d={};d.uv={.75f,.5f,.25f,.5f};d.tint={.5f,.25f,1,.4f};v=make_vertices(d);
 CHECK(v&&(*v)[0].u==.75f&&(*v)[5].u==.25f&&(*v)[1].v==.5f&&(*v)[2].a==.4f&&(*v)[4].g==.25f);
 for(int field=0;field<15;++field)for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
  d={};float* fields[]={&d.rect.x,&d.rect.y,&d.rect.width,&d.rect.height,&d.uv.u0,&d.uv.v0,&d.uv.u1,&d.uv.v1,&d.tint.r,&d.tint.g,&d.tint.b,&d.tint.a,&d.pivot_x,&d.pivot_y,&d.angle_degrees};*fields[field]=invalid;CHECK(!make_vertices(d));
 }
 d={};d.rect.width=0;CHECK(!make_vertices(d));d={};d.rect.height=-1;CHECK(!make_vertices(d));
 d={};d.tint.a=1.01f;CHECK(!make_vertices(d));d={};d.uv.u0=-.1f;CHECK(!make_vertices(d));d={};d.pivot_y=2;CHECK(!make_vertices(d));
 d={};CHECK(!make_vertices(d,0,240)&&!make_vertices(d,320,std::numeric_limits<float>::infinity()));
 d.angle_degrees=(std::numeric_limits<float>::max)();v=make_vertices(d);CHECK(v);for(auto x:*v)CHECK(std::isfinite(x.x)&&std::isfinite(x.y));
 d={};d.rect.x=(std::numeric_limits<float>::max)();CHECK(!make_vertices(d,1,240));
 d={};d.rect.x=-40;v=make_vertices(d);CHECK(v&&(*v)[0].x < -1);
 std::puts("Image geometry: corner pivot, 54 rotation/pivot cases, periodicity, UV reversal/constant sample, tint, 45 nonfinite fields and float range passed");
}
