#include "renderer/image_geometry.h"
#include <cstdio>
int main(){study_image_quad::Draw d;d.rect={10,20,20,10};d.pivot_x=d.pivot_y=0;d.angle_degrees=90;
 auto v=study_image_quad::make_vertices(d);if(!v)return 1;
 std::printf("rotated top-right logical=(%.1f,%.1f), UV=(%.1f,%.1f)\n",((*v)[5].x+1)*160,(1-(*v)[5].y)*120,(*v)[5].u,(*v)[5].v);
}
