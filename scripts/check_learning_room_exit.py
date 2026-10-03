"""Check lesson106's stale membership, concurrent exits and versioned notices contracts."""
from pathlib import Path
import sys,json
from check_learning_text_layout import run
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/106-room-exit'
OUT=ROOT/'out/learning-checkpoints/106-room-exit-check'
def check_root():
    # Private copied headers expose fixtures; decisions remain the production code.
    R=ROOT; O=OUT
    O.mkdir(parents=True,exist_ok=True)
    source=(R/'server/room.cpp').read_text()
    header=(R/'server/room.h').read_text()
    helpers='''
        void study_seed(const net::TcpSocket& a,const net::TcpSocket& b,bool ready=false) {
            std::lock_guard<std::mutex> lock(mu); rooms.clear(); Entry r; r.code="AAAAA";
            r.hostSock=a;r.guestSock=b;r.hostPresent=true;r.guestPresent=b.valid();
            r.hostReady=ready;r.guestReady=ready;r.roomInfoVersion=next_room_info_version_++;
            rooms.emplace(r.code,std::move(r));
        }
        void study_run(bool host,const net::TcpSocket& expected) { CALL }
        bool study_intact(const net::TcpSocket& a,const net::TcpSocket& b) {
            std::lock_guard<std::mutex> lock(mu); auto it=rooms.find("AAAAA");if(it==rooms.end())return false;
            const auto& r=it->second;return r.hostPresent && r.guestPresent && !r.hostReady && !r.guestReady &&
                r.hostSock.transport==a.transport && r.guestSock.transport==b.transport;
        }
    '''.replace('CALL','roomLoop_("AAAAA",host,expected,{});')
    assert header.count('    RoomRegistry();') == 1
    header=header.replace('    RoomRegistry();','    RoomRegistry();'+helpers)
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic']
    flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    for kind in ['stale','wakeup','resource']:
     d=O/('after-'+kind);d.mkdir(exist_ok=True);s=source
     if kind=='wakeup':
      anchor='                if (isHost)  return r.guestExited || !r.guestPresent;'
      assert s.count(anchor)==1;s='extern void room_wait_observed();\n'+s.replace(anchor,'                room_wait_observed();\n'+anchor)
     (d/'room.h').write_text(header);(d/'room.cpp').write_text(s)
     test={'stale':'room_stale_owner.cpp','wakeup':'room_exit_wakeup.cpp','resource':'room_departed_resource.cpp'}[kind]
     cmd=[*flags,'-I'+str(d),'-I'+str(R/'server'),'-I'+str(R),str(R/'tests/learning'/test),str(d/'room.cpp'),*[str(R/p) for p in ['server/room_code.cpp','server/log.cpp','net/socket.cpp','net/framing.cpp']],'-o',str(d/'check')]
     run(cmd)
     for args in ([[],['entry']] if kind=='stale' else [[]]):
      p=run([str(d/'check'),*args],timeout=10)
      print(kind,args,'exit',p.returncode,p.stdout,p.stderr,flush=True)
      assert p.returncode==0

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    if "--skip-root" not in sys.argv: check_root()
    old=CP.parent/'105-forwarder'
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if p.is_file() and rel.as_posix() not in {'README.md','DESIGN.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/rel).read_bytes(),rel
    flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic',
           '-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP)]
    for name,source,extra in [
        ('contract','tests/room_membership_contract.cpp',[]),
        ('probe','tools/room_membership_probe.cpp',
         [str(CP/('net/'+x+'.cpp')) for x in ['socket','stream','send_socket','receive_socket']])]:
        exe=OUT/name
        run([*flags,str(CP/source),*extra,'-o',str(exe)])
        p=run([str(exe)],timeout=20);print(p.stdout,p.stderr,flush=True)
    if '--skip-build' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,
                 '-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            p=run(['cmake','--build',str(b),'--target','room_membership_contract','room_membership_probe','-j3'])
            assert 'warning:' not in p.stdout+p.stderr
            p=run(['ctest','--test-dir',str(b),'-R','^room_membership_(contract|probe)$','--output-on-failure'])
            print(backend,p.stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/106.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
                       (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')]
                 for lang in ['cpp','cmake']}
        count=0
        for sec in json.loads(lesson.read_text())['sections']:
            for code in sec.get('codes',[]):
                if code['language'] in corpora and 'text' in code:
                    assert any(normalized(code['text'],code['language']) in text for text in corpora[code['language']]),code['label']
                    count+=1
        print('inline snippets:',count)
if __name__=='__main__':main()
