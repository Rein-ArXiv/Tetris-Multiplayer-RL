"""Lesson117: authoritative shop prices, ownership and serialized mutations."""
from pathlib import Path
import json,subprocess,tempfile,sys,urllib.request,urllib.error
from concurrent.futures import ThreadPoolExecutor
from check_learning_text_layout import run
from check_learning_tables_keys import service
from check_learning_transactions import process_contract
from check_learning_migrations import legacy
from check_part_docs import normalized
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/117-icon-ownership'
OUT=ROOT/'out/learning-checkpoints/117-icon-ownership-check'

def post(port,record,version=2):
    body=record if isinstance(record,str) else json.dumps(record)
    req=urllib.request.Request(f'http://127.0.0.1:{port}/study/v{version}/matches',body.encode(),{'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(req,timeout=8) as response:return response.status,json.loads(response.read())
    except urllib.error.HTTPError as e:return e.code,json.loads(e.read())

def settlement_contract(build):
    with tempfile.TemporaryDirectory(prefix='settlement-114-') as tmp:
        db=Path(tmp)/'study.db'
        record=dict(key=17,round=1,player_a=101,player_b=202,ticks=11,score_a=100,score_b=0,lines_a=0,lines_b=0,winner=1)
        with service(build/'study_meta_db',db) as port:
            first=post(port,record);assert first[0]==200 and first[1]['bp_a']==10 and first[1]['bp_b']==3
            assert post(port,dict(record,key=18))[0]==200
            assert post(port,json.dumps(dict(reversed(list(record.items()))),indent=4))==first
            for field in record:
                if field=='key':continue
                changed=dict(record);changed[field]+=1;assert post(port,changed)[0]==409,field
            with ThreadPoolExecutor(max_workers=8) as pool:assert list(pool.map(lambda _:post(port,record),range(8)))==[first]*8
            expected={k:v for k,v in first[1].items() if k not in ['bp_a','bp_b','policy']}
            assert post(port,record,1)==(200,expected)
            assert run([str(build/'settlement_probe'),str(port),'17']).stdout=='row=1 awards=10,3 policy=1\n'
            wide=dict(record,key=2**64-1);assert post(port,wide)[0]==200
            assert run([str(build/'settlement_probe'),str(port),str(2**64-1)]).stdout.endswith('awards=10,3 policy=1\n')
        with service(build/'study_meta_db',db) as port:assert post(port,record)==first
        old=Path(tmp)/'old.db';legacy(old)
        with service(build/'study_meta_db',old) as port:
            old_result=post(port,record);assert old_result[0]==200
            assert (old_result[1]['policy'],old_result[1]['bp_a'],old_result[1]['bp_b'])==(0,0,0)
    print('HTTP v1 compatibility/v2 original awards, semantic JSON, all-field conflicts, concurrency, UINT64 and restart/legacy: passed',flush=True)

def account_http(build):
    import sqlite3,hashlib,http.client
    def request(port,path,body=None,auth=None):
        data=None if body is None else body.encode()
        headers={"Content-Type":"application/json"}
        if auth is not None:headers['Authorization']=auth
        req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,data,headers)
        try:
            with urllib.request.urlopen(req,timeout=5) as r:return r.status,json.loads(r.read()),r.headers
        except urllib.error.HTTPError as e:return e.code,json.loads(e.read()),e.headers
    with tempfile.TemporaryDirectory(prefix='account-116-') as tmp:
        db=Path(tmp)/'study.db'
        with service(build/'study_account_db',db) as port:
            status,guest,headers=request(port,'/study/v1/guest','{}')
            assert status==201 and headers['Cache-Control']=='no-store'
            assert len(guest['token'])==32 and 0<guest['player_id']<=2**64-1
            auth='Bearer '+guest['token'];profile=request(port,'/study/v1/me',auth=auth)
            assert profile[0]==200 and profile[1]==dict(player_id=guest['player_id'],rp=0,xp=0,bp=0)
            assert profile[2]['Cache-Control']=='no-store' and guest['token'] not in json.dumps(profile[1])
            second=request(port,'/study/v1/guest','{}')[1]
            assert second['player_id']!=guest['player_id'] and second['token']!=guest['token']
            assert request(port,'/study/v1/me',auth='Bearer '+second['token'])[1]['player_id']==second['player_id']
            digest=hashlib.sha256(('study-account-v1:'+guest['token']).encode()).hexdigest()
            for bad in [None,'Bearer '+str(guest['player_id']),'Bearer '+digest,'Bearer '+'0'*32,'Basic '+guest['token']]:
                assert request(port,'/study/v1/me',auth=bad)[0]==401
            assert request(port,'/study/v1/me?player_id='+str(second['player_id']),auth=auth)[0]==400
            assert request(port,'/study/v1/me?token='+guest['token'])[0]==400
            for bad in ['null','[]','{"player_id":1}','{"token":"chosen"}','{"a":1,"a":2}']:
                assert request(port,'/study/v1/guest',bad)[0]==400
            connection=http.client.HTTPConnection('127.0.0.1',port,timeout=5)
            connection.putrequest('GET','/study/v1/me');connection.putheader('Authorization',auth);connection.putheader('Authorization',auth);connection.endheaders()
            response=connection.getresponse();assert response.status==401;response.read();connection.close()
            with sqlite3.connect(db) as con:
                assert con.execute('SELECT token_hash FROM account_keys WHERE player_id=?',(str(guest['player_id']),)).fetchone()==(digest,)
                assert con.execute('SELECT count(*) FROM players').fetchone()[0]==2
                assert guest['token'] not in '\n'.join(con.iterdump())
                con.execute("CREATE TRIGGER fail_guest BEFORE INSERT ON account_keys BEGIN SELECT RAISE(ABORT,'injected'); END")
            assert request(port,'/study/v1/guest','{}')[0]==503
            with sqlite3.connect(db) as con:
                assert con.execute('SELECT count(*) FROM players').fetchone()[0]==2
                con.execute('DROP TRIGGER fail_guest')
            run([sys.executable,str(CP/'tools/account_probe.py'),str(port)])
        with service(build/'study_account_db',db) as port:assert request(port,'/study/v1/me',auth=auth)[:2]==profile[:2]
    print('Account HTTP: own identity, ID/hash rejection, duplicate headers/query rejection, no-store, hash-only DB, rollback, restart and memory-only probe: passed',flush=True)

def shop_http(build):
    import sqlite3
    def request(port,path,token=None,body=None):
        headers={"Content-Type":"application/json"}
        if token:headers['Authorization']='Bearer '+token
        data=None if body is None else json.dumps(body).encode()
        req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,data,headers)
        try:
            with urllib.request.urlopen(req,timeout=5) as r:return r.status,json.load(r)
        except urllib.error.HTTPError as e:return e.code,json.load(e)
    with tempfile.TemporaryDirectory(prefix='shop-http-117-') as tmp:
        db=Path(tmp)/'study.db'
        with service(build/'study_account_db',db) as port:
            a=request(port,'/study/v1/guest',body={})[1];b=request(port,'/study/v1/guest',body={})[1]
            tok=a['token'];own=lambda:request(port,'/study/v1/inventory',tok)
            assert own()[1]['owned']==['default'] and own()[1]['selected_icon_id']=='default'
            assert request(port,'/study/v1/inventory')[0]==401
            catalog=request(port,'/study/v1/icons/catalog');assert catalog[0]==200
            assert [(i['id'],i['price_bp']) for i in catalog[1]]==[('default',0),('ruby',100),('gold',250)]
            buy='/study/v1/icons/buy';choose='/study/v1/icons/select'
            assert request(port,choose,tok,{'icon_id':'ruby'})[0]==403
            assert request(port,buy,tok,{'icon_id':'ruby'})[0]==402
            with sqlite3.connect(db) as con:con.execute('UPDATE wallets SET bp=300 WHERE player_id=?',(str(a['player_id']),))
            for field in ['price','price_bp','player_id','owned']:
                assert request(port,buy,tok,dict(icon_id='ruby',**{field:0}))[0]==400
            assert own()[1]['bp']==300
            with ThreadPoolExecutor(max_workers=2) as pool:
                responses=list(pool.map(lambda _:request(port,buy,tok,{'icon_id':'ruby'}),range(2)))
            assert sorted(x[0] for x in responses)==[200,409]
            assert own()[1]['bp']==200 and own()[1]['owned']==['default','ruby']
            assert own()[1]['selected_icon_id']=='default'
            assert request(port,choose,b['token'],{'icon_id':'ruby'})[0]==403
            assert request(port,choose,tok,{'icon_id':'ruby'})[1]['selected_icon_id']=='ruby'
            assert request(port,choose,tok,{'icon_id':'ruby'})[0]==200 and own()[1]['bp']==200
            run([sys.executable,str(CP/'tools/shop_probe.py'),str(port)])
        with service(build/'study_account_db',db) as port:
            result=request(port,'/study/v1/inventory',tok)
            assert result[1]['bp']==200 and result[1]['selected_icon_id']=='ruby'
    print('Shop HTTP: server catalog/price, extra-field rejection, concurrent debit once, ownership isolation, selection/retry and restart: passed',flush=True)

