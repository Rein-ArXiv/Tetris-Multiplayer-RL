"""Lesson138: asynchronous cancellation, buffer ownership and event-loop scope."""
from pathlib import Path
import argparse,json,os,tempfile,subprocess
from check_learning_text_layout import run
from check_part_docs import normalized
from learning_tls_fixture import certificates
from learning_wss_fixture import echo_backend,gateway
from learning_async_tls_fixture import matrix
R=Path(__file__).resolve().parents[1];CP=R/'docs/learn/checkpoints/138-async-tls';OUT=R/'out/learning-checkpoints/138-async-tls-check'
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--boost',default=os.environ.get('STUDY_BOOST_INCLUDE','/usr/include'));ap.add_argument('--snippets-only',action='store_true');ap.add_argument('--sanitize',action='store_true');args=ap.parse_args()
    prev=CP.parent/'137-admission-tickets'
    for p in prev.rglob('*'):
        if p.is_file()and p.relative_to(prev).as_posix()not in {'README.md','CMakeLists.txt'}:
            assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.snippets_only:
        boost=Path(args.boost).resolve();assert (boost/'boost/beast.hpp').exists(),'Set --boost or STUDY_BOOST_INCLUDE'
        b=OUT/'scripted'
        print(run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM=SCRIPTED','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release',f'-DSTUDY_BOOST_INCLUDE={boost}']).stdout,flush=True)
        print(run(['cmake','--build',str(b),'--target','async_wss_probe','async_context_contract','-j2'],timeout=300).stdout,flush=True)
        print(run(['ctest','--test-dir',str(b),'-R','^async_context_contract$','--output-on-failure']).stdout,flush=True)
        flags=['c++','-std=c++17','-O0','-Wall','-Wextra','-Wpedantic','-pthread','-DBOOST_ERROR_CODE_HEADER_ONLY','-isystem',str(boost)]
        gw=OUT/'current_gateway';native=OUT/'current_wss_lifetime'
        for target,source,extra in [(gw,['server/wss_gateway.cpp'],[]),(native,['tests/learning/current_wss_lifetime.cpp','net/wss_client.cpp','net/socket.cpp','net/system_trust.cpp'],['-DTETRIS_HAS_WSS'])]:
            result=run([*flags,'-I'+str(R),*extra,*[str(R/p)for p in source],'-lssl','-lcrypto','-o',str(target)],timeout=240)
            (OUT/(target.name+'-build.log')).write_text(result.stdout+result.stderr)
        with tempfile.TemporaryDirectory(prefix='study138-tls-')as tmp:
            certs=certificates(Path(tmp));matrix(b/'async_wss_probe',gw,certs)
            if args.sanitize:
                probe=OUT/'async_wss_sanitized'
                run([*flags,'-g0','-I'+str(CP),'-fsanitize=address,undefined','-fno-sanitize-recover=all',str(CP/'tools/async_wss_probe.cpp'),'-lssl','-lcrypto','-o',str(probe)],timeout=300)
                matrix(probe,gw,certs)
            with echo_backend()as backend,gateway(gw,certs,backend.server_address[1])as port:
                env={**os.environ,'TETRIS_CA_FILE':str(certs/'ca.pem')}
                print(run([str(native),f'wss://localhost:{port}/play'],env=env,timeout=12).stdout,flush=True)
        exe=OUT/'context_sanitized'
        run([*flags,'-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(CP/'tests/async_context_contract.cpp'),'-o',str(exe)],timeout=90)
        print('ASan/UBSan',run([str(exe)]).stdout,flush=True)
    f=R/'docs/learn/lessons/138.json'
    if f.exists():
        corpus={lang:[normalized(p.read_text(),lang)for p in CP.rglob('*')if p.is_file()and(p.suffix in ['.cpp','.h']if lang=='cpp'else p.name=='CMakeLists.txt')]for lang in ['cpp','cmake']};count=0
        for s in json.loads(f.read_text())['sections']:
            ref=s.get('reference')
            if ref:assert ref['symbol']in(R/ref['path']).read_text(),ref
            for c in s.get('codes',[]):
                if 'text'in c and c['language']in corpus:
                    assert any(normalized(c['text'],c['language'])in text for text in corpus[c['language']]),c['label'];count+=1
        print('Inline snippets',count,'current symbols and cumulative files verified',flush=True)
if __name__=='__main__':main()
