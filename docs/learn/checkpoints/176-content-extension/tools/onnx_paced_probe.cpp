#include "bot/onnx_policy.h"
#include "bot/paced_driver.h"
#include <charconv>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
template<class T> T number(const char* raw) {
    const std::string text(raw);T value{};
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())throw std::invalid_argument("numeric argument");
    return value;
}
int main(int argc,char** argv) {
    try {
        if(argc!=7)throw std::invalid_argument("model seed ticks interval think minimum");
        const auto seed=number<std::uint64_t>(argv[2]);const int ticks=number<int>(argv[3]);
        if(ticks<=0)throw std::invalid_argument("positive tick budget required");
        study_paced::Driver driver({number<int>(argv[4]),number<int>(argv[5]),number<int>(argv[6])});
        study_inference::Policy model;std::string error;
        if(!model.load(argv[1],error))throw std::runtime_error(error);
        study_python::Session state(seed);
        for(int tick=0;tick<ticks && !state.finished();++tick) {
            int selected=-1;
            const auto result=driver.tick(state,[&](const auto& observed,int& action) {
                study_inference::Prediction prediction;
                if(!model.infer(observed,prediction,error))return false;
                action=prediction.action;selected=action;return true;
            });
            std::cout<<tick<<' '<<unsigned(result.mask)<<' '<<static_cast<int>(result.event)<<' '
                     <<result.decision_attempted<<' '<<selected<<' '<<state.score()<<' '<<state.finished()<<' ';
            for(auto byte:state.state_bytes())std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
            std::cout<<std::dec<<'\n';
        }
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
