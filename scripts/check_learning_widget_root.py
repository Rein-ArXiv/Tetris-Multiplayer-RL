"""Root widget layout regressions using extracted production functions."""
from pathlib import Path
import subprocess
from check_learning_utf8 import cut
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/69-widget-state-check'
HEAD=r'''#include <cstdint>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
struct Color{int r,g,b,a;};
constexpr Color WHITE{255,255,255,255},kBtnHighlight{1,2,3,4},kBtnHoverBg{4,3,2,1};
int mx=0,my=0,measured=40,draws=0; bool pressed=true;
std::vector<std::string> labels;
int platform_mouse_x(){return mx;} int platform_mouse_y(){return my;}
bool platform_mouse_pressed(int){return pressed;}
int measure_text(const char*,int){return measured;}
void draw_rect(int,int,int w,int h,Color){if(w<0||h<0)std::abort();++draws;}
void draw_text(const char* t,int,int,int,Color){++draws;labels.emplace_back(t);}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d %s\n",__LINE__,#e);std::exit(1);}}while(false)
'''
def compile_run(name,source,expect_ub=False):
 p=OUT/(name+'.cpp');p.write_text(source);b=OUT/name
 run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined','-fno-sanitize-recover=all',str(p),'-o',str(b)])
 r=subprocess.run([str(b)],text=True,capture_output=True)
 (OUT/(name+'.log')).write_text(r.stdout+r.stderr)
 if expect_ub: assert r.returncode!=0 and 'signed integer overflow' in r.stderr
 else: assert r.returncode==0,r.stdout+r.stderr
 print(name+': '+('signed overflow reproduced' if expect_ub else r.stdout.strip()))
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 before=OUT/'before-src-gui.cpp'
 if before.exists():
  s=before.read_text();pieces=HEAD+cut(s,'bool gui_hover_rect(')+cut(s,'bool gui_checkbox(')
  compile_run('checkbox-before',pieces+'int main(){measured=2147483647;return gui_checkbox(0,0,26,"label",false,false);}',True)
  pieces=HEAD+cut(s,'bool gui_hover_rect(')+cut(s,'int gui_value_selector(')
  compile_run('selector-before',pieces+'int main(){mx=0;my=1;CHECK(gui_value_selector(0,0,20,26,"label",false)==-1);std::puts("overlapping arrow regions accepted");}')
 s=(ROOT/'src/gui.cpp').read_text()
 pieces=HEAD+cut(s,'static bool fits_ui_coordinate(')+cut(s,'bool gui_hover_rect(')+cut(s,'bool gui_checkbox(')+cut(s,'int gui_value_selector(')
 compile_run('widgets-after',pieces+r'''
int main(){
 mx=60;my=5;measured=40;bool value=false;
 CHECK(gui_checkbox(0,0,26,"label",value,false)&&!value);
 mx=76;CHECK(!gui_checkbox(0,0,26,"label",false,false));
 measured=2147483647;draws=0;CHECK(!gui_checkbox(0,0,26,"label",false,false)&&draws==0);
 measured=40;draws=0;CHECK(!gui_checkbox(0,0,3,"label",true,false)&&draws==0);
 CHECK(!gui_checkbox(2147483647,0,26,"label",true,false)&&draws==0);
 CHECK(!gui_checkbox(0,2147483647,26,"label",true,false)&&draws==0);
 measured=-1;CHECK(!gui_checkbox(0,0,26,"label",false,false)&&draws==0);
 measured=10;mx=0;my=1;CHECK(!gui_value_selector(0,0,20,26,"label",false,true,true)&&draws==0);
 unsigned cases=0;
 for(int x=-3;x<124;++x)for(int y=-3;y<30;++y)for(bool left:{false,true})for(bool right:{false,true}){
   mx=x;my=y;labels.clear();draws=0;
   int result=gui_value_selector(0,0,120,26,"label",false,left,right);
   int expected=y>=0&&y<26 ? (left&&x>=0&&x<26?-1:(right&&x>=94&&x<120?1:0)) : 0;
   CHECK(result==expected);CHECK(draws==1+int(left)+int(right));
   for(auto& text:labels){CHECK(text!="<"||left);CHECK(text!=">"||right);}++cases;
 }
 for(int n:{(-2147483647-1),-1,0,1,6}){draws=0;CHECK(!gui_value_selector(0,0,120,n,"label",false,true,true)&&draws==0);}
 draws=0;CHECK(!gui_value_selector(2147483647,0,120,26,"label",false,true,true)&&draws==0);
 CHECK(!gui_value_selector(0,2147483647,120,26,"label",false,true,true)&&draws==0);
 measured=2147483647;CHECK(!gui_value_selector((-2147483647-1),0,120,26,"label",false,true,true)&&draws==0);
 measured=-1;CHECK(!gui_value_selector(0,0,120,26,"label",false,true,true)&&draws==0);
 std::printf("%u independent pointer/availability combinations; invalid geometry, wide arithmetic and checkbox label hit passed\n",cases);
}
''')
if __name__=='__main__': main()
