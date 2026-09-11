#include "../bot/controller.h"
#include "../bot/opponents.h"
#include <cstdio>
#include <filesystem>
#include <fstream>

int main() {
    int failed=0;
    auto check=[&](bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);++failed;}};
    SimGame sim(42);
    bot::Controller driver;
    driver.reset(6,18,60);
    auto still=[](const SimGame& s,int& col,int& rot){col=s.CurrentCol();rot=s.CurrentRotation();return true;};
    for(int tick=0;tick<60;++tick) {
        auto mask=driver.next(sim,still);
        check(tick==59 ? mask==INPUT_DROP : mask==INPUT_NONE,"first placement takes a full second");
        sim.SubmitInput(mask);sim.Tick();
    }
    for(int tick=0;tick<18;++tick) {
        check(driver.next(sim,still)==INPUT_NONE,"new piece receives think delay");sim.Tick();
    }
    // Let gravity spawn a piece while an old multi-input plan is still pending.
    driver.reset(30,0,60);
    auto move=[](const SimGame&,int& col,int& rot){col=0;rot=3;return true;};
    driver.next(sim,move);
    const auto before=sim.RngState();
    for(int i=0;i<25 && sim.RngState()==before;++i)sim.MoveBlockDown();
    check(sim.RngState()!=before,"gravity spawned a new piece");
    int calls=0;
    driver.next(sim,[&](const SimGame& s,int& c,int& r){++calls;return still(s,c,r);});
    check(calls==1,"stale plan discarded after gravity lock");
    // Strict numeric parsing and multiple identities sharing one policy.
    const auto path=std::filesystem::temp_directory_path()/"tetris-opponents-test.cfg";
    {std::ofstream f(path);f<<"a|A|@heuristic|||Easy|8|24|90\nb|B|@heuristic|||Hard|3|9|30\nbad|Bad|@heuristic|||Bad|6x|0|30\n";}
    auto roster=bot::discover_opponents(path.string().c_str(),"/missing-legacy");
    check(roster.size()>=2 && roster[0].id=="a" && roster[1].id=="b","same model can have distinct rivals");
    for(const auto& entry:roster)check(entry.id!="bad","malformed pacing rejected");
    std::filesystem::remove(path);
    return failed?1:0;
}
