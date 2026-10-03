"""Validate ordered colour batching, actual GL pixels and baked root offsets."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/56-color-batch'
OUT=ROOT/'out/learning-checkpoints/56-color-batch-check'
def run(args,**kw):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',600),**kw)
 if r.returncode:raise RuntimeError(f'{args}\n{r.stdout}\n{r.stderr}')
 return r

def root_probe():
 text=(ROOT/'renderer/renderer.cpp').read_text()
 state=text[text.index('static int s_screen_w'):text.index('// ─── 셰이더')]
 functions=text[text.index('static void push_vertex'):text.index('// ─── 공개 API')]
 offset=text[text.index('void renderer_set_view_offset'):text.index('void renderer_end')]
 probe=r'''#include "renderer/gl_internal.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
struct Packet {GLuint texture;std::vector<float> vertices;};
static std::vector<Packet> packets;static std::vector<float> uploaded;static GLuint bound=0;
static void bind_buffer(GLenum,GLuint){}
static void data(GLenum,GLsizeiptr bytes,const void* ptr,GLenum usage){CHECK(usage==GL_STREAM_DRAW);auto p=static_cast<const float*>(ptr);uploaded.assign(p,p+bytes/sizeof(float));}
static void active(GLenum unit){CHECK(unit==GL_TEXTURE0);}
static void texture(GLenum,GLuint tex){bound=tex;}
static void array(GLuint){}
static void draw(GLenum mode,GLint first,GLsizei count){CHECK(mode==GL_TRIANGLES&&first==0&&count*14==static_cast<int>(uploaded.size()));packets.push_back({bound,uploaded});}
decltype(gl_BindBuffer) gl_BindBuffer=bind_buffer;
decltype(gl_BufferData) gl_BufferData=data;
decltype(gl_ActiveTexture) gl_ActiveTexture=active;
decltype(gl_BindTexture) gl_BindTexture=texture;
decltype(gl_BindVertexArray) gl_BindVertexArray=array;
decltype(gl_DrawArrays) gl_DrawArrays=draw;
'''+state+functions+offset+r'''
int main(){s_ready=true;s_screen_w=s_screen_h=1000;s_white=7;s_vao=2;s_vbo=3;
 glb_rect(7,10,20,5,6,0,0,1,1,{255,0,0,255},0,0);
 renderer_set_view_offset(100,200);CHECK(packets.empty());
 glb_rect(7,10,20,5,6,0,0,1,1,{0,255,0,255},0,0);
 const float x[]={1,3,3,1},y[]={2,2,4,4},u[]={0,1,1,0},v[]={0,0,1,1};
 renderer_set_view_offset(-10,30);glb_quad(7,x,y,u,v,{0,0,255,255},0);
 CHECK(packets.empty());glb_flush();CHECK(packets.size()==1&&packets[0].texture==7);
 const auto& vertices=packets[0].vertices;CHECK(vertices.size()==18*14);
 const int order[]={0,1,2,0,2,3};
 const float rx[]={10,15,15,10},ry[]={20,20,26,26};
 for(int i=0;i<6;++i){int k=order[i];CHECK(vertices[i*14]==rx[k]&&vertices[i*14+1]==ry[k]);CHECK(vertices[(i+6)*14]==rx[k]+100&&vertices[(i+6)*14+1]==ry[k]+200);CHECK(vertices[(i+12)*14]==x[k]-10&&vertices[(i+12)*14+1]==y[k]+30);}
 glb_rect(7,20,20,4,4,0,0,1,1,{255,255,255,255},0,0);
 glb_rect(8,20,20,4,4,0,0,1,1,{255,255,255,255},0,0);
 CHECK(packets.size()==2&&packets.back().texture==7);glb_flush();CHECK(packets.size()==3&&packets.back().texture==8);
 glb_flush();CHECK(packets.size()==3);std::puts("Actual root rectangle/quad offsets, ordered vertices and texture boundaries passed");
}
'''
 p=OUT/'root-batch-probe.cpp';p.write_text(probe)
 run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/'root-probe')]);print(run([str(OUT/'root-probe')]).stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'55-screen-state'
 for p in previous.rglob('*'):
  rel=p.relative_to(previous)
  if p.is_file() and str(rel)not in {'CMakeLists.txt','README.md','src/main.cpp'}:assert p.read_bytes()==(SOURCE/rel).read_bytes(),rel
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  r=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);assert 'warning:'not in r.stdout+r.stderr
  r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
  output=run([str(build/'batch_demo')]).stdout;(OUT/f'demo-{backend}.log').write_text(output);assert '1356 vertices x 24 bytes = 32544 bytes' in output
  if backend=='SDL':
   glenv={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'}
   out=run([str(build/'batch_real')],env=glenv).stdout;(OUT/'pixels.log').write_text(out);print(out.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE)]
 run([*flags,str(SOURCE/'tests/batch_contract.cpp'),'-o',str(OUT/'batch-sanitized')]);print(run([str(OUT/'batch-sanitized')]).stdout.strip(),flush=True)
 run([*flags,str(SOURCE/'tests/batch_device_contract.cpp'),str(SOURCE/'renderer/program.cpp'),str(SOURCE/'renderer/shader.cpp'),'-o',str(OUT/'device-sanitized')]);print(run([str(OUT/'device-sanitized')]).stdout.strip(),flush=True)
 for name,before,after in [('missing_clear','used_ = 0;','used_ = used_;'),('opaque_alpha','color.a != 1.0f','false'),('reverse_order','points[i].x, points[i].y','points[count-1-i].x, points[count-1-i].y')]:
  folder=OUT/name;p=folder/'renderer/color_batch.h';p.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/'renderer/color_batch.h').read_text();assert before in text
  # Mutate clear only, not the member initializer.
  p.write_text(text.replace(before,after,1))
  run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),'-I'+str(SOURCE),str(SOURCE/'tests/batch_contract.cpp'),'-o',str(folder/'check')])
  r=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert r.returncode==1 and 'CHECK failed' in r.stderr,name;(folder/'result.log').write_text(r.stderr)
 root_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','--build',str(build),'-j3'])
 r=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('CPU/GL batch contracts, framebuffer equivalence, root baked-offset probe, unchanged goldens, UBSan and three Release mutations passed.',flush=True)
if __name__=='__main__':main()
