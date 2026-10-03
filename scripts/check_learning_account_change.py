"""Lesson141: atomic credential changes in the cumulative account service."""
from pathlib import Path
import argparse,json,os,tempfile,subprocess,sqlite3,urllib.request,urllib.error,contextlib,secrets,concurrent.futures
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1];CP=R/'docs/learn/checkpoints/141-account-change';OUT=R/'out/learning-checkpoints/141-account-change-check'
@contextlib.contextmanager
def server(exe,db):
    p=subprocess.Popen([str(exe),'0',str(db)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        import selectors
        sel=selectors.DefaultSelector();sel.register(p.stdout,selectors.EVENT_READ)
        try:assert sel.select(10),'service start timeout'
        finally:sel.close()
        line=p.stdout.readline();assert line.startswith('PORT '),line
        yield 'http://127.0.0.1:'+line.split()[1]
    finally:
        p.terminate();p.communicate(timeout=10)
def request(base,path,data=None,token=None,raw=None):
    headers={'Content-Type':'application/json'}
    if token:headers['Authorization']='Bearer '+token
    body=raw if raw is not None else json.dumps(data).encode()if data is not None else None
    req=urllib.request.Request(base+path,data=body,headers=headers)
    try:r=urllib.request.urlopen(req,timeout=8)
    except urllib.error.HTTPError as e:r=e
    with r:return r.status,json.loads(r.read()),r.headers
def http_contract(exe):
    with tempfile.TemporaryDirectory(prefix='study141-http-')as tmp:
        db=Path(tmp)/'account.db'
        with server(exe,db)as base:
            status,guest,_=request(base,'/study/v1/guest',{});assert status==201;old=guest['token'];player=guest['player_id']
            def call(kind,proof,next_token,code):return request(base,'/study/v1/account/'+kind,{'credential':proof,'next_token':next_token,'next_recovery':code})
            rec='rc1.'+secrets.token_hex(32)
            assert call('backup',old,old,rec)[0]==200
            candidates=[(secrets.token_hex(16),'rc1.'+secrets.token_hex(32))for _ in range(4)]
            with concurrent.futures.ThreadPoolExecutor(4)as pool:results=list(pool.map(lambda pair:call('recover',rec,*pair),candidates))
            assert [r[0]for r in results].count(200)==1 and [r[0]for r in results].count(401)==3
            winner=candidates[next(i for i,r in enumerate(results)if r[0]==200)];new,code=winner
            status,ack,headers=call('recover',rec,new,code);assert status==200 and ack=={'player_id':player,'auth_epoch':2}and headers['Cache-Control']=='no-store'
            assert request(base,'/study/v1/me',token=old)[0]==401
            assert request(base,'/study/v1/me',token=new)[1]['player_id']==player
            assert call('recover',rec,*candidates[(candidates.index(winner)+1)%len(candidates)])[0]==401
            with sqlite3.connect(db)as con:
                row=con.execute('SELECT token_hash,recovery_hash,auth_epoch,last_change_hash FROM account_keys WHERE player_id=?',(str(player),)).fetchone()
                assert row[2]==2 and old not in row and new not in row and code not in row
                con.execute("CREATE TRIGGER reject_change BEFORE UPDATE ON account_keys BEGIN SELECT RAISE(ABORT,'injected');END")
            fresh,backup=secrets.token_hex(16),'rc1.'+secrets.token_hex(32)
            assert call('rotate',new,fresh,backup)[0]==503
            assert request(base,'/study/v1/me',token=new)[0]==200
            with sqlite3.connect(db)as con:con.execute('DROP TRIGGER reject_change')
            bad=b'{"credential":"x","credential":"y","next_token":"z","next_recovery":"w"}'
            assert request(base,'/study/v1/account/rotate',raw=bad)[0]==400
            assert call('rotate',new,fresh,backup)[0]==200
            assert call('recover',rec,new,code)[0]==401
        with server(exe,db)as base:assert request(base,'/study/v1/me',token=fresh)[1]['player_id']==player
    print('HTTP: strict input, no-store, one recovery winner, receipt retry, trigger rollback, old receipt expiry and restart passed',flush=True)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--boost',default=os.environ.get('STUDY_BOOST_INCLUDE','/usr/include'));ap.add_argument('--snippets-only',action='store_true');a=ap.parse_args()
    changed={'README.md','CMakeLists.txt','meta/migrations.h','meta/credential_lab.h','meta/sqlite_results.h','meta/account_service.cpp','tests/migration_contract.cpp','tests/progression_contract.cpp','tests/shop_contract.cpp'}
    prev=CP.parent/'140-credential-migration'
    for p in prev.rglob('*'):
        if p.is_file()and p.relative_to(prev).as_posix()not in changed:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not a.snippets_only:
        b=OUT/'scripted'
        run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM=SCRIPTED','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(a.boost).resolve())])
        targets=['study_account_db','account_contract','migration_contract','shop_contract','progression_contract','account_change_contract']
        result=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300);(OUT/'build.log').write_text(result.stdout+result.stderr)
        with tempfile.TemporaryDirectory(prefix='study141-contract-')as tmp:
            for name in targets[1:]:print(name,run([str(b/name),str(Path(tmp)/(name+'.db'))],timeout=30).stdout,flush=True)
        http_contract(b/'study_account_db')
        exe=OUT/'change-sanitized'
        run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(R/'third_party'),str(CP/'tests/account_change_contract.cpp'),str(b/'libstudy_sqlite.a'),'-lcrypto','-ldl','-pthread','-o',str(exe)],timeout=120)
        with tempfile.TemporaryDirectory(prefix='study141-sanitized-')as tmp:print('ASan/UBSan',run([str(exe),str(Path(tmp)/'account.db')]).stdout,flush=True)
    f=R/'docs/learn/lessons/141.json'
    if f.exists():
        corpus={lang:[normalized(p.read_text(),lang)for p in CP.rglob('*')if p.is_file()and(p.suffix in {'.h','.cpp'}if lang=='cpp'else p.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};count=0
        for s in json.loads(f.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for c in s.get('codes',[]):
                if 'text'in c and c['language']in corpus:assert any(normalized(c['text'],c['language'])in text for text in corpus[c['language']]),c['label'];count+=1
        print('Inline snippets',count,'current symbols and cumulative preservation verified',flush=True)
if __name__=='__main__':main()
