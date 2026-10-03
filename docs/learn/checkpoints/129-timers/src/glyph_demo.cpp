#include "text/line.h"
#include <cstdio>
#include <fstream>
int main(int argc,char** argv) {
    const auto path=argc>1 ? argv[1] : "assets/NanumGothic.ttf";
    study_font::Font font;
    if(!font.load_trusted(std::filesystem::u8path(path))) {
        std::fputs("Cannot load the trusted packaged font\n",stderr);return 1;
    }
    const auto m=font.metrics(32);
    const auto label=study_labels::decode(u8"A g 가");
    const auto line=label ? study_font::prepare_line(font,*label,32) : std::nullopt;
    if(!m || !line) return 1;
    std::printf("height=32 scale=%.6f ascent=%.3f descent=%.3f\n",m->scale,m->ascent,m->descent);
    for(std::size_t i=0;i<line->count;++i) {
        const auto& p=line->items[i];const auto& g=p.glyph;
        std::printf("U+%04X glyph=%d missing=%d pen=%.3f advance=%.3f box=%dx%d offset=(%d,%d)\n",
            unsigned(label->scalars[i]),g.index,g.missing,p.pen_x,g.advance,g.width,g.height,g.xoff,g.yoff);
    }
    // Optional diagnostic image: coverage values displayed as grayscale.
    if(argc>2) {
        const auto g=font.rasterize(U'가',32);if(!g || g->coverage.empty()) return 1;
        std::ofstream out(std::filesystem::u8path(argv[2]),std::ios::binary);
        out<<"P5\n"<<g->width<<' '<<g->height<<"\n255\n";
        out.write(reinterpret_cast<const char*>(g->coverage.data()),std::streamsize(g->coverage.size()));
        if(!out) return 1;
    }
}
