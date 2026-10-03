#include "renderer/board_geometry.h"
#include "src/board_example.h"
#include <cstdio>
#include <climits>
#include <limits>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"board line %d: %s\n",__LINE__,#x);return 1;} } while(false)
using namespace study_board;
using namespace study_grid;
static bool close(double a,double b){return std::abs(a-b)<0.000001;}
int main(){
    const auto example=make_example_board();
    const auto mesh=make_mesh(example);
    CHECK(mesh.empty_vertices==1182 && mesh.vertices.size()==1200);
    CHECK(sizeof(mesh.vertices)==9600);
    CHECK(!cell_rect(-1,0) && !cell_rect(0,-1));
    CHECK(!cell_rect(20,0) && !cell_rect(0,10));
    CHECK(!cell_rect(INT_MAX,0) && !cell_rect(0,INT_MIN));
    const auto r=cell_rect(1,2);
    CHECK(r && r->left==130 && r->top==30 && r->right==139 && r->bottom==39);
    for(const auto p:{std::array<double,2>{110,20},{119.5,29.5},{209.5,219.5}}){
        const auto hit=cell_at(p[0],p[1]);CHECK(hit);
        CHECK(hit->column==static_cast<int>((p[0]-110)/10));
        CHECK(hit->row==static_cast<int>((p[1]-20)/10));
    }
    CHECK(!cell_at(std::nextafter(110.0,0.0),20));
    CHECK(!cell_at(110,std::nextafter(20.0,0.0)));
    CHECK(!cell_at(210,20) && !cell_at(110,220));
    const auto last=cell_at(std::nextafter(210.0,0.0),std::nextafter(220.0,0.0));
    CHECK(last && last->row==19 && last->column==9);
    for(double bad:{std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max()}){
        CHECK(!cell_at(bad,20) && !cell_at(110,bad));
    }
    for(int pattern=0;pattern<4;++pattern){
        Grid g;
        for(int row=0;row<20;++row)for(int col=0;col<10;++col){
            const bool fill=pattern==1 || (pattern==2 && (row+col)%2==1) ||
                            (pattern==3 && (row==1 && col==2));
            CHECK(g.set(row,col,fill?Cell::filled:Cell::empty));
        }
        const auto before=g.cells();
        const auto m=make_mesh(g);
        CHECK(g.cells()==before); // Rendering data creation must not mutate rules.
        const std::size_t expected_empty[]={1200,0,600,1194};
        CHECK(m.empty_vertices==expected_empty[pattern]);
        std::size_t write=0;
        for(int filled=0;filled<2;++filled)for(int row=0;row<20;++row)for(int col=0;col<10;++col){
            if((*g.get(row,col)==Cell::filled)!=static_cast<bool>(filled))continue;
            const double left=110+10*col,top=20+10*row;
            const double points[6][2]={{left,top+9},{left+9,top+9},{left+9,top},
                                        {left,top+9},{left+9,top},{left,top}};
            for(const auto& p:points){
                CHECK(close(m.vertices[write].x,p[0]/160-1));
                CHECK(close(m.vertices[write].y,1-p[1]/120));
                ++write;
            }
            const auto& a=m.vertices[write-6];const auto& b=m.vertices[write-5];const auto& c=m.vertices[write-4];
            CHECK((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)>0);
            const auto hit=cell_at(left+4.5,top+4.5);CHECK(hit&&hit->row==row&&hit->column==col);
        }
        CHECK(write==1200);
        const auto old_count=m.empty_vertices;
        CHECK(g.set(0,0,Cell::filled));
        CHECK(m.empty_vertices==old_count); // Snapshot, not a live view.
        CHECK(make_mesh(g).empty_vertices==old_count-(before[0]==Cell::empty?6:0));
    }
    std::puts("board: all vertices, CCW, ranges, gaps, extreme input, snapshots and unchanged Grid passed");
}
