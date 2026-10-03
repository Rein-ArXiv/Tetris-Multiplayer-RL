"""Lesson98: intention, local agreement, transport and result are separate."""
from pathlib import Path
import sys,os,subprocess,json
from check_learning_text_layout import run
from check_learning_utf8 import cut
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/98-end-negotiation';OUT=ROOT/'out/learning-checkpoints/98-end-negotiation-check'
def root_probe(before=False):
 OUT.mkdir(parents=True,exist_ok=True)
 s=(ROOT/('out/learning-jobs/098-before-session.cpp' if before else 'net/session.cpp')).read_text()
 a=s.index('    case MsgType::GAME_OVER_CHOICE: {');b=s.index('    case MsgType::',a+10)
 (OUT/'choice_case.inc').write_text(s[a:b])
 syms=['void Session::SendGameOverChoice(','bool Session::GetRemoteGameOverChoice(','void Session::ClearGameOverChoices(']
 (OUT/'choice_methods.inc').write_text('\n'.join(cut(s,x).replace('Session::','ProbeSession::') for x in syms))
 exe=OUT/('root-before' if before else 'root-after')
 run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/end_choice.cpp'),str(ROOT/'net/framing.cpp'),'-o',str(exe)])
 p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
 assert p.returncode==(1 if before else 0),p.stderr
 if before:assert 'size==1' in p.stderr
 print('Actual prefix-only decoder fails exact-length regression (expected)' if before else p.stdout.strip(),flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 old=CP.parent/'97-hash-observation'
 for p in old.rglob('*'):
  if p.is_file() and p.relative_to(old).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-225:],flush=True)
 for source,name in [('tests/end_negotiation_contract.cpp','contract'),('tools/end_timeline.cpp','timeline')]:
  exe=OUT/(name+'-sanitized');run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/source),'-o',str(exe)]);print(run([str(exe)]).stdout,flush=True)
 root_probe(True);root_probe()
 lesson=ROOT/'docs/learn/lessons/098.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for code in sec.get('codes',[]):
    if 'text' in code and code['language'] in corpora:
     assert any(normalized(code['text'],code['language']) in s for s in corpora[code['language']]),code['label'];count+=1
  print('Inline implementations',count,flush=True)
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-210:],flush=True)
 print('End negotiation checks complete',flush=True)
if __name__=='__main__':main()
