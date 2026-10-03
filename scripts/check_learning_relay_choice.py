"""Lesson99: actual loopback topology and explicitly assumed path/traffic costs."""
from pathlib import Path
import sys,os,json
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/99-relay-choice';OUT=ROOT/'out/learning-checkpoints/99-relay-choice-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 old=CP.parent/'98-end-negotiation'
 for p in old.rglob('*'):
  if p.is_file() and p.relative_to(old).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP)]
 for source,name,extra in [('tests/relay_cost_contract.cpp','cost',[]),('tools/topology_probe.cpp','topology',[str(CP/'net/socket.cpp'),str(CP/'net/stream.cpp')])]:
  exe=OUT/(name+'-sanitized');run([*flags,str(CP/source),*extra,'-o',str(exe)]);p=run([str(exe)],timeout=25);print(p.stdout,flush=True)
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});(OUT/f'ctest-{backend}.log').write_text(p.stdout);print(backend,p.stdout[-225:],flush=True)
 lesson=ROOT/'docs/learn/lessons/099.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for code in sec.get('codes',[]):
    if 'text' in code and code['language'] in corpora:
     assert any(normalized(code['text'],code['language']) in s for s in corpora[code['language']]),code['label'];count+=1
  print('Inline implementations',count,flush=True)
 print('Relay choice checks complete',flush=True)
if __name__=='__main__':main()
