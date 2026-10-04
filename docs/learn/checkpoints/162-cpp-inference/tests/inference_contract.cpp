#include "bot/onnx_policy.h"
#include "bindings/session.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc<4)return 2; // model, invalid-shape, bad-numeric models...
    study_inference::Policy policy;
    study_python::Session state(42);
    study_inference::Prediction value;value.action=-19;value.value=-23;value.logits.fill(-29);
    std::string error;
    if(policy.loaded() || policy.infer(state,value,error) || value.action!=-19)return 3;
    error="stale";
    if(!policy.load(argv[1],error) || !error.empty())return 4;
    const auto before=state.state_bytes();
    if(!policy.infer(state,value,error) || !error.empty() || before!=state.state_bytes())return 5;
    const auto copy=value;
    for(int i=0;i<20;++i) {
        if(!policy.infer(state,value,error) || copy.logits!=value.logits || copy.action!=value.action)return 6;
    }
    if(policy.load(argv[2],error) || policy.loaded() || error.empty())return 7;
    if(policy.infer(state,value,error) || copy.logits!=value.logits || copy.action!=value.action)return 8;
    if(!policy.load(argv[1],error) || !error.empty())return 9;
    for(int i=3;i<argc;++i) {
        if(!policy.load(argv[i],error)){std::cerr<<error;return 10;}
        if(policy.infer(state,value,error) || error.empty() || copy.logits!=value.logits ||
           copy.action!=value.action || copy.value!=value.value)return 11;
    }
    if(policy.load("missing-model-162.onnx",error) || policy.loaded() || error.empty())return 12;
    return 0;
}
