"""Lesson136: real WSS tunnel boundaries, stable message adapter, native SAN policy."""
from pathlib import Path
import argparse,json,os,subprocess,tempfile
from check_learning_text_layout import run
from learning_tls_fixture import certificates
from learning_wss_fixture import tunnel_matrix,native_identity_matrix
from check_part_docs import normalized
R=Path(__file__).resolve().parents[1]
CP=R/'docs/learn/checkpoints/136-wss-tunnel'
OUT=R/'out/learning-checkpoints/136-wss-tunnel-check'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boost',default=os.environ.get('STUDY_BOOST_INCLUDE','/usr/include'))
    args=parser.parse_args();boost=Path(args.boost).resolve()
    assert (boost/'boost/beast.hpp').exists(),'Provide Boost headers via --boost or STUDY_BOOST_INCLUDE'
    OUT.mkdir(parents=True,exist_ok=True)
    prev=CP.parent/'135-secure-connections'
    for p in prev.rglob('*'):
        if p.is_file()and p.relative_to(prev).as_posix()not in {'README.md','CMakeLists.txt'}:
            assert (CP/p.relative_to(prev)).read_bytes()==p.read_bytes(),p
    b=OUT/'scripted'
    print(run(['cmake','-S',str(CP),'-B',str(b),'-DSTUDY_PLATFORM=SCRIPTED','-DSTUDY_AUDIO=NONE','-DCMAKE_BUILD_TYPE=Release',f'-DSTUDY_BOOST_INCLUDE={boost}']).stdout,flush=True)
    print(run(['cmake','--build',str(b),'--target','message_stream_contract','wss_roundtrip','connection_policy_contract','-j2'],timeout=300).stdout,flush=True)
    print(run(['ctest','--test-dir',str(b),'-R','^(message_stream_contract|connection_policy_contract)$','--output-on-failure']).stdout,flush=True)
    flags=['c++','-std=c++17','-O0','-Wall','-Wextra','-Wpedantic','-pthread','-DBOOST_ERROR_CODE_HEADER_ONLY','-I'+str(R),'-isystem',str(boost)]
    gateway=OUT/'current_gateway';native=OUT/'current_wss_after'
    result=run([*flags,str(R/'server/wss_gateway.cpp'),'-lssl','-lcrypto','-o',str(gateway)],timeout=180)
    (OUT/'gateway-build.log').write_text(result.stdout+result.stderr)
    result=run([*flags,'-DTETRIS_HAS_WSS',*[str(R/f)for f in ['tests/learning/current_wss.cpp','net/wss_client.cpp','net/socket.cpp','net/system_trust.cpp']],'-lssl','-lcrypto','-o',str(native)],timeout=180)
    (OUT/'native-build.log').write_text(result.stdout+result.stderr)
    run([str(native),'--url-contract']);print('Native URL NUL rejection preserves output',flush=True)
    with tempfile.TemporaryDirectory(prefix='study136-tls-')as tmp:
        certs=certificates(Path(tmp))
        tunnel_matrix(gateway,b/'wss_roundtrip',certs)
        native_identity_matrix(gateway,native,certs)
    sanitized=OUT/'message_stream_sanitized'
    run(['c++','-std=c++17','-g','-Wall','-Wextra','-Wpedantic','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(CP),str(CP/'tests/message_stream_contract.cpp'),'-o',str(sanitized)],timeout=90)
    print('ASan/UBSan',run([str(sanitized)]).stdout,flush=True)
    lesson=R/'docs/learn/lessons/136.json'
    if lesson.exists():
        corpus={lang:[normalized(p.read_text(),lang)for p in CP.rglob('*')if p.is_file()and(p.suffix in ['.h','.cpp']if lang=='cpp'else p.name=='CMakeLists.txt')]for lang in ['cpp','cmake']}
        for s in json.loads(lesson.read_text())['sections']:
            for code in s.get('codes',[]):
                if 'text'in code and code['language']in corpus:
                    assert any(normalized(code['text'],code['language'])in content for content in corpus[code['language']]),code['label']
        print('Inline snippets match stable checkpoint',flush=True)
if __name__=='__main__':main()
