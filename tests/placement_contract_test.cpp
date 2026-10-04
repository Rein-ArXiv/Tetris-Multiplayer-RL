#include "src/sim_game.h"
#include "bot/placement.h"
#include <limits>
#include <stdexcept>

void require(bool value) {if(!value)throw std::runtime_error("placement contract");}
int main() {
    for(unsigned seed=1;seed<=32;++seed) {
        SimGame game(seed);
        for(int turn=0;turn<25 && !game.IsGameOver();++turn) {
            const auto before=game.DiagnosticStateHashV2();
            const auto legal=game.LegalPlacements();
            require(game.DiagnosticStateHashV2()==before);
            for(const auto p:legal) {
                auto copy=game;
                require(copy.ApplyPlacement(p.col,p.rot)>=0);
                require(game.DiagnosticStateHashV2()==before);
            }
            for(const auto col:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
                require(game.ApplyPlacement(col,0)==-1);
                require(game.DiagnosticStateHashV2()==before);
            }
            require(game.ApplyPlacement(0,-1)==-1 && game.DiagnosticStateHashV2()==before);
            if(legal.empty())break;
            const auto p=legal[(seed+turn)%legal.size()];
            require(game.ApplyPlacement(p.col,p.rot)>=0);
        }
    }
    // Deliberate fixture: an endpoint is clear, but a wall blocks the key route.
    unsigned seed=1;
    while(SimGame(seed).CurrentBlockId()!=4) {++seed;require(seed<1000);}
    SimGame endpoint(seed);
    auto& board=const_cast<int(&)[SimGrid::kRows][SimGrid::kCols]>(endpoint.Grid());
    for(int row=0;row<SimGrid::kRows;++row)board[row][2]=1;
    auto route=endpoint;
    bool listed=false;
    for(const auto p:endpoint.LegalPlacements())listed|=p.col==0&&p.rot==0;
    require(listed && endpoint.ApplyPlacement(0,0)>=0);
    for(const auto mask:bot::expand_placement(route.CurrentCol(),route.CurrentRotation(),0,0)) {
        route.SubmitInput(mask);route.Tick();
    }
    require(endpoint.DiagnosticStateHashV2()!=route.DiagnosticStateHashV2());
    require(endpoint.Grid()[SimGrid::kRows-1][0]>0 && route.Grid()[SimGrid::kRows-1][0]==0);
}
