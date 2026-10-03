#include "renderer/rounded_distance.h"
#include "renderer/rounded_geometry.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
// Independent boundary oracle: distance to straight segments and quarter-circle.
static double boundary(double x,double y,double hw,double hh,double r){
 x=std::abs(x);y=std::abs(y);
 const double cx=hw-r,cy=hh-r;
 double d=(std::min)(std::hypot(x-(std::clamp)(x,0.,cx),y-hh),std::hypot(x-hw,y-(std::clamp)(y,0.,cy)));
 d=(std::min)(d,(std::min)(std::hypot(x-hw,y-cy),std::hypot(x-cx,y-hh)));
 const double angle=std::atan2(y-cy,x-cx);
 if(angle>=0 && angle<=1.5707963267948966)
  d=(std::min)(d,std::abs(std::hypot(x-cx,y-cy)-r));
 const bool inside=x<=hw && y<=hh && (x<=cx || y<=cy || std::hypot(x-cx,y-cy)<=r);
 return inside ? -d : d;
}
int main(){using study_rounded_distance::signed_distance;using study_rounded_distance::opacity;
 CHECK(*signed_distance(0,0,4,3,1)==-3);
 CHECK(*signed_distance(4,0,4,3,1)==0);
 CHECK(std::abs(*signed_distance(4,3,4,3,1)-(std::sqrt(2.)-1))<1e-12);
 std::size_t cases=0;
 for(double hw:{2.,4.,12.})for(double hh:{2.,3.,8.})for(double ratio:{0.,.1,.5,1.}){
  const double r=(std::min)(hw,hh)*ratio;
  for(int yi=-40;yi<=40;++yi)for(int xi=-40;xi<=40;++xi){
   const double x=xi*.4,y=yi*.4;const auto d=signed_distance(x,y,hw,hh,r);CHECK(d);
   CHECK(std::abs(*d-boundary(x,y,hw,hh,r))<1e-10);
   CHECK(*d==*signed_distance(-x,-y,hw,hh,r));++cases;
  }
 }
 CHECK(*opacity(-.5)==1 && *opacity(0)==.5 && *opacity(.5)==0);
 CHECK(*opacity(-.25)==.84375 && *opacity(.25)==.15625);
 double prev=1;
 for(int i=-100;i<=100;++i){double a=*opacity(i*.01);CHECK(a>=0&&a<=1&&a<=prev);prev=a;}
 for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
  CHECK(!signed_distance(bad,0,4,3,1));CHECK(!signed_distance(0,bad,4,3,1));
  CHECK(!signed_distance(0,0,bad,3,1));CHECK(!signed_distance(0,0,4,bad,1));CHECK(!signed_distance(0,0,4,3,bad));
  CHECK(!opacity(bad));CHECK(!opacity(0,bad));
 }
 CHECK(!signed_distance(0,0,0,3,0)&&!signed_distance(0,0,4,3,-1)&&!signed_distance(0,0,4,3,4));
 CHECK(!signed_distance(1e6+1,0,4,3,1)&&!opacity(0,0)&&!opacity(0,-1));
 study_rounded::Draw draw;draw.radius=12;draw.image.angle_degrees=37;draw.image.uv={1,0,0,1};
 const auto vertices=study_rounded::make_vertices(draw);const auto image=study_image_quad::make_vertices(draw.image);CHECK(vertices&&image);
 const float xs[]={-24,-24,24,-24,24,24},ys[]={-24,24,24,-24,24,-24};
 for(int i=0;i<6;++i){const auto& v=(*vertices)[i];CHECK(v.x==(*image)[i].x&&v.y==(*image)[i].y&&v.u==(*image)[i].u);
  CHECK(v.local_x==xs[i]&&v.local_y==ys[i]&&v.half_width==24&&v.half_height==24&&v.radius==12);}
 for(float radius:{-1.f,25.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){draw.radius=radius;CHECK(!study_rounded::make_vertices(draw));}
 draw.radius=24;CHECK(study_rounded::make_vertices(draw));draw.image.rect.width=1e6f+1;CHECK(!study_rounded::make_vertices(draw));
 draw.image.rect={20,20,1e6f,1e6f};draw.radius=5e5f;CHECK(study_rounded::make_vertices(draw));
 draw.radius=0;draw.image.rect.width=0;CHECK(!study_rounded::make_vertices(draw));
 std::printf("Rounded SDF: %zu boundary-oracle cases, sign/symmetry, smooth mask, validation and local/UV independence passed\n",cases);
}
