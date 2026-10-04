#include "bot/onnx_policy.h"
#include "bindings/session.h"
#include <onnxruntime_cxx_api.h>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <limits>
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
        if(argc!=4)throw std::invalid_argument("model.onnx seed decisions");
        const auto seed=number<std::uint64_t>(argv[2]);
        const auto limit=number<int>(argv[3]);
        if(limit<=0)throw std::invalid_argument("positive decisions required");
        std::cerr<<"ONNX Runtime "<<OrtGetApiBase()->GetVersionString()<<'\n';
        study_inference::Policy policy;std::string error;
        if(!policy.load(argv[1],error))throw std::runtime_error(error);
        study_python::Session state(seed);
        std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10);
        for(int i=0;i<limit && !state.finished();++i) {
            study_inference::Prediction p;
            if(!policy.infer(state,p,error))throw std::runtime_error(error);
            std::cout<<p.action<<' '<<p.value;
            for(float score:p.logits)std::cout<<' '<<score;
            std::cout<<'\n';
            state.apply_action(p.action);
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
