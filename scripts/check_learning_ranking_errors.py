"""Production leaderboard: distinguish empty, failed and interrupted partial reads."""
from pathlib import Path
import subprocess,tempfile,json,sys,socket,time,sqlite3
from check_learning_text_layout import run
from check_learning_meta_service import request
R=Path(__file__).resolve().parents[1];OUT=R/'out/learning-checkpoints/123-ranking-check'
def method_probe(before=False):
    source=(R/'meta/database.cpp').read_text()
    a=source.index('std::optional<std::vector<LeaderRow>>\nDatabase::leaderboard');b=source.index('\n\nstd::optional<int> Database::botReward',a)
    method=(R/'out/learning-jobs/123-ranking-before.txt').read_text() if before else source[a:b]
    nullable=source[source.index('std::optional<std::string> read_nullable_text'):source.index('const IconCatalogEntry* find_icon_def')]
    numeric=source[source.index('bool read_nonnegative_int'):source.index('std::optional<Player> read_player')]
    decl='std::vector<LeaderRow>' if before else 'std::optional<std::vector<LeaderRow>>'
    code='''#include "meta/database.h"
#include "sqlite3.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstdio>
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
namespace meta {
struct StmtGuard {sqlite3_stmt* s=nullptr;~StmtGuard(){if(s)sqlite3_finalize(s);}};
'''+nullable+numeric+'''struct Probe {sqlite3* db_;std::mutex mu_;explicit Probe(sqlite3* db):db_(db){};'''+decl+''' leaderboard(int);};
'''+method.replace('Database::leaderboard','Probe::leaderboard')+'''
}
void exec(sqlite3* db,const char* sql){CHECK(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);}
int main(){sqlite3* db=nullptr;CHECK(sqlite3_open(":memory:",&db)==SQLITE_OK);meta::Probe p(db);
exec(db,"CREATE TABLE players(id INTEGER PRIMARY KEY,username TEXT,elo INTEGER,wins INTEGER,losses INTEGER,xp INTEGER);");
auto empty=p.leaderboard(10);
'''+('CHECK(empty.empty());'if before else'CHECK(empty&&empty->empty());')+'''
exec(db,"INSERT INTO players VALUES(1,NULL,100,0,0,0),(2,NULL,0,0,0,0);");
auto full=p.leaderboard(10);
'''+('CHECK(full.size()==2);'if before else'CHECK(full&&full->size()==2);')+'''
exec(db,"ALTER TABLE players RENAME TO base;CREATE INDEX rank_base ON base(elo DESC,id ASC);");
CHECK(sqlite3_create_function_v2(db,"row_xp",1,SQLITE_UTF8,nullptr,[](sqlite3_context* c,int,sqlite3_value** a){if(sqlite3_value_int(a[0])==2)sqlite3_result_error(c,"injected step error",-1);else sqlite3_result_int(c,0);},nullptr,nullptr,nullptr)==SQLITE_OK);
exec(db,"CREATE VIEW players AS SELECT id,username,elo,wins,losses,row_xp(id) AS xp FROM base;");
auto partial=p.leaderboard(10);
'''+('CHECK(partial.size()==1);std::cout<<"Before: step failure exposed one-row partial ranking; empty and failure share a vector\\n";'if before else'CHECK(!partial);std::cout<<"After: step failure never publishes a partial ranking\\n";')+'''
exec(db,"DROP VIEW players;");auto missing=p.leaderboard(10);
'''+('CHECK(missing.empty());'if before else'CHECK(!missing);')+'''
sqlite3_close(db);}
'''
    OUT.mkdir(parents=True,exist_ok=True);p=OUT/('root-ranking-before.cpp'if before else'root-ranking-after.cpp');p.write_text(code);exe=p.with_suffix('')
    run(['c++','-std=c++17','-pthread','-I'+str(R),'-isystem',str(R/'third_party'),str(p),str(R/'out/learning-checkpoints/122-account-screen-check/scripted/libstudy_sqlite.a'),'-ldl','-o',str(exe)],timeout=90)
    print(run([str(exe)],timeout=10).stdout,flush=True)

def server_probe():
    with tempfile.TemporaryDirectory(prefix='root-ranking123-')as tmp:
        db=Path(tmp)/'root.db';log=Path(tmp)/'server.log'
        with socket.socket()as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
        with log.open('w')as output:p=subprocess.Popen([str(R/'build-polish/tetris_meta'),'--db',str(db),'--http',f'127.0.0.1:{port}','--allow-public-matches'],stdout=output,stderr=output,cwd=R)
        try:
            deadline=time.monotonic()+10
            while time.monotonic()<deadline:
                assert p.poll() is None,log.read_text()
                try:
                    if request(port,path='/healthz')[0]==200:break
                except OSError:pass
                time.sleep(.03)
            else:raise AssertionError('server readiness')
            assert request(port,path='/v1/leaderboard')==(200,[])
            for _ in range(2):assert request(port,body={},path='/v1/guest')[0]==200
            status,rows=request(port,path='/v1/leaderboard');assert status==200 and len(rows)==2
            assert [r['rank']for r in rows]==[1,2] and rows[0]['player_id']<rows[1]['player_id']
            with sqlite3.connect(db)as c:c.execute('UPDATE players SET xp=-1 WHERE id=?',(rows[-1]['player_id'],))
            assert request(port,path='/v1/leaderboard')==(503,{'error':'leaderboard_unavailable'})
            print('Production HTTP: empty200, ordered public rows200, invalid later row503 passed',flush=True)
        finally:
            if p.poll() is None:p.terminate()
            try:p.wait(timeout=3)
            except subprocess.TimeoutExpired:p.kill();p.wait(timeout=3)
if __name__=='__main__':
    method_probe('--before' in sys.argv)
    if '--before' not in sys.argv:server_probe()
