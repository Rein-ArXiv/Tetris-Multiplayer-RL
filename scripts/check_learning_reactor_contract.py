"""Cumulative Reactor ownership: real readiness, registration identity and callback mutation."""
from pathlib import Path
import json,sys
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/126-reactor-contract';OUT=ROOT/'out/learning-checkpoints/126-reactor-contract-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True);prev=CP.parent/'125-io-models'
 for p in prev.rglob('*'):
  if p.is_file() and p.relative_to(prev).as_posix()not in {'README.md','CMakeLists.txt'}:assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
 if '--snippets-only'not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),f'-DSTUDY_PLATFORM={backend}','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   targets=['reactor_contract','reactor_probe','io_models_probe']+(['tetris']if backend=='SDL'else[])
   r=run(['cmake','--build',str(b),'--target',*targets,'-j2']);(OUT/f'build-{backend}.log').write_text(r.stdout+r.stderr);print(backend,'targets built',flush=True)
   print(run(['ctest','--test-dir',str(b),'-R','^(reactor_contract|reactor_probe|io_models_probe)$','--output-on-failure'],timeout=60).stdout,flush=True)
   if backend=='SCRIPTED':
    for _ in range(20):run([str(b/'reactor_contract')],timeout=15)
    print(run([str(b/'reactor_probe')],timeout=15).stdout,flush=True)
    print('Registration/callback/wake contracts repeated20',flush=True)
  sources=['net/socket.cpp','net/stream.cpp','net/send_socket.cpp','net/receive_socket.cpp','net/poll_reactor.cpp']
  exe=OUT/'reactor-sanitized';run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/'tests/reactor_contract.cpp'),*[str(CP/p)for p in sources],'-o',str(exe)],timeout=120)
  print(run([str(exe)],timeout=20).stdout,flush=True);print('ASan/UBSan all new socket/reactor/callback paths passed',flush=True)
  print(run([sys.executable,str(ROOT/'scripts/check_learning_reactor_wake.py')],timeout=60).stdout,flush=True)
 p=ROOT/'docs/learn/lessons/126.json'
 if p.exists():
  corpus={lang:[normalized(f.read_text(),lang)for f in CP.rglob('*')if f.is_file()and(f.suffix in ['.h','.cpp']if lang=='cpp'else f.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};n=0
  for s in json.loads(p.read_text())['sections']:
   for c in s.get('codes',[]):
    if 'text'in c and c['language']in corpus:
     assert any(normalized(c['text'],c['language'])in t for t in corpus[c['language']]),c['label'];n+=1
  print('inline snippets',n,flush=True)
if __name__=='__main__':main()
