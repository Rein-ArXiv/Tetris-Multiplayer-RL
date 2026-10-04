#include "bindings/session.h"
#include <stdexcept>

void require(bool value) {
    if (!value) throw std::runtime_error("session contract");
}
int main() {
    using study_python::Session;
    Session game(77);
    const auto original = game.state_bytes();
    auto copy = game.clone();
    copy.step(study_input::drop);
    require(copy.state_bytes()!=original && game.state_bytes()==original);
    for (const auto mask : {256u,~0u}) {
        bool caught=false;
        try {game.step(mask);} catch (const std::invalid_argument&) {caught=true;}
        require(caught && game.state_bytes()==original);
    }
    bool caught=false;
    try {game.reset(1,0);} catch(const std::invalid_argument&) {caught=true;}
    require(caught && game.state_bytes()==original);
    auto cells=game.grid();cells[0][0]=99;
    require(game.grid()[0][0]!=99);
    for (int i=0;i<1000 && !game.finished();++i)game.step(study_input::drop);
    require(game.finished());
    auto terminal=game.state_bytes();
    require(game.step(0)==study_round::Step::stopped && game.state_bytes()==terminal);
    game.reset(77,30);
    require(game.state_bytes()==original);
}
