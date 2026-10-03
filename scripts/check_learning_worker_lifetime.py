"""Lesson101: task/capture completion, bounded admission and real socket jobs."""
from pathlib import Path
import os,sys,json,subprocess
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/101-worker-lifetime'
OUT=ROOT/'out/learning-checkpoints/101-worker-lifetime-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for p in (CP.parent/'100-first-admission').rglob('*'):
  rel=p.relative_to(CP.parent/'100-first-admission')
  if p.is_file() and rel.as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/rel).read_bytes(),rel
 flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 for name,src,include,extra in [
 ('root',ROOT/'tests/learning/worker_lifetime.cpp',ROOT,[]),
 ('contract',CP/'tests/worker_contract.cpp',CP,[]),
 ('probe',CP/'tools/worker_probe.cpp',CP,[str(CP/('net/'+f+'.cpp')) for f in ['socket','stream','send_socket','receive_socket']])]:
  exe=OUT/name;run([*flags,'-I'+str(include),str(src),*extra,'-o',str(exe)])
  p=run([str(exe)],timeout=20);print(p.stdout,p.stderr,flush=True)
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower()
   run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''})
   (OUT/f'ctest-{backend}.log').write_text(p.stdout);print(backend,p.stdout[-220:],flush=True)
  for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
   run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-220:],flush=True)
 lesson=ROOT/'docs/learn/lessons/101.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for code in sec.get('codes',[]):
    if 'text' in code and code['language'] in corpora:
     assert any(normalized(code['text'],code['language']) in s for s in corpora[code['language']]),code['label'];count+=1
  print('Inline implementations',count,flush=True)
 print('Worker lifetime checks complete',flush=True)
if __name__=='__main__':main()
