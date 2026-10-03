"""Lesson145: shared result policy, mutex owner and event-loop completion owner."""
from pathlib import Path
import argparse,json,sqlite3,tempfile
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/145-relay-ownership'
OUT=R/'out/learning-checkpoints/145-relay-ownership-check'

def database_probe(executable):
    with tempfile.TemporaryDirectory() as folder:
        db=Path(folder)/'results.db'
        run([str(executable),str(db)],timeout=30)
        with sqlite3.connect(db) as c:
            before={name:c.execute('SELECT * FROM '+name+' ORDER BY 1').fetchall()
                    for name in ['matches','wallets','careers','match_rewards','match_progress']}
        assert len(before['matches'])==2
        run([str(executable),str(db)],timeout=30)
        with sqlite3.connect(db) as c:
            after={name:c.execute('SELECT * FROM '+name+' ORDER BY 1').fetchall() for name in before}
        assert before==after
        print('real SQLite: two owner models, stable keys, identical results/rewards/progression after replay',flush=True)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--boost',default='/usr/include');ap.add_argument('--snippets-only',action='store_true');args=ap.parse_args()
    previous=CP.parent/'144-input-pacing'
    for p in previous.rglob('*'):
        if p.is_file() and p.relative_to(previous).as_posix() not in {'README.md','CMakeLists.txt'}:
            assert p.read_bytes()==(CP/p.relative_to(previous)).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        targets=['relay_owner_contract','relay_owner_probe','pacing_contract','authoritative_contract','offload_contract','match_submission_contract']
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(args.boost).resolve()),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
            result=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300)
            (OUT/(backend.lower()+'-build.log')).write_text(result.stdout+result.stderr)
            pattern='^('+'|'.join(targets)+')$'
            print(backend,run(['ctest','--test-dir',str(b),'-R',pattern,'--no-tests=error','--output-on-failure'],timeout=60).stdout,flush=True)
            database_probe(b/'relay_owner_probe')
        for name,source in [('contract','tests/relay_owner_contract.cpp'),('probe','tools/relay_owner_probe.cpp')]:
            exe=OUT/(name+'-sanitized')
            cmd=['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/source),'-pthread']
            if name=='probe':cmd+=['-isystem',str(R/'third_party'),str(OUT/'scripted/libstudy_sqlite.a'),'-lcrypto','-ldl']
            result=run(cmd+['-o',str(exe)],timeout=240);(OUT/(name+'-sanitized-build.log')).write_text(result.stdout+result.stderr)
            if name=='probe':database_probe(exe)
            else:print('ASan/UBSan',run([str(exe)],timeout=30).stdout,flush=True)
    lesson=R/'docs/learn/lessons/145.json'
    if lesson.exists():
        corpus=[normalized(p.read_text(),'cpp')for p in CP.rglob('*')if p.suffix in {'.h','.cpp'}]
        count=0
        for s in json.loads(lesson.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for code in s.get('codes',[]):
                if 'text'in code and code['language']=='cpp':
                    assert any(normalized(code['text'],'cpp')in source for source in corpus),code['label'];count+=1
        print('Inline snippets',count,'current symbols, cumulative files passed',flush=True)
if __name__=='__main__':main()
