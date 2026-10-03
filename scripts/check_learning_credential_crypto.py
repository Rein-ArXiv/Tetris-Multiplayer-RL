"""Lesson139: credential generation, hashing, lookup and comparison contracts."""
from pathlib import Path
import argparse, json, os, tempfile
from check_learning_text_layout import run
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/139-credential-crypto'
OUT=R/'out/learning-checkpoints/139-credential-crypto-check'
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--boost',default=os.environ.get('STUDY_BOOST_INCLUDE','/usr/include'))
    ap.add_argument('--snippets-only',action='store_true');args=ap.parse_args()
    prev=CP.parent/'138-async-tls'
    for p in prev.rglob('*'):
        if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','CMakeLists.txt'}:
            assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        for backend in ['SCRIPTED','SDL']:
            b=OUT/backend.lower()
            run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM='+backend,'-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release',f'-DSTUDY_BOOST_INCLUDE={Path(args.boost).resolve()}'])
            targets=['credential_crypto_contract','account_contract']+(['tetris']if backend=='SDL'else[])
            build=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300)
            (OUT/f'{backend}-build.log').write_text(build.stdout+build.stderr)
            warnings=[line for line in (build.stdout+build.stderr).splitlines() if 'warning:' in line]
            allowed=("warning: assignment discards ‘const’ qualifier from pointer target type", "warning: ‘strlen’ reading 1 or more bytes from a region of size 0")
            for line in warnings:
                assert '/third_party/sqlite3.c:' in line and any(w in line for w in allowed),line
            if warnings:print(backend,'external SQLite warnings retained in build log:',*warnings,sep='\n',flush=True)
            print(backend,run(['ctest','--test-dir',str(b),'-R','^credential_crypto_contract$','--output-on-failure']).stdout,flush=True)
            with tempfile.TemporaryDirectory(prefix='study139-account-') as tmp:
                print(backend,run([str(b/'account_contract'),str(Path(tmp)/'account.db')]).stdout,flush=True)
        for name,sources,include in [('study',[CP/'tests/credential_crypto_contract.cpp'],CP),('current',[R/'tests/credentials_test.cpp',R/'meta/credentials.cpp'],R)]:
            exe=OUT/(name+'-sanitized')
            run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(include),*[str(p)for p in sources],'-lcrypto','-o',str(exe)],timeout=90)
            print(name,'ASan/UBSan',run([str(exe)]).stdout,flush=True)
    f=R/'docs/learn/lessons/139.json'
    if f.exists():
        corpus=[normalized(p.read_text(),'cpp')for p in CP.rglob('*')if p.suffix in {'.h','.cpp'}];count=0
        for s in json.loads(f.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for c in s.get('codes',[]):
                if 'text'in c and c['language']=='cpp':
                    assert any(normalized(c['text'],'cpp')in text for text in corpus),c['label'];count+=1
        print('Inline snippets',count,'current symbols and cumulative files verified',flush=True)
if __name__=='__main__':main()
