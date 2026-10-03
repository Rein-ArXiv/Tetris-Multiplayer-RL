"""Validate flush boundaries and pending image lifetime with actual source functions."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/57-flush-boundaries'
OUT=ROOT/'out/learning-checkpoints/57-flush-boundaries-check'
r=ROOT;out=OUT

def run(args,**kw):
 result=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',900),**kw)
 if result.returncode:raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
 return result

def cut(s,signature):
 a=s.index(signature);b=s.index('{',a);depth=1;i=b+1
 while depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 return s[a:i]+'\n'
def make(before):
 renderer=(out/'before-renderer.cpp'if before else r/'renderer/renderer.cpp').read_text()
 image=(out/'before-image_gl.cpp'if before else r/'renderer/image_gl.cpp').read_text()
 state=renderer[renderer.index('static int s_screen_w'):renderer.index('// ─── 셰이더')]
 funcs=renderer[renderer.index('static void push_vertex'):renderer.index('// ─── 공개 API')]
 code='''#include "renderer/gl_internal.h"
#include "renderer/image.h"
#include <vector>
#include <set>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"CHECK failed %d\\n",__LINE__);std::exit(1);}}while(false)
'''+state+'''
static std::set<GLuint> live{7,8,9};static GLuint bound=0;
static std::vector<GLuint> draws,deleted;static int stale=0,pending_delete=0;
static void bind_buffer(GLenum,GLuint){}
static void data(GLenum,GLsizeiptr,const void*,GLenum){}
static void active(GLenum){}
static void texture(GLenum,GLuint tex){bound=tex;}
static void array(GLuint){}
static void draw(GLenum,GLint,GLsizei count){CHECK(count%6==0);draws.push_back(bound);if(!live.count(bound))++stale;}
static void del(GLsizei n,const GLuint* names){for(int i=0;i<n;++i){auto tex=names[i];if(!s_verts.empty()&&s_batch_tex==tex)++pending_delete;CHECK(live.erase(tex)==1);deleted.push_back(tex);}}
decltype(gl_BindBuffer) gl_BindBuffer=bind_buffer;
decltype(gl_BufferData) gl_BufferData=data;
decltype(gl_ActiveTexture) gl_ActiveTexture=active;
decltype(gl_BindTexture) gl_BindTexture=texture;
decltype(gl_BindVertexArray) gl_BindVertexArray=array;
decltype(gl_DrawArrays) gl_DrawArrays=draw;
decltype(gl_DeleteTextures) gl_DeleteTextures=del;
'''+funcs+'''
struct ImageEntry {bool used=false;int w=0,h=0;GLuint tex=0;};
static std::vector<ImageEntry> s_images;
'''+cut(image,'void image_init()')+cut(image,'void image_shutdown()')+cut(image,'void image_unload(')+cut(image,'void draw_image_tinted(')+'''
int main(){s_ready=true;s_screen_w=s_screen_h=100;s_white=9;s_vao=2;s_vbo=3;image_init();s_images.push_back({true,2,2,7});s_images.push_back({true,2,2,8});
 draw_image_tinted(1,1,1,10,10,{255,255,255,255});CHECK(s_verts.size()==84&&draws.empty());
 image_unload(1);
'''
 if before:code+='''glb_flush();CHECK(stale==1&&pending_delete==1);std::printf("BEFORE: deleted while pending=%d; submitted dead texture=%d\\n",pending_delete,stale);}\n'''
 else:code+='''CHECK(draws.size()==1&&draws[0]==7&&s_verts.empty()&&s_batch_tex==0&&pending_delete==0);
 image_unload(1);CHECK(deleted.size()==1);image_unload(-1);image_unload(99);CHECK(deleted.size()==1);
 // GL is allowed to reuse the number 7 for a different image.
 live.insert(7);s_images[1]={true,2,2,7};draw_image_tinted(1,1,1,10,10,{255,255,255,255});
 image_unload(2);CHECK(draws.size()==1&&s_verts.size()==84);glb_flush();CHECK(draws.size()==2&&draws.back()==7&&stale==0);
 draw_image_tinted(1,1,1,10,10,{255,255,255,255});image_shutdown();CHECK(draws.size()==3&&draws.back()==7&&s_batch_tex==0&&s_verts.empty());
 image_shutdown();CHECK(pending_delete==0&&stale==0&&s_images.empty());
 std::puts("AFTER: pending image use submitted before delete; dead name forgotten; unrelated texture, name reuse and shutdown passed");}\n'''
 return code

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for p in (SOURCE.parent/'56-color-batch').rglob('*'):
  rel=p.relative_to(SOURCE.parent/'56-color-batch')
  if p.is_file() and str(rel) not in {'CMakeLists.txt','README.md','src/main.cpp','renderer/color_batch.h'}:assert p.read_bytes()==(SOURCE/rel).read_bytes(),rel
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  log=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(log.stdout+log.stderr);assert 'warning:'not in log.stdout+log.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
  demo=run([str(build/'flush_demo')]).stdout;assert '1356 vertices in 2 ordered submissions' in demo;(OUT/f'demo-{backend}.log').write_text(demo)
  if backend=='SDL':
   result=run([str(build/'flush_real')],env={**env,'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'});(OUT/'pixels.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE)]
 run([*flags,str(SOURCE/'tests/flush_contract.cpp'),'-o',str(OUT/'flush-sanitized')]);print(run([str(OUT/'flush-sanitized')]).stdout.strip(),flush=True)
 for name,before,after in [
  ('new_clip_first','if (!flush()) return false; // Submit with the OLD clip first.','clip_ = next; if (!flush()) return false;'),
  ('clear_omitted','pending_.clear();','/* clear omitted */'),
  ('retry_failure','good_=false; return false;','return false;')]:
  folder=OUT/name;p=folder/'renderer/flush_stream.h';p.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/'renderer/flush_stream.h').read_text();assert before in text;p.write_text(text.replace(before,after,1))
  run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),'-I'+str(SOURCE),str(SOURCE/'tests/flush_contract.cpp'),'-o',str(folder/'check')])
  result=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert result.returncode==1 and 'CHECK failed' in result.stderr,name;(folder/'result.log').write_text(result.stderr)
 probe=OUT/'root-after.cpp';probe.write_text(make(False));binary=OUT/'root-after'
 run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(probe),'-o',str(binary)]);print(run([str(binary)]).stdout.strip(),flush=True)
 # Standalone atlas retirement and the parent shutdown must use the same boundary.
 text=(ROOT/'renderer/text_gl.cpp').read_text();body=cut(text,'void renderer_text_shutdown()');assert body.index('glb_before_texture_delete')<body.index('gl_DeleteTextures')
 body=cut((ROOT/'renderer/renderer.cpp').read_text(),'void renderer_shutdown()');assert body.index('glb_flush()')<body.index('image_shutdown()')
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root'
 run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release'])
 run(['cmake','--build',str(build),'-j3'])
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('Flush contracts, three Release mutations, offscreen clip pixels, root retirement probe, UBSan and unchanged golden passed.',flush=True)
if __name__=='__main__':main()
