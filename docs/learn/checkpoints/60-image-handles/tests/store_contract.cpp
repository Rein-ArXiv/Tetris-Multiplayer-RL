#include "images_fake.h"
#include "renderer/image_store.h"
#include "renderer/badge_pixels.h"
int main(int argc,char** argv){CHECK(argc==2);const auto gl=texture_api();
 {study_image::ImageStore a(gl),b(gl);int w=99,h=88;
  CHECK(!a.size(0,w,h)&&w==99&&h==88&&!a.unload(0));
  auto one=a.load(std::filesystem::u8path(argv[1]));CHECK(one&&a.count()==1&&a.size(one,w,h)&&w==8&&h==8);
  auto two=a.create((study_texture::RgbaView{study_texture::badge_pixels.data(),study_texture::badge_pixels.size(),8,8}));CHECK(two&&two!=one&&a.count()==2&&f.live.size()==2);
  auto foreign=b.create((study_texture::RgbaView{study_texture::badge_pixels.data(),study_texture::badge_pixels.size(),8,8}));CHECK(foreign&&!a.size(foreign,w,h)&&!b.unload(one));
  CHECK(!a.load("does-not-exist-study-image.png")&&a.count()==2);
  CHECK(a.unload(one)&&!a.unload(one)&&f.live.size()==2);
  auto replacement=a.create((study_texture::RgbaView{study_texture::badge_pixels.data(),study_texture::badge_pixels.size(),8,8}));CHECK(replacement&&replacement!=one);
  w=99;h=88;CHECK(!a.size(one,w,h)&&w==99&&h==88&&!a.unload(one));
  CHECK(a.size(replacement,w,h)&&w==8&&a.count()==2);
  const auto size=a.count(),live=f.live.size();f.upload_error=true;
  CHECK(!a.create((study_texture::RgbaView{study_texture::badge_pixels.data(),study_texture::badge_pixels.size(),8,8}))&&a.count()==size&&f.live.size()==live);f.upload_error=false;
  CHECK(!a.create({nullptr,0,8,8})&&a.count()==size);
  a.clear();CHECK(a.count()==0&&!a.size(two,w,h)&&f.live.size()==1);a.clear();
  auto new_one=a.create((study_texture::RgbaView{study_texture::badge_pixels.data(),study_texture::badge_pixels.size(),8,8}));CHECK(new_one&&!a.size(replacement,w,h));
  study_handles::next_stamp=0; // Isolated exhaustion fault injection.
  const auto old_live=f.live.size();CHECK(!a.create((study_texture::RgbaView{study_texture::badge_pixels.data(),study_texture::badge_pixels.size(),8,8}))&&f.live.size()==old_live&&a.size(new_one,w,h));
 }
 CHECK(f.live.empty()&&f.gens==f.deletes);std::puts("ImageStore: file/create/multiple owners, failures, stale rejection, size preservation, clear/destruction and exhausted registration cleanup passed");
}
