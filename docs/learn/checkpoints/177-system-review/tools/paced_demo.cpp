#include "bot/paced_driver.h"
#include <iostream>
int main() {
    study_python::Session state(91);
    study_paced::Driver driver({3,2,8});
    for(int tick=0;tick<120 && !state.finished();++tick) {
        const auto result=driver.tick(state,[](const auto& observed,int& action) {
            const auto legal=observed.legal_actions();if(legal.empty())return false;
            action=legal.front();return true;
        });
        std::cout<<tick<<' '<<unsigned(result.mask)<<' '<<static_cast<int>(result.event)
                 <<' '<<result.decision_attempted<<' '<<driver.age_for_gates()<<'\n';
    }
}
