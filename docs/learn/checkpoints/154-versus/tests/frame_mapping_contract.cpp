#include "client/frame_mapping.h"
#include "client/menu_model.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"mapping line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using study_letterbox::Size;
using study_letterbox::Point;
struct Example {Size window,drawable;study_letterbox::Rect viewport;};
static study_pointer::Frame click(int x,int y){return {{study_pointer::Point{x,y}},{study_pointer::Point{x,y}},false,false};}
int main(){
 constexpr Example examples[]={{{640,480},{1280,960},{0,0,1280,960}},{{1000,400},{1500,600},{350,0,800,600}},{{400,1000},{600,1500},{0,525,600,450}},{{801,603},{801,603},{0,1,801,600}},{{803,601},{803,601},{1,0,801,601}},{{600,400},{1200,1200},{0,150,1200,900}}};
 constexpr Size logical{320,240};std::size_t comparisons=0,boundary=0;
 for(const auto& e:examples){
  study_ui::FrameMapping mapping;
  auto first=mapping.update(e.window,e.drawable,logical,click(0,0));CHECK(first.geometry_changed&&first.input.cancelled&&first.layout);
  const auto& v=first.layout->viewport;CHECK(v.x==e.viewport.x&&v.y==e.viewport.y&&v.width==e.viewport.width&&v.height==e.viewport.height);
  CHECK(study_letterbox::gl_y(*first.layout)==e.drawable.height-e.viewport.y-e.viewport.height);
  for(int y=-3;y<=e.window.height+3;y+=5)for(int x=-3;x<=e.window.width+3;x+=7){
   auto f=mapping.update(e.window,e.drawable,logical,click(x,y));CHECK(!f.geometry_changed&&!f.input.cancelled);
   // Exact integer inequalities, independent of floating inverse implementation.
   const std::int64_t dx=std::int64_t(x)*e.drawable.width-std::int64_t(e.viewport.x)*e.window.width;
   const std::int64_t dy=std::int64_t(y)*e.drawable.height-std::int64_t(e.viewport.y)*e.window.height;
   const std::int64_t width=std::int64_t(e.viewport.width)*e.window.width,height=std::int64_t(e.viewport.height)*e.window.height;
   const bool in=dx>=0&&dy>=0&&dx<width&&dy<height;
   CHECK(bool(f.input.press)==in);
   for(auto r:{study_menu::start_bounds,study_menu::decorations_bounds,study_menu::badge_bounds}) {
    const auto nx=dx*320,ny=dy*240;
    const auto left=std::int64_t(r.x)*width,right=std::int64_t(r.x+r.w)*width;
    const auto top=std::int64_t(r.y)*height,bottom=std::int64_t(r.y+r.h)*height;
    // Float rounding at exact projected widget edges is separately checked with
    // binary-exact fixtures. Never invent a tolerance that enlarges a hit area.
    if(nx==left||nx==right||ny==top||ny==bottom){++boundary;continue;}
    const bool expected=in&&nx>left&&nx<right&&ny>top&&ny<bottom;
    CHECK(study_ui::button(r,f.input).activated==expected);++comparisons;
   }
  }
  for(Point p:std::array<Point,3>{{{20.25,70.5},{151.125,90.25},{280.5,130.25}}}){
   auto window=study_letterbox::logical_to_window(*first.layout,p);CHECK(window);
   auto back=study_letterbox::window_to_logical(*first.layout,*window);CHECK(back);
   CHECK(std::abs(back->x-p.x)<1e-10&&std::abs(back->y-p.y)<1e-10);
  }
 }
 // Each size component independently changes the basis, including height-only changes.
 struct DimensionsCase{Size window,drawable,logical;};
 for(auto d:std::array<DimensionsCase,6>{{{{641,480},{640,480},{320,240}},{{640,481},{640,480},{320,240}},{{640,480},{641,480},{320,240}},{{640,480},{640,481},{320,240}},{{640,480},{640,480},{321,240}},{{640,480},{640,480},{320,241}}}}){
  study_ui::FrameMapping m;m.update({640,480},{640,480},{320,240},{});
  auto changed=m.update(d.window,d.drawable,d.logical,click(40,140));CHECK(changed.geometry_changed&&changed.input.cancelled&&changed.layout);
 }
 // Concrete cross-widget counterexample: reinterpreting the same old press
 // with a new viewport changes a decoration toggle into a start request.
 const auto raw=click(240,180);
 const auto before=study_letterbox::make_layout({640,480},{640,480},logical);
 const auto after=study_letterbox::make_layout({1200,617},{1200,617},logical);
 CHECK(study_ui::button(study_menu::decorations_bounds,study_ui::map_pointer(before,raw)).activated);
 CHECK(study_ui::button(study_menu::start_bounds,study_ui::map_pointer(after,raw)).activated);
 study_ui::FrameMapping guarded;guarded.update({640,480},{640,480},logical,{});
 const auto cancelled=guarded.update({1200,617},{1200,617},logical,raw);
 CHECK(cancelled.layout&&cancelled.input.cancelled&&!study_ui::button(study_menu::start_bounds,cancelled.input).activated);
 study_ui::FrameMapping mapping;mapping.update({640,480},{640,480},logical,{});
 // (40,140) hits start in the old 2x scale, but not after becoming 4x.
 auto old=mapping.update({640,480},{640,480},logical,click(40,140));CHECK(study_ui::button(study_menu::start_bounds,old.input).activated);
 auto resized=mapping.update({1280,960},{1280,960},logical,click(40,140));CHECK(resized.geometry_changed&&resized.layout&&resized.input.cancelled&&!resized.input.press);
 auto next=mapping.update({1280,960},{1280,960},logical,click(80,280));CHECK(!next.geometry_changed&&study_ui::button(study_menu::start_bounds,next.input).activated);
 auto dpi=mapping.update({1280,960},{2560,1920},logical,click(80,280));CHECK(dpi.geometry_changed&&dpi.input.cancelled);
 auto logical_change=mapping.update({1280,960},{2560,1920},{640,480},click(80,280));CHECK(logical_change.geometry_changed&&logical_change.input.cancelled);
 auto zero=mapping.update({0,0},{0,0},logical,click(0,0));CHECK(!zero.layout&&zero.geometry_changed&&!zero.input.press);
 auto zero_again=mapping.update({0,0},{0,0},logical,click(0,0));CHECK(!zero_again.layout&&!zero_again.geometry_changed&&!zero_again.input.press);
 auto restored=mapping.update({640,480},{640,480},logical,click(40,140));CHECK(restored.input.cancelled&&restored.layout);
 // Mouse cancellation leaves keyboard menu commands available.
 study_menu::Preferences prefs;const auto intent=study_menu::evaluate(prefs,study_menu::Focus::start,restored.input,{false,false,false,false,true,false});CHECK(intent.action==study_menu::Action::start);
 // Exact top/left included, right/bottom excluded at an exactly representable scale.
 mapping.update({640,480},{640,480},logical,{});
 for(const auto p:std::array<study_pointer::Point,4>{{{28,128},{156,128},{28,184},{155,183}}}) {
  const auto f=mapping.update({640,480},{640,480},logical,click(p.x,p.y));
  const bool expected=p.x<156&&p.y<184;CHECK(study_ui::button(study_menu::start_bounds,f.input).activated==expected);
 }
 std::printf("Frame mapping: %zu rational interior hit comparisons, %zu exact-edge exclusions; independent layouts, round trips, resize/DPI/invalid/restore and keyboard separation passed\n",comparisons,boundary);
}
