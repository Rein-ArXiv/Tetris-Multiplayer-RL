"""Lesson102: bounded FIFO, cancellation ownership and real post-match inputs."""
from pathlib import Path
import os,sys,json
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/102-match-queue'
OUT=ROOT/'out/learning-checkpoints/102-match-queue-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 old=CP.parent/'101-worker-lifetime'
 for p in old.rglob('*'):
  rel=p.relative_to(old)
  if p.is_file() and rel.as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/rel).read_bytes(),rel
 flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 for name,src,inc,extra,modes in [
 ('root',ROOT/'tests/learning/matchmaker_queue.cpp',ROOT,[str(ROOT/f) for f in ['server/matchmaker.cpp','server/log.cpp','net/socket.cpp','net/framing.cpp']],[[],['late'],['lone'],['full']]),
 ('contract',CP/'tests/pair_queue_contract.cpp',CP,[],[[]]),
 ('probe',CP/'tools/pair_queue_probe.cpp',CP,[str(CP/('net/'+f+'.cpp')) for f in ['socket','stream','send_socket','receive_socket']],[[]])]:
  exe=OUT/name;run([*flags,'-I'+str(inc),str(src),*extra,'-o',str(exe)])
  for mode in modes:p=run([str(exe),*mode],timeout=25);print(name,mode,p.stdout,p.stderr,flush=True)
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'--target','pair_queue_contract','pair_queue_probe','-j3'])
   (OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'-R','^pair_queue_(contract|probe)$','--output-on-failure'])
   (OUT/f'ctest-{backend}.log').write_text(p.stdout);print(backend,p.stdout[-220:],flush=True)
  for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
   run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-220:],flush=True)
 lesson=ROOT/'docs/learn/lessons/102.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for c in sec.get('codes',[]):
    if 'text' in c and c['language'] in corpora:assert any(normalized(c['text'],c['language']) in t for t in corpora[c['language']]),c['label'];count+=1
  print('Inline implementations',count,flush=True)
 print('Pair queue checks complete; unchanged prior checkpoint verified byte-for-byte, prior test suite not rebuilt',flush=True)
if __name__=='__main__':main()
