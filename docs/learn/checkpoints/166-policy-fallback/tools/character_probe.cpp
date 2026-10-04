#include "bot/onnx_policy.h"
#include "bot/paced_driver.h"
#include "bot/characters.h"
#include <fstream>
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
        if(argc!=5)throw std::invalid_argument("catalog character-id seed ticks");
        std::ifstream input(argv[1]);
        if(!input)throw std::runtime_error("cannot open character catalog");
        const auto catalog=study_characters::Catalog::read(input);
        const auto selected=catalog.select(argv[2]);
        const auto seed=number<std::uint64_t>(argv[3]);const int ticks=number<int>(argv[4]);
        if(ticks<=0)throw std::invalid_argument("positive tick budget required");
        study_paced::Driver driver(selected.behavior.pacing);
        study_inference::Policy model;std::string error;
        if(selected.behavior.model=="@heuristic")throw std::invalid_argument("this probe requires an ONNX profile");
        if(!model.load(selected.behavior.model,error))throw std::runtime_error(error);
        std::cout<<"character "<<std::quoted(selected.id)<<' '<<std::quoted(selected.appearance.name)<<' '
                 <<std::quoted(selected.appearance.icon)<<' '<<std::quoted(selected.appearance.portrait)<<' '
                 <<std::quoted(selected.appearance.difficulty)<<'\n';
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
