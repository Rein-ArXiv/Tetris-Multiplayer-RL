#include "bot/onnx_policy.h"
#include "bot/policy_match.h"
#include <charconv>
#include <iostream>
#include <iomanip>
#include <string>
template<class T> T number(const char* raw) {
    const std::string text(raw);T value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())throw std::invalid_argument("numeric argument");
    return value;
}
void bytes(const study_python::Session& state) {
    for(auto byte:state.state_bytes())std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
    std::cout<<std::dec;
}
int main(int argc,char** argv) {
    try {
        if(argc!=5)throw std::invalid_argument("model seed ticks practice|strict");
        const auto seed=number<std::uint64_t>(argv[2]);const auto ticks=number<int>(argv[3]);
        const std::string mode=argv[4];if(ticks<=0 || (mode!="practice" && mode!="strict"))throw std::invalid_argument("run conditions");
        study_characters::Character character{"rival",{"Rival","","",""},{argv[1],{3,2,8}}};
        study_inference::Policy model;std::string error;if(!model.load(argv[1],error))throw std::runtime_error(error);
        study_match::Match match(seed,character,bot::Mode::reward);
        std::vector<std::uint8_t> inputs;int selected=-1;
        auto picker=[&](const study_python::Session& observed,int& action) {
            study_inference::Prediction output;if(!model.infer(observed,output,error))return false;
            action=output.action;selected=action;return true;
        };
        for(int tick=0;tick<ticks && !match.finished();++tick) {
            selected=-1;const auto frame=match.tick(0,picker,mode=="practice");inputs.push_back(0);
            std::cout<<tick<<' '<<unsigned(frame.bot.mask)<<' '<<static_cast<int>(frame.source)<<' '
                     <<match.status().reward_eligible()<<' '<<selected<<' '<<match.human().score()<<' '
                     <<match.enemy().score()<<' '<<match.finished()<<' ';
            bytes(match.human());std::cout<<' ';bytes(match.enemy());std::cout<<'\n';
            if(mode=="strict" && !match.status().reward_eligible())break;
        }
        const auto verdict=study_match::verify(seed,character,inputs,static_cast<std::size_t>(ticks),picker);
        std::cout<<"verdict "<<static_cast<int>(verdict)<<'\n';
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
