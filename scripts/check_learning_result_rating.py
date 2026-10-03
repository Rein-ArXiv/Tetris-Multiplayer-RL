"""Exercise the exact production rating-update statements without building the GUI."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/121-save-uncertainty-check'

def check(before=False):
    OUT.mkdir(parents=True,exist_ok=True)
    if before:body=(ROOT/'out/learning-jobs/121-rating-before.txt').read_text()
    else:
        s=(ROOT/'src/main.cpp').read_text();a=s.index('                    lastMatchResult = mr;')+len('                    lastMatchResult = mr;')
        body=s[a:s.index('                    // bp/xp',a)]
    source='''#include "net/match_result.h"
#include <iostream>
struct MatchResult {int elo_after;net::ResultStatus status;};
int apply(int myElo, MatchResult mr) {
'''+body+'''return myElo;
}
int main() {
    for (unsigned offset=0;offset<256;++offset) {
        const unsigned tag=(offset+4)%256; // Reproduce SaveFailed first, then cover all bytes.
        for(int previous:{0,200,900}) for(int reported:{0,150,1000}) {
            // Independent wire-status contract: only Applied(1) and Draw(5) confirm rating.
            const int expected=(tag==1 || tag==5) ? reported : previous;
            if(apply(previous,{reported,static_cast<net::ResultStatus>(tag)})!=expected) {
                std::cout<<"rating changed without confirmed status="<<tag<<"\\n";return 1;
            }
        }
    }
    std::cout<<"Production result projection: 256 statuses, confirmed zero and unchanged unknown/unconfirmed ratings passed\\n";
}
'''
    stem='rating-before' if before else 'rating-after'
    src=OUT/(stem+'.cpp');src.write_text(source);exe=OUT/stem
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-I'+str(ROOT),str(src),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (ROOT/('out/learning-jobs/121-'+stem+'.log')).write_text(result.stdout+result.stderr)
    assert result.returncode==(1 if before else 0),result.stdout+result.stderr
    print(result.stdout,flush=True)
if __name__=='__main__':
    import sys
    check('--before' in sys.argv)
