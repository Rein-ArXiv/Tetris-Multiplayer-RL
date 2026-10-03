#include "renderer/rounded_distance.h"
#include <cstdio>
int main(){for(double x:{0.,3.,4.})for(double y:{0.,3.}){auto d=study_rounded_distance::signed_distance(x,y,4,3,1);if(!d)return 1;auto a=study_rounded_distance::opacity(*d);if(!a)return 1;std::printf("point(%g,%g) distance=%.6f mask=%.6f\n",x,y,*d,*a);}}
