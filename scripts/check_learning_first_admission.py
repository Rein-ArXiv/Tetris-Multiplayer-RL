"""Lesson100: bounded first-command routing and socket/parser ownership handoff."""
from pathlib import Path
import sys,os,json,subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/100-first-admission';OUT=ROOT/'out/learning-checkpoints/100-first-admission-check'
def root_probe(before=False):
 OUT.mkdir(parents=True,exist_ok=True)
 for src,target,marker in [('player_conn.cpp','initial_parse.inc','            for (size_t i'),('room.cpp','room_parse.inc','            for (const auto& f')]:
  s=(ROOT/('server/'+src)).read_text();a=s.index('            std::vector<net::Frame> frames;');b=s.index(marker,a)
  branch=s[a:b]
  if before:
   # Reconstruct only the former unchecked parse call; no unpublished backup dependency.
   assert 'if (!net::parse_frames(stream, frames))' in branch
   branch='            std::vector<net::Frame> frames;\n            net::parse_frames(stream, frames);\n'
  (OUT/target).write_text(branch)
 s=(ROOT/'server/player_conn.cpp').read_text();(OUT/'residual.inc').write_text(cut(s,'std::vector<uint8_t> residual_stream('))
 exe=OUT/('root-before' if before else 'root-after')
 run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/first_parse.cpp'),str(ROOT/'net/framing.cpp'),str(ROOT/'net/socket.cpp'),'-o',str(exe)])
 for mode in ['first','room']:
  p=subprocess.run([str(exe),mode],text=True,capture_output=True,timeout=10);assert p.returncode==(1 if before else 0),p.stderr
  if before:assert 'continued==0' in p.stderr
  print(mode+' expected pre-fix dispatch failure' if before else p.stdout.strip(),flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 old=CP.parent/'99-relay-choice'
 for p in old.rglob('*'):
  if p.is_file() and p.relative_to(old).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
 flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP)]
 for src,name,extra in [('tests/admission_contract.cpp','contract',[]),('tools/admission_probe.cpp','probe',[str(CP/('net/'+f+'.cpp')) for f in ['socket','stream','send_socket','receive_socket']])]:
  exe=OUT/(name+'-sanitized');run([*flags,str(CP/src),*extra,'-o',str(exe)]);print(run([str(exe)],timeout=25).stdout,flush=True)
 root_probe(True);root_probe()
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});(OUT/f'ctest-{backend}.log').write_text(p.stdout);print(backend,p.stdout[-225:],flush=True)
 lesson=ROOT/'docs/learn/lessons/100.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for code in sec.get('codes',[]):
    if 'text' in code and code['language'] in corpora:
     assert any(normalized(code['text'],code['language']) in s for s in corpora[code['language']]),code['label'];count+=1
  print('Inline implementations',count,flush=True)
 if '--skip-build' in sys.argv:
  print('First admission targeted checks complete',flush=True);return
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-210:],flush=True)
 print('First admission checks complete',flush=True)
if __name__=='__main__':main()
