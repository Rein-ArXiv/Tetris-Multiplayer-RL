"""Lesson135: two connection roles, actual HTTPS identity checks, no downgrade."""
from pathlib import Path
import json, os, shutil, sys, tempfile
from check_learning_text_layout import run
from check_part_docs import normalized
from learning_tls_fixture import certificates, matrix
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/135-secure-connections';OUT=R/'out/learning-checkpoints/135-secure-connections-check'
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    prev=CP.parent/'134-threat-model'
    for p in prev.rglob('*'):
        if p.is_file() and p.relative_to(prev).as_posix() not in {'README.md','CMakeLists.txt'}:
            assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
    assert (CP/'net/tls_identity.h').read_text()==(R/'meta/tls_identity.h').read_text().replace('meta::client::tls_identity','study_net::tls_identity')
    if '--snippets-only' not in sys.argv:
        with tempfile.TemporaryDirectory(prefix='study135-tls-') as tmp:
            certs=certificates(Path(tmp))
            for backend in ['SCRIPTED','SDL']:
                b=OUT/backend.lower()
                run(['cmake','-S',str(CP),'-B',str(b),f'-DSTUDY_PLATFORM={backend}','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'])
                targets=['connection_policy_contract','https_probe','threat_model_contract','threat_model_probe','http_retry_contract','meta_service_contract']+(['tetris'] if backend=='SDL' else [])
                p=run(['cmake','--build',str(b),'--target',*targets,'-j2'],timeout=300);(OUT/f'build-{backend}.log').write_text(p.stdout+p.stderr)
                print(backend,run(['ctest','--test-dir',str(b),'-R','^(connection_policy_contract|threat_model_contract|threat_model_probe|epoll_threat_model_probe|http_retry_contract|meta_service_contract)$','--output-on-failure'],timeout=45).stdout,flush=True)
                matrix(b/'https_probe',certs)
            flags=['c++','-std=c++17','-pthread','-Wall','-Wextra','-Wpedantic','-I'+str(R),'-isystem',str(R/'third_party')]
            exe=OUT/'root-after'
            p=run([*flags,'-DCPPHTTPLIB_OPENSSL_SUPPORT',str(R/'tests/learning/current_https.cpp'),str(R/'meta/http_client.cpp'),'-lssl','-lcrypto','-o',str(exe)],timeout=180);(OUT/'root-compile.log').write_text(p.stdout+p.stderr)
            matrix(exe,certs,root=True)
            # A non-TLS build must reject HTTPS at construction, before network I/O.
            exe=OUT/'root-no-tls'
            run([*flags,str(R/'tests/learning/current_https.cpp'),str(R/'meta/http_client.cpp'),'-o',str(exe)],timeout=180)
            import subprocess
            p=subprocess.run([str(exe),'https://localhost:443'],capture_output=True,text=True,timeout=5)
            assert p.returncode==3
            print('Actual no-TLS build rejects HTTPS; no plaintext fallback',flush=True)
        for name,source,include,libs in [('policy',CP/'tests/connection_policy_contract.cpp',CP,[]),('identity',R/'tests/tls_identity_test.cpp',R,['-lssl','-lcrypto'])]:
            exe=OUT/(name+'-sanitized')
            run(['c++','-std=c++17','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(include),str(source),*libs,'-o',str(exe)],timeout=120)
            print('ASan/UBSan',run([str(exe)],timeout=15).stdout,flush=True)
        cross=os.environ.get('STUDY_MINGW_CXX')or shutil.which('x86_64-w64-mingw32-g++')
        if cross:
            run([cross,'-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(CP),str(CP/'tests/connection_policy_contract.cpp'),'-static','-o',str(OUT/'connection-policy.exe')],timeout=90)
            print('Windows connection policy cross-linked; HTTPS/native Windows execution not tested',flush=True)
    lesson=R/'docs/learn/lessons/135.json'
    if lesson.exists():
        corpus={lang:[normalized(f.read_text(),lang)for f in CP.rglob('*')if f.is_file()and(f.suffix in ['.cpp','.h']if lang=='cpp'else f.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};count=0
        for section in json.loads(lesson.read_text())['sections']:
            for c in section.get('codes',[]):
                if 'text'in c and c['language']in corpus:
                    assert any(normalized(c['text'],c['language'])in t for t in corpus[c['language']]),c['label'];count+=1
        print('inline snippets',count)
if __name__=='__main__':main()
