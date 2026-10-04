#include "../bot/bot_onnx.h"
#include "../bot/placement.h"
#include "../src/sim_game.h"
#include <algorithm>
#include <iostream>
int main(int argc,char** argv) {
    if(argc<3)return 2;
    bot::BotOnnx model;std::string why="stale error";
    SimGame sim(42);int col=-37,rot=-39;
    if(model.IsLoaded() || model.Infer(sim,col,rot) || col!=-37 || rot!=-39)return 3;
    if(!model.Load(argv[1],&why) || !why.empty()){std::cerr<<why;return 4;}
    // Constant fixture has increasing logits: expect the highest legal label.
    int expected=-1;
    for(const auto& p:sim.LegalPlacements())expected=std::max(expected,bot::encode_action(p.col,p.rot));
    for(int i=0;i<20;++i) {
        if(!model.Infer(sim,col,rot) || bot::encode_action(col,rot)!=expected)return 5;
    }
    if(model.Load(argv[2],&why) || model.IsLoaded() || why.empty())return 6;
    col=-37;rot=-39;
    if(model.Infer(sim,col,rot) || col!=-37 || rot!=-39)return 7;
    if(!model.Load(argv[1],&why) || !why.empty())return 8;
    for(int i=3;i<argc;++i) {
        if(!model.Load(argv[i],&why)){std::cerr<<why;return 9;}
        col=-37;rot=-39;
        if(model.Infer(sim,col,rot) || col!=-37 || rot!=-39)return 10;
    }
    if(model.Load("missing-model-162.onnx",&why) || model.IsLoaded() || why.empty())return 11;
    return 0;
}
