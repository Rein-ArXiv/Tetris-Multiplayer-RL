#include "text/paragraph.h"
#include <cstdio>
int main(int argc,char** argv){
 study_font::Font font;if(argc!=2||!font.load_trusted(argv[1]))return 1;
 const auto label=study_labels::decode("AV\nA V\n");
 const auto p=study_font::prepare_paragraph(font,*label,32);if(!p)return 1;
 std::printf("lines=%zu width=%.3f height=%.3f line_advance=%.3f\n",p->layout.lines,p->layout.width,p->layout.height,p->layout.line_advance);
 for(std::size_t i=0;i<p->count;++i)std::printf("glyph=%d pen=%.3f baseline=%.3f advance=%.3f\n",p->items[i].glyph.index,p->items[i].pen_x,p->items[i].baseline,p->items[i].glyph.advance);
 if(p->layout.ink){const auto b=*p->layout.ink;std::printf("bitmap bounds=(%.3f,%.3f) %.3fx%.3f\n",b.x,b.y,b.w,b.h);}
}
