#include "simulation/piece.h"
#include "renderer/piece_geometry.h"
#include <climits>
#include <cstdio>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"piece line %d: %s\n",__LINE__,#x);return 1;}}while(false)
using namespace study_piece;
static bool same(const BoardCells& a,const BoardCells& b){
    for(std::size_t i=0;i<a.size();++i)if(a[i].row!=b[i].row||a[i].column!=b[i].column)return false;
    return true;
}
int main(){
    Piece p{t_shape,{4,3}};
    const auto cells=to_board(p);
    const BoardCells expected{{{4,4},{5,3},{5,4},{5,5}}};
    CHECK(cells&&same(*cells,expected));
    const auto b=local_bounds(t_shape);CHECK(b.min_row==0&&b.max_row==1&&b.min_column==0&&b.max_column==2);
    auto copy=p;copy.origin={8,5};copy.local[0]={2,2};
    CHECK(p.origin.row==4&&p.local[0].row==0);
    p.origin={5,3};CHECK(same(*cells,expected));CHECK(to_board(p)->at(0).row==5);
    // Preserve relative cell differences and order over every edge neighborhood.
    for(int row=-4;row<=22;++row)for(int col=-4;col<=12;++col){
        p={t_shape,{row,col}};const auto positions=to_board(p);CHECK(positions);
        std::size_t visible=0;
        const auto mesh=study_piece_view::make_visible_mesh(*positions);
        for(std::size_t i=0;i<4;++i){
            CHECK((*positions)[i].row==row+t_shape[i].row&&(*positions)[i].column==col+t_shape[i].column);
            for(std::size_t j=0;j<4;++j){
                CHECK((*positions)[i].row-(*positions)[j].row==t_shape[i].row-t_shape[j].row);
                CHECK((*positions)[i].column-(*positions)[j].column==t_shape[i].column-t_shape[j].column);
            }
            const auto c=(*positions)[i];
            if(c.row<0||c.row>=20||c.column<0||c.column>=10)continue;
            const double x=110+10*c.column,y=20+10*c.row;
            const double points[6][2]={{x,y+9},{x+9,y+9},{x+9,y},{x,y+9},{x+9,y},{x,y}};
            for(std::size_t k=0;k<6;++k){
                CHECK(std::abs(mesh.vertices[visible*6+k].x-(points[k][0]/160-1))<1e-6);
                CHECK(std::abs(mesh.vertices[visible*6+k].y-(1-points[k][1]/120))<1e-6);
            }
            ++visible;
        }
        CHECK(mesh.count==visible*6&&mesh.count<=24);
        CHECK(p.origin.row==row && p.local[0].column==1);
    }
    // Rebase local coordinates and compensate the origin: same board cells.
    p={t_shape,{4,3}};auto rebased=p;
    for(auto& cell:rebased.local){cell.row-=2;cell.column+=3;}
    rebased.origin.row+=2;rebased.origin.column-=3;
    CHECK(same(*to_board(p),*to_board(rebased)));
    const auto shifted=local_bounds(rebased.local);
    CHECK(shifted.min_row==-2&&shifted.max_row==-1&&shifted.min_column==3&&shifted.max_column==5);
    p.origin={INT_MAX,3};CHECK(!to_board(p));
    p.origin={0,INT_MAX};CHECK(!to_board(p));
    p.origin={INT_MAX-1,INT_MAX-2};CHECK(to_board(p));
    CHECK(study_piece_view::make_visible_mesh(*to_board(p)).count==0);
    p.local[0]={-1,0};p.origin={INT_MIN,0};CHECK(!to_board(p));
    p.local[0]={0,-1};p.origin={0,INT_MIN};CHECK(!to_board(p));
    p={t_shape,{INT_MIN,INT_MIN}};CHECK(to_board(p));
    // Arbitrary lists still translate; this API does not certify a shape.
    p.local={{{INT_MIN,INT_MIN},{INT_MAX,INT_MAX},{0,0},{0,0}}};p.origin={0,0};
    CHECK(to_board(p));const auto extremes=local_bounds(p.local);
    CHECK(extremes.min_row==INT_MIN&&extremes.max_column==INT_MAX);
    std::puts("piece: translation/order/rebase/copies/extrema/overflow and visible-prefix geometry passed");
}
