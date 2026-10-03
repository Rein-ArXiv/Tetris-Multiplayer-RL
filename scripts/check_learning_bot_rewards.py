"""Lesson147: verified PvE replay, atomic shared BP and durable capped receipts."""
from pathlib import Path
import argparse,json,sqlite3,tempfile
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/147-bot-rewards'
OUT=R/'out/learning-checkpoints/147-bot-rewards-check'

def contract(exe):
    with tempfile.TemporaryDirectory() as folder:
        print(run([str(exe),str(Path(folder)/'bot.db')],timeout=40).stdout,flush=True)
def probe(exe):
    with tempfile.TemporaryDirectory() as folder:
        db=Path(folder)/'bot.db';run([str(exe),str(db)])
        def rows():
            with sqlite3.connect(db) as c:
                return {n:c.execute('SELECT * FROM '+n+' ORDER BY 1').fetchall()for n in ['bot_rewards','wallets','careers','matches']}
        before=rows();assert len(before['bot_rewards'])==1 and not before['matches']
        run([str(exe),str(db)]);assert rows()==before
        print('new process: same bot receipt and shared wallet, no RP/XP/match mutation',flush=True)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--boost',default='/usr/include');ap.add_argument('--snippets-only',action='store_true');a=ap.parse_args()
    allowed={'README.md','CMakeLists.txt','meta/migrations.h','meta/sqlite_results.h','tests/migration_contract.cpp'}
    for p in CP.with_name('146-result-notices').rglob('*'):
        if p.is_file() and p.relative_to(CP.with_name('146-result-notices')).as_posix()not in allowed:
            assert p.read_bytes()==(CP/p.relative_to(CP.with_name('146-result-notices'))).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not a.snippets_only:
        targets=['bot_reward_contract','bot_reward_probe','migration_contract','reward_contract','result_notice_contract']
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(a.boost).resolve()),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            r=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300);(OUT/(backend.lower()+'-build.log')).write_text(r.stdout+r.stderr)
            contract(b/'bot_reward_contract');probe(b/'bot_reward_probe')
            for name in ['migration_contract','reward_contract']:contract(b/name)
            print(backend,run(['ctest','--test-dir',str(b),'-R','^result_notice_contract$','--no-tests=error','--output-on-failure'],timeout=60).stdout,flush=True)
        for name,source in [('contract','tests/bot_reward_contract.cpp'),('probe','tools/bot_reward_probe.cpp')]:
            exe=OUT/(name+'-sanitized')
            r=run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(R/'third_party'),str(CP/source),str(OUT/'scripted/libstudy_sqlite.a'),'-pthread','-lcrypto','-ldl','-o',str(exe)],timeout=240)
            (OUT/(name+'-sanitized-build.log')).write_text(r.stdout+r.stderr)
            (contract if name=='contract'else probe)(exe)
    p=R/'docs/learn/lessons/147.json'
    if p.exists():
        corpus=[normalized(f.read_text(),'cpp')for f in CP.rglob('*')if f.suffix in {'.h','.cpp'}];count=0
        for s in json.loads(p.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for c in s.get('codes',[]):
                if c.get('language')=='cpp'and'text'in c:assert any(normalized(c['text'],'cpp')in src for src in corpus),c['label'];count+=1
        print('Inline snippets',count,'current references and cumulative preservation passed',flush=True)
if __name__=='__main__':main()
