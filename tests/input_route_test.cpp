#include "bot/controller.h"
#include <limits>
#include <stdexcept>
void require(bool ok){if(!ok)throw std::runtime_error("input route contract");}
int main() {
    for(int value:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max(),-1}) {
        bool rejected=false;
        try{bot::expand_placement(3,0,0,value);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected);
    }
    for(int value:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
        bool rejected=false;
        try{bot::expand_placement(value,0,0,0);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected);
    }
    unsigned seed=1;while(SimGame(seed).CurrentBlockId()!=4)++seed;
    SimGame live(seed);
    auto& grid=const_cast<int(&)[SimGrid::kRows][SimGrid::kCols]>(live.Grid());
    for(int row=0;row<SimGrid::kRows;++row)grid[row][2]=1;
    bot::Controller driver;driver.reset(1,0,1);bool blocked=false;
    for(int i=0;i<10;++i) {
        const auto before=live.DiagnosticStateHashV2();
        const auto mask=driver.next(live,[](const SimGame&,int& c,int& r){c=0;r=0;return true;});
        require(live.DiagnosticStateHashV2()==before && mask!=INPUT_DROP);
        blocked|=driver.status()==bot::Controller::Status::blocked;
        live.SubmitInput(mask);live.Tick();
    }
    require(blocked);
    driver.reset(1,0,1);
    require(driver.next(live,[](const SimGame&,int& c,int& r){c=std::numeric_limits<int>::max();r=0;return true;})==INPUT_NONE);
    require(driver.status()==bot::Controller::Status::invalid_target);
    driver.reset(1,0,1);require(driver.next(live,[](const SimGame&,int&,int&){return false;})==INPUT_NONE);
    require(driver.status()==bot::Controller::Status::no_decision);
    // External shift after planning: prevent the queued drop at a different origin.
    SimGame changed(seed);driver.reset(1,0,3);
    auto stay=[](const SimGame& s,int& c,int& r){c=s.CurrentCol();r=s.CurrentRotation();return true;};
    require(driver.next(changed,stay)==INPUT_NONE);changed.SubmitInput(INPUT_RIGHT);changed.Tick();
    require(driver.next(changed,stay)==INPUT_NONE);changed.SubmitInput(INPUT_NONE);changed.Tick();
    require(driver.next(changed,stay)==INPUT_NONE && driver.status()==bot::Controller::Status::target_lost);
}
