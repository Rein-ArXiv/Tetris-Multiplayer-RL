"""Lesson148: bounded read-only observations, sample scope and false-positive boundaries."""
from pathlib import Path
import argparse,json,sqlite3,tempfile
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/148-abuse-review'
OUT=R/'out/learning-checkpoints/148-abuse-review-check'
def contract(exe):
    with tempfile.TemporaryDirectory() as folder:print(run([str(exe),str(Path(folder)/'review.db')],timeout=40).stdout,flush=True)
def probe(fixture,reader):
    with tempfile.TemporaryDirectory() as folder:
        db=Path(folder)/'review.db';run([str(fixture),str(db)])
        def snapshot():
            with sqlite3.connect(db)as c:
                names=[r[0]for r in c.execute("SELECT name FROM sqlite_schema WHERE type='table' AND name NOT GLOB 'sqlite_*' ORDER BY name")]
                return {n:c.execute('SELECT * FROM '+n+' ORDER BY 1').fetchall()for n in names}
        before=snapshot();assert len(before['matches'])==5
        report=run([str(reader),str(db),'3','3','2','2','1000']).stdout
        assert '표본=3' in report and '이전 행 있음=1'in report and '반복 상대'in report
        assert snapshot()==before
        run([str(fixture),str(db)]);assert snapshot()==before
        report=run([str(reader),str(db),'8','3','2','2','1000']).stdout
        assert '표본=5'in report and '이전 행 있음=0'in report and snapshot()==before
        print('operator reader: full DB rows unchanged, bounded sample metadata, repeated result keys unchanged',flush=True)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--boost',default='/usr/include');ap.add_argument('--snippets-only',action='store_true');a=ap.parse_args()
    previous=CP.with_name('147-bot-rewards')
    for p in previous.rglob('*'):
        if p.is_file()and p.relative_to(previous).as_posix()not in {'README.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(previous)).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not a.snippets_only:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(a.boost).resolve()),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            r=run(['cmake','--build',str(b),'--target','review_contract','review_results','review_fixture','bot_reward_contract','-j2'],timeout=300);(OUT/(backend.lower()+'-build.log')).write_text(r.stdout+r.stderr)
            contract(b/'review_contract');contract(b/'bot_reward_contract');probe(b/'review_fixture',b/'review_results')
        for name,source in [('contract','tests/review_contract.cpp'),('reader','tools/review_results.cpp')]:
            exe=OUT/(name+'-sanitized')
            r=run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-isystem',str(R/'third_party'),str(CP/source),str(OUT/'scripted/libstudy_sqlite.a'),'-pthread','-lcrypto','-ldl','-o',str(exe)],timeout=240)
            (OUT/(name+'-sanitized-build.log')).write_text(r.stdout+r.stderr)
            if name=='contract':contract(exe)
            else:probe(OUT/'scripted/review_fixture',exe)
    p=R/'docs/learn/lessons/148.json'
    if p.exists():
        corpus=[normalized(f.read_text(),'cpp')for f in CP.rglob('*')if f.suffix in {'.h','.cpp'}];count=0
        for s in json.loads(p.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for c in s.get('codes',[]):
                if c.get('language')=='cpp'and'text'in c:assert any(normalized(c['text'],'cpp')in src for src in corpus),c['label'];count+=1
        print('Inline snippets',count,'current references and cumulative preservation passed',flush=True)
if __name__=='__main__':main()
