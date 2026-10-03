"""Settings regressions in actual root functions; only disposable fixture files."""
from pathlib import Path
import subprocess,resource,signal,os
from check_learning_utf8 import cut
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/73-settings-check'
HEAD=r'''#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <optional>
#include <string>
#include "meta/private_file.h"
static constexpr int kWindowScaleCount=5;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"settings-root %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
'''
def extract(s):
 out=HEAD+cut(s,'struct GameSettings {')+';\n'+cut(s,'static std::string trim_copy(')
 for name in ['static std::optional<bool> parse_bool_value(','static bool parse_bool01(','static int parse_legacy_volume(','static int parse_int_clamped(','static GameSettings load_settings(','static bool save_settings(']:
  if name in s:out+=cut(s,name)+'\n'
 return out
TAIL=r'''int main(int argc,char** argv){CHECK(argc>=2);std::string mode=argv[1];
if(mode=="parse-prefix"){CHECK(parse_int_clamped("12junk",75,0,100)==75);}
else if(mode=="parse-overflow"){CHECK(parse_int_clamped("999999999999999999999999999",75,0,100)==75);}
else if(mode=="parse-nul"){CHECK(parse_int_clamped(std::string("12\0junk",7),75,0,100)==75);}
else if(mode=="parse"){
 CHECK(parse_int_clamped("12junk",75,0,100)==75);
 CHECK(parse_int_clamped("999999999999999999999999999",75,0,100)==75);
 CHECK(parse_int_clamped("2147483648",75,0,100)==100);
 CHECK(parse_int_clamped(" +30 ",75,0,100)==30&&parse_int_clamped("-5",75,0,100)==0&&parse_int_clamped("101",75,0,100)==100);
 CHECK(parse_int_clamped(std::string("12\0junk",7),75,0,100)==75);
}
else if(mode=="load"){CHECK(argc==3);auto s=load_settings(argv[2]);CHECK(s.bgmVol==37&&s.sfxVol==42&&s.ghostOn);}
else if(mode=="save"){CHECK(argc==3);GameSettings s;s.bgmVol=37;s.sfxVol=42;const bool ok=save_settings(argv[2],s);std::printf("saved=%d\n",ok);return ok?0:2;}
std::puts("Root settings contract passed");}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 baseline=OUT/'before-src-main.cpp'
 binaries={}
 for label,source in [('before',baseline.read_text()),('after',(ROOT/'src/main.cpp').read_text())]:
  p=OUT/('root-settings-'+label+'.cpp');p.write_text(extract(source)+TAIL);b=OUT/('root-settings-'+label)
  run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(ROOT),str(p),str(ROOT/'meta/private_file.cpp'),'-o',str(b)]);binaries[label]=b
 case=OUT/'parse-case.cfg';case.write_bytes(b'bgm_vol=37\nbgm=garbage\nsfx_vol=42\nsfx=garbage\n'+b' '*255+b'ghost=0\n'+b'bgm_vol=90\0junk\n')
 long_case=OUT/'long-line.cfg';long_case.write_bytes(b'bgm_vol=37\nsfx_vol=42\n'+b' '*255+b'ghost=0\n')
 nul_case=OUT/'nul-line.cfg';nul_case.write_bytes(b'bgm_vol=37\nsfx_vol=42\nbgm_vol=90\0junk\n')
 for label,mode,args in [('prefix','parse-prefix',[]),('overflow','parse-overflow',[]),('nul-int','parse-nul',[]),('legacy','load',[str(case)]),('long','load',[str(long_case)]),('nul-line','load',[str(nul_case)])]:
  before=subprocess.run([str(binaries['before']),mode,*args],capture_output=True,text=True);assert before.returncode!=0
  (OUT/('before-settings-'+label+'.log')).write_text(before.stdout+before.stderr)
  after=run([str(binaries['after']),mode,*args]);print(label+': before failure reproduced, after passed',flush=True)
 run([str(binaries['after']),'parse'])
 def limit():signal.signal(signal.SIGXFSZ,signal.SIG_IGN);resource.setrlimit(resource.RLIMIT_FSIZE,(0,0))
 for label,binary in binaries.items():
  path=OUT/('disk-full-'+label+'.cfg');path.write_text('previous settings\n')
  r=subprocess.run([str(binary),'save',str(path)],capture_output=True,text=True,preexec_fn=limit)
  assert r.returncode==2,(label,r.stderr)
  assert path.read_text()==('' if label=='before' else 'previous settings\n')
  (OUT/('disk-full-'+label+'.log')).write_text(r.stdout+r.stderr)
 print('write failure: before truncates old settings; after keeps old bytes',flush=True)
 fresh=OUT/'fresh/nested/settings.cfg';run([str(binaries['after']),'save',str(fresh)]);run([str(binaries['after']),'load',str(fresh)])
 print('actual root create/save/reload and legacy invalid values passed',flush=True)
if __name__=='__main__':main()
