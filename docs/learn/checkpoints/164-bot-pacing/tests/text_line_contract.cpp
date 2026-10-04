#include "images_fake.h"
#include "renderer/text_line.h"
#include <limits>
static study_font::Line sample() {
 study_font::Line line{};line.count=3;
 for(std::size_t i=0;i<line.count;++i) {
  auto& g=line.items[i].glyph;g.width=2;g.height=2;g.advance=3;g.coverage={0,64,128,255};
  line.items[i].pen_x=float(i)*3;
 }
 return line;
}
int main() {
 auto gl=texture_api();study_image::ImageStore images(gl);auto line=sample();
 // Force registration of the second texture to fail after one successful upload.
 study_handles::next_stamp=std::numeric_limits<std::uint32_t>::max();
 {
  study_text::TextLine text(images);CHECK(!text.init(line));CHECK(images.count()==0&&f.live.empty());
  study_handles::next_stamp=1;CHECK(text.init(line)&&images.count()==3);
  CHECK(!text.init(line)&&images.count()==3);text.reset();CHECK(images.count()==0&&f.live.empty());
  f.upload_error=true;CHECK(!text.init(line)&&images.count()==0&&f.live.empty());f.upload_error=false;
  CHECK(text.init(line)&&images.count()==3);
 }
 CHECK(images.count()==0&&f.live.empty()&&f.gens==f.deletes);
 study_font::Line empty{};empty.count=1;empty.items[0].glyph.advance=9;
 {study_text::TextLine text(images);CHECK(text.init(empty)&&images.count()==0);}
 std::puts("TextLine: partial upload rollback, retry, reset/destruction, spaces passed");
}
