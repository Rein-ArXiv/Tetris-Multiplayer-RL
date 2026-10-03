#include "text/line.h"
#include "font_reference.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed: %s line %d\n",#e,__LINE__);std::exit(1);}}while(false)
static bool close(float a,float b){return std::abs(a-b)<0.0001f;}
int main(int argc,char** argv) {
    CHECK(argc==2);study_font::Font font;
    CHECK(!font.loaded()&&!font.metrics(32)&&!font.rasterize(U'A',32));
    CHECK(font.load_trusted(std::filesystem::u8path(argv[1])));
    std::size_t nonempty=0, fractional=0;
    for(float height:{1.f,16.f,32.f,128.f}) {
        const auto m=font.metrics(height);CHECK(m);
        const float scale=height/float(font_ascent-font_descent);
        CHECK(close(m->scale,scale)&&close(m->ascent-m->descent,height));
        CHECK(close(m->ascent,font_ascent*scale)&&close(m->descent,font_descent*scale));
        CHECK(close(m->line_gap,font_gap*scale));
        for(const auto& ref:font_reference) {
            const auto g=font.rasterize(char32_t(ref.cp),height);CHECK(g);
            CHECK(g->index==ref.index&&g->missing==(ref.index==0));
            CHECK(close(g->advance,ref.advance*scale)&&close(g->left_bearing,ref.lsb*scale));
            const int left=int(std::floor(ref.x0*scale)),top=int(std::floor(-ref.y1*scale));
            const int right=int(std::ceil(ref.x1*scale)),bottom=int(std::ceil(-ref.y0*scale));
            CHECK(g->xoff==left&&g->yoff==top&&g->width==right-left&&g->height==bottom-top);
            CHECK(g->coverage.size()==std::size_t(g->width)*std::size_t(g->height));
            if(!g->coverage.empty()) {
                ++nonempty;
                CHECK(*std::max_element(g->coverage.begin(),g->coverage.end())>0);
                fractional+=std::count_if(g->coverage.begin(),g->coverage.end(),[](auto v){return v>0&&v<255;});
            }
            const auto rgba=study_font::white_rgba(*g);CHECK(rgba.size()==4*g->coverage.size());
            for(std::size_t i=0;i<g->coverage.size();++i) {
                CHECK(rgba[4*i]==255&&rgba[4*i+1]==255&&rgba[4*i+2]==255);
                CHECK(rgba[4*i+3]==g->coverage[i]);
            }
        }
    }
    CHECK(nonempty>30&&fractional>100);
    const auto space=font.rasterize(U' ',32);CHECK(space&&space->coverage.empty()&&space->advance>0);
    const auto g=font.rasterize(U'g',32);CHECK(g&&g->yoff+g->height>0); // descender below baseline
    for(float bad:{0.f,-1.f,129.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        CHECK(!font.metrics(bad)&&!font.rasterize(U'A',bad));
    CHECK(!font.rasterize(char32_t(0xD800),32)&&!font.rasterize(char32_t(0x110000),32));
    const auto labels=study_labels::decode(u8"A 가");CHECK(labels);
    const auto line=study_font::prepare_line(font,*labels,32);CHECK(line&&line->count==3);
    CHECK(line->items[0].pen_x==0);
    CHECK(close(line->items[2].pen_x,line->items[0].glyph.advance+space->advance));
    CHECK(close(line->advance,line->items[2].pen_x+line->items[2].glyph.advance));
    auto too_many=*labels;too_many.count=17;CHECK(!study_font::prepare_line(font,too_many,32));
    const auto newline=study_labels::decode("A\n");CHECK(newline&&!study_font::prepare_line(font,*newline,32));
    CHECK(study_font::prepare_line(font,study_labels::Label{},32)->count==0);
    // Failure must not invalidate the old borrowed font bytes.
    CHECK(!font.load_trusted(std::filesystem::u8path(argv[1]).parent_path()/"missing-font.ttf"));
    CHECK(font.rasterize(U'g',32)->coverage==g->coverage);
    study_font::Font moved=std::move(font);CHECK(moved.loaded()&&!font.loaded());
    CHECK(moved.rasterize(U'g',32)->coverage==g->coverage);
    study_font::Font assigned;assigned=std::move(moved);CHECK(!moved.loaded());
    CHECK(assigned.rasterize(U'g',32)->coverage==g->coverage);
    const auto owned=assigned.rasterize(U'g',32);assigned=study_font::Font{};
    CHECK(!assigned.loaded()&&owned->coverage==g->coverage);
    std::printf("56 glyph cases, %zu nonempty masks, %zu fractional samples; metrics, ownership and labels passed\n",nonempty,fractional);
}
