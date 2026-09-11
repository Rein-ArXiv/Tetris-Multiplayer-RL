#include "../bot/bot_onnx.h"
#include "../src/sim_game.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    bot::BotOnnx model;std::string why;
    if(!model.Load(argv[1],&why)){std::cerr<<why;return 3;}
    SimGame sim(42);int col=0,rot=0;
    if(!model.Infer(sim,col,rot))return 4;
    bool legal=false;
    for(const auto& p:sim.LegalPlacements())if(p.col==col && p.rot==rot)legal=true;
    if(!legal)return 5;
    if(model.Load(argv[2],&why) || model.IsLoaded() || why.empty())return 6;
    return 0;
}
