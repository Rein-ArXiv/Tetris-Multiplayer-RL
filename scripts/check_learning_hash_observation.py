"""Lesson97: consistent snapshots and bounded per-tick comparisons across owners."""
from pathlib import Path
import sys,os,subprocess,json
from check_learning_text_layout import run
from check_learning_utf8 import cut
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/97-hash-observation';OUT=ROOT/'out/learning-checkpoints/97-hash-observation-check'
def root_probe(before=False):
 OUT.mkdir(parents=True,exist_ok=True)
 s=(ROOT/('out/learning-jobs/097-before-session.cpp' if before else 'net/session.cpp')).read_text()
 a=s.index('    case MsgType::HASH: {');b=s.index('    case MsgType::GAME_OVER_CHOICE:',a);(OUT/'hash_receive.inc').write_text(s[a:b])
 syms=['void Session::SendHash(','bool Session::GetLastRemoteHash(']+([] if before else ['bool Session::PollHashComparison('])
 (OUT/'hash_methods.inc').write_text('\n'.join(cut(s,x) for x in syms))
 main=(ROOT/('out/learning-jobs/097-before-main.cpp' if before else 'src/main.cpp')).read_text()
 a=main.index('struct HashSnap {' if before else 'constexpr uint32_t HASH_PERIOD_TICKS');b=main.index('// Section I — 공격',a);(OUT/'hash_storage.inc').write_text(main[a:b])
 a=main.index('// F.2 — 원격 HASH 수신' if before else '// F.2 — Compare pending');(OUT/'hash_compare.inc').write_text(cut(main[a:],'if (app == AppMode::Net && gameLocal)'))
 flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 exe=OUT/('root-before' if before else 'root-after')
 run([*flags,*(['-DBEFORE'] if before else []),'-I'+str(ROOT),'-I'+str(OUT),str(ROOT/'tests/learning/hash_observation.cpp'),str(ROOT/'net/framing.cpp'),'-o',str(exe)])
 p=subprocess.run([str(exe)],text=True,capture_output=True,timeout=15);assert p.returncode==(1 if before else 0),p.stderr
 if before:assert 'desyncDetected&&desyncTick==600' in p.stderr
 else:
  assert 'captured_tick=1201 compared_tick=600' in p.stderr
  assert s.count('hashMailbox_.clear();')==7
  assert main.count('resetHashComparison();')==2
  assert 'hash_player_pair(hL, hR)' in main and 'hash_player_pair(hR, hL)' in main
  run([*flags,'-I'+str(ROOT),str(ROOT/'tests/learning/hash_pair.cpp'),'-o',str(OUT/'pair')]);run([str(OUT/'pair')])
 print('Actual latest-only implementation misses earlier mismatch (expected failure)' if before else p.stdout.strip(),flush=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 old=CP.parent/'96-round-inputs'
 for p in old.rglob('*'):
  if p.is_file() and p.relative_to(old).as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
 root=(ROOT/'net/hash_mailbox.h').read_text().replace('NET_HASH_MAILBOX_H','STUDY_NET_HASH_MAILBOX_H').replace('namespace net','namespace study_net').replace('using HashExchange = HashMailbox<600, 8>;\n\n','')
 assert root==(CP/'net/hash_mailbox.h').read_text()
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'-j3']);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'--output-on-failure'],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','DISPLAY':'','WAYLAND_DISPLAY':''});print(backend,p.stdout[-225:],flush=True)
 flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 for source,name in [('tests/mailbox_contract.cpp','mailbox'),('tools/observation_demo.cpp','observation')]:
  run([*flags,'-I'+str(CP),str(CP/source),'-o',str(OUT/(name+'-sanitized'))]);print(run([str(OUT/(name+'-sanitized'))]).stdout,flush=True)
 root_probe(True);root_probe()
 lesson=ROOT/'docs/learn/lessons/097.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for code in sec.get('codes',[]):
    if 'text' in code and code['language'] in corpora:
     assert any(normalized(code['text'],code['language']) in s for s in corpora[code['language']]),code['label'];count+=1
  print('Inline implementations',count,flush=True)
 for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
  run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-210:],flush=True)
 print('Hash observation checks complete',flush=True)
if __name__=='__main__':main()
