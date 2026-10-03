"""Lesson103: code allocation, generations, registry ownership and real TCP join."""
from pathlib import Path
import os,sys,json
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/103-room-code'
OUT=ROOT/'out/learning-checkpoints/103-room-code-check'
def check_root():
 flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic']
 common=[str(ROOT/f) for f in ['server/room_code.cpp','server/log.cpp','net/socket.cpp','net/framing.cpp']]
 for kind in ['shutdown','exception','abandoned']:
  directory=OUT/('root-'+kind);directory.mkdir(parents=True,exist_ok=True)
  header=(ROOT/'server/room.h').read_text();source=(ROOT/'server/room.cpp').read_text()
  if kind=='shutdown':
   anchor='    std::string code;'
   assert source.count(anchor)==1
   source='extern void room_create_gate();\n'+source.replace(anchor,anchor+'\n    room_create_gate();')
  elif kind=='exception':
   header=header.replace('    RoomRegistry();','    RoomRegistry();\n    std::size_t study_room_count() { std::lock_guard<std::mutex> lock(mu); return rooms.size(); }')
  else:
   helpers = """
    std::size_t study_room_count() { std::lock_guard<std::mutex> lock(mu); return rooms.size(); }
    void study_seed_room(const net::TcpSocket& host) { std::lock_guard<std::mutex> lock(mu); Entry r; r.code="AAAAA"; r.hostSock=host; r.hostPresent=true; r.roomInfoVersion=next_room_info_version_++; rooms.emplace(r.code,r); }
    void study_change_version() { std::lock_guard<std::mutex> lock(mu); ++rooms.at("AAAAA").roomInfoVersion; }
"""
   header=header.replace('    RoomRegistry();','    RoomRegistry();'+helpers)
   anchor='            ++next_room_info_version_;\n            lk.unlock();'
   assert source.count(anchor)==1
   source='extern void room_join_gate();\n'+source.replace(anchor,anchor+'\n            room_join_gate();')
  (directory/'room.h').write_text(header);(directory/'room.cpp').write_text(source)
  exe=directory/'check'
  sanitizers=['-fsanitize=address,undefined','-fno-sanitize-recover=all'] if kind!='exception' else []
  run([*flags,*sanitizers,'-I'+str(directory),'-I'+str(ROOT/'server'),'-I'+str(ROOT),str(ROOT/('tests/learning/room_join_abandoned.cpp' if kind=='abandoned' else 'tests/learning/room_create_'+kind+'.cpp')),str(directory/'room.cpp'),*common,'-o',str(exe)])
  p=run([str(exe)],timeout=20);print(kind,p.stdout,p.stderr,flush=True)
 exe=OUT/'room-code-root';extra=['-DROOM_RANDOM_WRAP','-Wl,--wrap=getrandom'] if sys.platform.startswith('linux') else []
 run([*flags,'-fsanitize=address,undefined','-fno-sanitize-recover=all',*extra,'-I'+str(ROOT),str(ROOT/'tests/learning/room_code.cpp'),str(ROOT/'server/room_code.cpp'),'-o',str(exe)])
 p=run([str(exe)]);print(p.stdout,flush=True)

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 check_root()
 old=CP.parent/'102-match-queue'
 for p in old.rglob('*'):
  rel=p.relative_to(old)
  if p.is_file() and rel.as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/rel).read_bytes(),rel
 flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 for name,src,inc,extra,modes in [
 ('contract',CP/'tests/room_directory_contract.cpp',CP,[],[[]]),
 ('probe',CP/'tools/room_directory_probe.cpp',CP,[str(CP/('net/'+f+'.cpp')) for f in ['socket','stream','send_socket','receive_socket']],[[]])]:
  exe=OUT/name;run([*flags,'-I'+str(inc),str(src),*extra,'-o',str(exe)])
  for mode in modes:p=run([str(exe),*mode],timeout=25);print(name,mode,p.stdout,p.stderr,flush=True)
 if '--skip-build' not in sys.argv:
  for backend in ['SCRIPTED','SDL']:
   b=OUT/backend.lower();run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
   p=run(['cmake','--build',str(b),'--target','room_directory_contract','room_directory_probe','-j3'])
   (OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr);assert 'warning:' not in p.stdout+p.stderr
   p=run(['ctest','--test-dir',str(b),'-R','^room_directory_(contract|probe)$','--output-on-failure'])
   (OUT/f'ctest-{backend}.log').write_text(p.stdout);print(backend,p.stdout[-220:],flush=True)
  for b in [ROOT/'out/learning-checkpoints/83-sockets-check/root',ROOT/'out/learning-checkpoints/54-game-adapter-check/root']:
   run(['cmake','--build',str(b),'-j3']);p=run(['ctest','--test-dir',str(b),'--output-on-failure']);print(p.stdout[-220:],flush=True)
 lesson=ROOT/'docs/learn/lessons/103.json'
 if lesson.exists():
  corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']};count=0
  for sec in json.loads(lesson.read_text())['sections']:
   for c in sec.get('codes',[]):
    if 'text' in c and c['language'] in corpora:assert any(normalized(c['text'],c['language']) in t for t in corpora[c['language']]),c['label'];count+=1
  print('Inline implementations',count,flush=True)
 print('Room code checks complete; unchanged prior checkpoint verified byte-for-byte, prior test suite not rebuilt',flush=True)
if __name__=='__main__':main()
