"""Lesson140: logical rollback, abrupt restart, WAL readers and local file scrub."""
from pathlib import Path
import argparse, json, os, sqlite3, tempfile, subprocess
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1];CP=R/'docs/learn/checkpoints/140-credential-migration'
OUT=R/'out/learning-checkpoints/140-credential-migration-check'
def experiment(exe):
    secret=b'a'*32  # Public CLI fixture, not an issued token.
    for mode in ['crash-row','crash-commit','crash-vacuum']:
        with tempfile.TemporaryDirectory(prefix='study140-crash-') as tmp:
            db=Path(tmp)/'legacy.db';run([str(exe),'init',str(db)])
            backup=Path(tmp)/'external-backup.db';backup.write_bytes(db.read_bytes())
            assert secret in backup.read_bytes()
            result=subprocess.run([str(exe),mode,str(db)],capture_output=True,timeout=10)
            assert result.returncode==73 and secret not in result.stdout+result.stderr,(mode,result.returncode)
            with sqlite3.connect(db)as con:
                cols={r[1]for r in con.execute('PRAGMA table_info(players)')}
                if mode=='crash-row':assert 'token'in cols and con.execute('SELECT token FROM players').fetchone()[0].encode()==secret
                else:
                    assert 'token_hash'in cols
                    assert {r[0]for r in con.execute('SELECT name FROM credential_steps')}=={'hash_v1'}
            for _ in range(2):run([str(exe),'migrate',str(db)])
            with sqlite3.connect(db)as con:
                assert con.execute('SELECT id,bp FROM players').fetchone()==(7,80)
                assert {r[0]for r in con.execute('SELECT name FROM credential_steps')}=={'hash_v1','scrub_v1'}
            for file in [db,Path(str(db)+'-wal')]:
                if file.exists():assert secret not in file.read_bytes()
            assert secret in backup.read_bytes() # Scrubbing current files cannot erase another copy.
            print(mode,'restart and retained external backup passed',flush=True)
    with tempfile.TemporaryDirectory(prefix='study140-reader-')as tmp:
        db=Path(tmp)/'legacy.db';run([str(exe),'init',str(db)])
        with sqlite3.connect(db)as con:con.execute('PRAGMA journal_mode=WAL')
        reader=sqlite3.connect(db)
        try:
            reader.execute('BEGIN');reader.execute('SELECT token FROM players').fetchall()
            result=subprocess.run([str(exe),'migrate',str(db)],capture_output=True,timeout=10)
            assert result.returncode==1
            with sqlite3.connect(db)as con:
                assert {r[0]for r in con.execute('SELECT name FROM credential_steps')}=={'hash_v1'}
            assert reader.execute('SELECT token FROM players').fetchone()[0].encode()==secret
        finally:reader.rollback();reader.close()
        run([str(exe),'migrate',str(db)]);print('WAL reader: committed hashes, failed scrub, retry after release passed',flush=True)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--boost',default=os.environ.get('STUDY_BOOST_INCLUDE','/usr/include'));ap.add_argument('--snippets-only',action='store_true');a=ap.parse_args()
    prev=CP.parent/'139-credential-crypto'
    for p in prev.rglob('*'):
        if p.is_file()and p.relative_to(prev).as_posix()not in {'README.md','CMakeLists.txt'}:assert p.read_bytes()==(CP/p.relative_to(prev)).read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not a.snippets_only:
        b=OUT/'scripted'
        run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM=SCRIPTED','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DSTUDY_BOOST_INCLUDE='+str(Path(a.boost).resolve())])
        r=run(['cmake','--build',str(b),'--target','credential_migration','credential_migration_contract','-j2'],timeout=300);(OUT/'build.log').write_text(r.stdout+r.stderr)
        with tempfile.TemporaryDirectory(prefix='study140-contract-')as tmp:print(run([str(b/'credential_migration_contract'),str(Path(tmp)/'cases')]).stdout,flush=True)
        experiment(b/'credential_migration')
        exe=OUT/'migration-sanitized'
        run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),'-I'+str(R/'third_party'),str(CP/'tests/credential_migration_contract.cpp'),str(b/'libstudy_sqlite.a'),'-lcrypto','-ldl','-pthread','-o',str(exe)],timeout=90)
        with tempfile.TemporaryDirectory(prefix='study140-sanitized-')as tmp:print('ASan/UBSan',run([str(exe),str(Path(tmp)/'cases')]).stdout,flush=True)
    f=R/'docs/learn/lessons/140.json'
    if f.exists():
        corpus=[normalized(p.read_text(),'cpp')for p in CP.rglob('*')if p.suffix in {'.h','.cpp'}];count=0
        for section in json.loads(f.read_text())['sections']:
            ref=section.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for c in section.get('codes',[]):
                if 'text'in c and c['language']=='cpp':assert any(normalized(c['text'],'cpp')in s for s in corpus),c['label'];count+=1
        print('Inline snippets',count,'symbols and cumulative files verified',flush=True)
if __name__=='__main__':main()
