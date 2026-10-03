"""Lesson 118: complete JSON bytes, decoded keys, typed fields and domain limits."""
from pathlib import Path
import json,subprocess,tempfile,sys,urllib.request,urllib.error,sqlite3
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_icon_ownership import settlement_contract, account_http, shop_http
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/118-json-boundary'
OUT=ROOT/'out/learning-checkpoints/118-json-boundary-check'

def boundary_http(build):
    def request(port,path,body,token=None):
        headers={'Content-Type':'application/json'}
        if token:headers['Authorization']='Bearer '+token
        req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers)
        try:
            with urllib.request.urlopen(req,timeout=5) as r:return r.status,r.read()
        except urllib.error.HTTPError as e:return e.code,e.read()
    with tempfile.TemporaryDirectory(prefix='json-118-') as tmp:
        db=Path(tmp)/'accounts.db'
        with service(build/'study_account_db',db) as port:
            for body in [b'{}\0',b'{}\0tail',b'{}{}',b'{"x":1,"\\u0078":2}',b'{"x":"\xff"}',b'{}'+b' '*1023]:
                assert request(port,'/study/v1/guest',body)[0] in [400,413]
            with sqlite3.connect(db) as con:assert con.execute('SELECT count(*) FROM players').fetchone()[0]==0
            status,payload=request(port,'/study/v1/guest',b'{}'+b' '*1022)
            assert status==201
            guest=json.loads(payload);pid=str(guest['player_id']);token=guest['token']
            with sqlite3.connect(db) as con:con.execute('UPDATE wallets SET bp=300 WHERE player_id=?',(pid,))
            for body in [b'{"icon_id":"ruby"}\0tail',b'{"icon_id":"ruby","icon_\\u0069d":"gold"}',
                         b'{"icon_id":"ruby\\u0000tail"}',b'{"icon_id":100}',b'{"icon_id":"ruby","price":0}',
                         b'{"icon_id":{"icon_id":"ruby"}}',b'{"icon_id":"\xff"}']:
                assert request(port,'/study/v1/icons/buy',body,token)[0]==400
            with sqlite3.connect(db) as con:
                assert con.execute('SELECT bp FROM wallets WHERE player_id=?',(pid,)).fetchone()==(300,)
                assert con.execute('SELECT icon_id FROM player_icons WHERE player_id=?',(pid,)).fetchall()==[('default',)]
            assert request(port,'/study/v1/icons/buy',b' {"icon_\\u0069d":"ruby"} ',token)[0]==200
            with sqlite3.connect(db) as con:assert con.execute('SELECT bp FROM wallets WHERE player_id=?',(pid,)).fetchone()==(200,)
    print('HTTP: malformed bodies cannot create accounts/debit/grant; byte limit and escaped key positive controls passed',flush=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'117-icon-ownership'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/wire.h','meta/shop_catalog.h','meta/account_service.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:
            assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            targets=['json_boundary_contract','study_account_db']
            if backend=='SCRIPTED':targets+=['study_meta_db','shop_contract','account_contract','idempotency_contract','progression_contract','settlement_probe']
            result=run(['cmake','--build',str(build),'--target',*targets,'-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            for line in (result.stdout+result.stderr).splitlines():
                if 'warning:' in line:assert '/third_party/sqlite3.c:' in line and any(x in line for x in ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),line
            print(run(['ctest','--test-dir',str(build),'-R','^json_boundary_contract$','--output-on-failure']).stdout,flush=True)
            boundary_http(build)
            if backend=='SCRIPTED':
                for target in ['shop_contract','account_contract','idempotency_contract','progression_contract']:
                    with tempfile.TemporaryDirectory(prefix='json-contract-') as tmp:print(run([str(build/target),str(Path(tmp)/'fixture-')]).stdout,flush=True)
                account_http(build);shop_http(build);settlement_contract(build)
        for name,source,include in [('study-json',CP/'tests/json_boundary_contract.cpp',CP),('current-json',ROOT/'tests/learning/current_json_boundary.cpp',ROOT)]:
            exe=OUT/name
            run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
                 '-I'+str(include),'-isystem',str(ROOT/'third_party'),str(source),'-o',str(exe)])
            print(run([str(exe)]).stdout,flush=True)
    lesson=ROOT/'docs/learn/lessons/118.json'
    if lesson.exists():
        corpora={lang:[normalized(p.read_text(),lang) for p in CP.rglob('*') if p.is_file() and
            (p.suffix in ['.cpp','.h'] if lang=='cpp' else p.name=='CMakeLists.txt')] for lang in ['cpp','cmake']}
        n=0
        for section in json.loads(lesson.read_text())['sections']:
            for code in section.get('codes',[]):
                if 'text' in code and code['language'] in corpora:
                    assert any(normalized(code['text'],code['language']) in c for c in corpora[code['language']]),code['label'];n+=1
        print('inline snippets',n)
if __name__=='__main__':main()
