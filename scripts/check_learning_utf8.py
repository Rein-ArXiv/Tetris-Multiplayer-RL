"""UTF-8 scalar decoding, byte boundaries, error recovery and label integration."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/learn/checkpoints/63-utf8'
OUT=ROOT/'out/learning-checkpoints/63-utf8-check'
def run(args,**kw):
 result=subprocess.run(args,cwd=ROOT,text=True,capture_output=True,timeout=kw.pop('timeout',1000),**kw)
 if result.returncode:raise RuntimeError(f'{args}\n{result.stdout}\n{result.stderr}')
 return result

def cut(s,sig):
 a=s.index(sig);b=s.index('{',a);i=b+1;depth=1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]+'\n'

def root_probe(before=False):
 OUT.mkdir(parents=True,exist_ok=True)
 source=OUT/'before-text_gl.cpp' if before else ROOT/'renderer/text_gl.cpp'
 code=r"""#include "core/utf8.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
"""+cut(source.read_text(),'static uint32_t utf8_next(')+r"""
int main(){const char overlong[]={char(0xC0),char(0xAF),0};const char surrogate[]={char(0xED),char(0xA0),char(0x80),0};const char high[]={char(0xF4),char(0x90),char(0x80),char(0x80),0};const char* p=overlong;
"""
 if before:code+=r"""CHECK(utf8_next(&p)==0x2F&&p==overlong+2);p=surrogate;CHECK(utf8_next(&p)==0xD800);p=high;CHECK(utf8_next(&p)==0x110000);std::puts("BEFORE: overlong slash, surrogate and above-Unicode scalar accepted");}
"""
 else:code+=r"""CHECK(utf8_next(&p)==0xFFFD&&p==overlong+1);p=surrogate;CHECK(utf8_next(&p)==0xFFFD&&p==surrogate+1);p=high;CHECK(utf8_next(&p)==0xFFFD&&p==high+1);
 const char recover[]={char(0xE2),char(0x82),'A',0};p=recover;CHECK(utf8_next(&p)==0xFFFD&&utf8_next(&p)==0xFFFD&&utf8_next(&p)=='A'&&*p==0);
 const char* empty="";p=empty;CHECK(utf8_next(&p)==0&&p==empty);
 const auto page=std::size_t(sysconf(_SC_PAGESIZE));auto* memory=static_cast<char*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(memory!=MAP_FAILED);CHECK(mprotect(memory+page,page,PROT_NONE)==0);
 for(const auto& sample:std::vector<std::string>{"", "A",u8"가",u8"🙂",std::string(1,char(0xE2)),std::string{char(0xF0),char(0x9F),char(0x99)}}){
  char* start=memory+page-sample.size()-1;std::memcpy(start,sample.data(),sample.size());start[sample.size()]=0;p=start;
  while(*p){auto item=utf8::decode_first(std::string_view(p,static_cast<std::size_t>(memory+page-1-p)));auto* previous=p;CHECK(utf8_next(&p)==item.codepoint&&p==previous+item.bytes);}
  CHECK(p==memory+page-1);
 }
 CHECK(munmap(memory,2*page)==0);std::puts("AFTER: malformed scalars rejected, ASCII recovery and guarded C-string lookahead passed");}
"""
 name='before'if before else'after';p=OUT/f'root-{name}.cpp';p.write_text(code)
 run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),'-o',str(OUT/f'root-{name}')]);r=run([str(OUT/f'root-{name}')]);(OUT/f'root-{name}.log').write_text(r.stdout+r.stderr);print(r.stdout.strip(),flush=True)

def guard_probe():
 code=r"""#include "text/utf8.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define CHECK(e) do{if(!(e))std::exit(1);}while(false)
int main(){const auto page=std::size_t(sysconf(_SC_PAGESIZE));auto* memory=static_cast<char*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(memory!=MAP_FAILED&&mprotect(memory+page,page,PROT_NONE)==0);
 for(const auto& sample:std::vector<std::string>{"A",u8"가",u8"🙂",std::string(1,char(0xE2)),std::string{char(0xF0),char(0x9F),char(0x99)}}){
  char* start=memory+page-sample.size();std::memcpy(start,sample.data(),sample.size());std::string_view view(start,sample.size());
  while(!view.empty()){auto r=study_utf8::decode_first(view);CHECK(r.bytes>=1&&r.bytes<=view.size());view.remove_prefix(r.bytes);}
 }
 CHECK(munmap(memory,2*page)==0);std::puts("Bounded decoder: unterminated/truncated spans at guard page passed");}
"""
 p=OUT/'guard.cpp';p.write_text(code);run(['c++','-std=c++17','-O1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE),str(p),'-o',str(OUT/'guard')]);r=run([str(OUT/'guard')]);(OUT/'guard.log').write_text(r.stdout);print(r.stdout.strip(),flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 previous=SOURCE.parent/'62-rounded-corners'
 for p in previous.rglob('*'):
  if p.is_file() and str(p.relative_to(previous))not in {'CMakeLists.txt','README.md','src/main.cpp'}:assert p.read_bytes()==(SOURCE/p.relative_to(previous)).read_bytes(),p
 env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''}
 for backend in ['SCRIPTED','SDL']:
  build=OUT/backend.lower();run(['cmake','-S',str(SOURCE),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
  result=run(['cmake','--build',str(build),'-j3']);(OUT/f'build-{backend}.log').write_text(result.stdout+result.stderr);assert 'warning:'not in result.stdout+result.stderr
  result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/f'ctest-{backend}.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
  print(run([str(build/'utf8_demo')]).stdout.strip(),flush=True)
 flags=['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(SOURCE)]
 for name in ['utf8_contract','labels_contract']:
  run([*flags,str(SOURCE/f'tests/{name}.cpp'),'-o',str(OUT/(name+'-sanitized'))]);print(run([str(OUT/(name+'-sanitized'))]).stdout.strip(),flush=True)
 run(['c++','-std=c++17','-O1','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(ROOT/'tests/utf8_test.cpp'),'-o',str(OUT/'root-sanitized')]);print(run([str(OUT/'root-sanitized')]).stdout.strip(),flush=True)
 guard_probe()
 for name,old,new in [('minimum','code < minimum ||','false ||'),('surrogate','(code >= kSurrogateLow && code <= kSurrogateHigh)','false')]:
  folder=OUT/name;header=folder/'text/utf8.h';header.parent.mkdir(parents=True,exist_ok=True);text=(SOURCE/'text/utf8.h').read_text();assert old in text;header.write_text(text.replace(old,new))
  run(['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(folder),str(SOURCE/'tests/utf8_contract.cpp'),'-o',str(folder/'check')]);result=subprocess.run([str(folder/'check')],capture_output=True,text=True);assert result.returncode==1 and 'CHECK failed'in result.stderr;(folder/'result.log').write_text(result.stderr)
 root_probe()
 build=ROOT/'out/learning-checkpoints/54-game-adapter-check/root';run(['cmake','-S',str(ROOT),'-B',str(build),'-DTETRIS_BUILD_REACTOR=OFF','-DCMAKE_BUILD_TYPE=Release']);run(['cmake','--build',str(build),'-j3'])
 result=run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env);(OUT/'root-ctest.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)
 assert run([str(build/'sim_hash_dump')]).stdout==(ROOT/'python/tests/_sim_hash_dump.txt').read_text()
 print('UTF-8 scalar space, invalid recovery, guard-page bounds, labels and root regressions passed.',flush=True)
if __name__=='__main__':main()