def root_contract(archive):
    exe=OUT/'current-shop'
    run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
         '-I'+str(ROOT),'-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(ROOT/'tests/learning/current_shop.cpp'),
         str(ROOT/'meta/credentials.cpp'),str(archive),'-lcrypto','-ldl','-o',str(exe)])
    with tempfile.TemporaryDirectory(prefix='root-guest-') as tmp:print(run([str(exe),str(Path(tmp)/'fixture-')]).stdout,flush=True)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    old=CP.parent/'116-guest-account'
    changed={'README.md','DESIGN.md','CMakeLists.txt','meta/sqlite_results.h','meta/account_service.cpp'}
    for p in old.rglob('*'):
        if p.is_file() and p.relative_to(old).as_posix() not in changed:assert p.read_bytes()==(CP/p.relative_to(old)).read_bytes(),p
    if '--snippets-only' not in sys.argv:
        for backend in ['SCRIPTED','SDL']:
            build=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(build),'-DSTUDY_PLATFORM='+backend,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(build),'--target','study_meta_db','sqlite_tables_contract','index_probe','migration_contract','reward_contract','reward_probe','idempotency_contract','settlement_probe','progression_contract','progression_probe','study_account_db','account_contract','shop_contract','-j2'])
            (OUT/('build-'+backend+'.log')).write_text(result.stdout+result.stderr)
            for line in (result.stdout+result.stderr).splitlines():
                if 'warning:' in line:assert '/third_party/sqlite3.c:' in line and any(x in line for x in ['[-Wdiscarded-qualifiers]','[-Wstringop-overread]']),line
            print(run(['ctest','--test-dir',str(build),'-R','^(sqlite_tables_contract|index_query_contract)$','--output-on-failure']).stdout,flush=True)
            for name in ['migration_contract','reward_contract','idempotency_contract','progression_contract','account_contract','shop_contract']:
                with tempfile.TemporaryDirectory(prefix='idempotency-contract-') as tmp:print(run([str(build/name),str(Path(tmp)/'fixture-')]).stdout,flush=True)
            process_contract(build);settlement_contract(build);account_http(build);shop_http(build)
        exe=OUT/'shop-sanitized';archive=OUT/'scripted/libstudy_sqlite.a'
        run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CP),'-isystem',str(ROOT/'third_party'),str(CP/'tests/shop_contract.cpp'),str(archive),'-ldl','-lcrypto','-o',str(exe)])
        with tempfile.TemporaryDirectory(prefix='idempotency-asan-') as tmp:print(run([str(exe),str(Path(tmp)/'test.db')]).stdout,flush=True)
        root_contract(archive)
    lesson=ROOT/'docs/learn/lessons/117.json'
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
