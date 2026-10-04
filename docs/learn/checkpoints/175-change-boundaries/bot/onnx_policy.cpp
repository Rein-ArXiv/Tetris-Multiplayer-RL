#include "bot/onnx_policy.h"
#include "bot/policy_choice.h"
#include "bindings/session.h"
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace study_inference {
namespace {
constexpr auto rows = study_grid::Grid::kRows;
constexpr auto cols = study_grid::Grid::kColumns;
constexpr auto pieces = study_catalog::definitions.size();
constexpr auto actions = study_actions::kCount;
const std::array<const char*,3> input_names = {"board","current","next"};
const std::array<const char*,2> output_names = {"policy_logits","value"};
const std::array<std::vector<int64_t>,3> input_shapes = {{{1,1,rows,cols},{1,pieces},{1,pieces}}};
const std::array<std::vector<int64_t>,2> output_shapes = {{{1,actions},{1}}};

struct Inputs {
    std::array<float,rows*cols> board{};
    std::array<float,pieces> current{}, next{};
    std::array<bool,actions> legal{};
    explicit Inputs(const study_python::Session& state) {
        if(state.finished())throw std::runtime_error("round finished");
        const auto snapshot=state.snapshot();
        for(int row=0;row<rows;++row)
            for(int col=0;col<cols;++col)
                board[row*cols+col]=snapshot.board.at(row).at(col) ? 1.f : 0.f;
        const auto ids=study_python::Session::piece_ids();
        auto order=[&](int id) {
            const auto it=std::find(ids.begin(),ids.end(),id);
            if(it==ids.end())throw std::runtime_error("unknown piece id");
            return static_cast<std::size_t>(it-ids.begin());
        };
        current.at(order(snapshot.current_id))=1.f;
        next.at(order(snapshot.next_id))=1.f;
        for(int action:state.legal_actions())legal.at(action)=true;
    }
};
}
struct Policy::Impl {
    // Members die in reverse declaration order: session before env.
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING,"study_inference"};
    Ort::SessionOptions options;
    Ort::MemoryInfo memory=Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,OrtMemTypeDefault);
    std::unique_ptr<Ort::Session> session;

    explicit Impl(const std::string& utf8_path) {
        options.SetIntraOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#if defined(_WIN32)
        const auto path=std::filesystem::u8path(utf8_path).wstring();
        session=std::make_unique<Ort::Session>(env,path.c_str(),options);
#else
        session=std::make_unique<Ort::Session>(env,utf8_path.c_str(),options);
#endif
        if(session->GetInputCount()!=input_names.size() || session->GetOutputCount()!=output_names.size())
            throw std::runtime_error("I/O count mismatch");
        Ort::AllocatorWithDefaultOptions allocator;
        auto validate=[&](bool input,std::size_t i,const char* name,const std::vector<int64_t>& shape) {
            auto actual=input ? session->GetInputNameAllocated(i,allocator) : session->GetOutputNameAllocated(i,allocator);
            auto type=input ? session->GetInputTypeInfo(i) : session->GetOutputTypeInfo(i);
            if(std::strcmp(actual.get(),name)!=0 || type.GetONNXType()!=ONNX_TYPE_TENSOR)
                throw std::runtime_error("I/O name or kind mismatch");
            const auto info=type.GetTensorTypeAndShapeInfo();
            if(info.GetElementType()!=ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || info.GetShape()!=shape)
                throw std::runtime_error("I/O dtype or shape mismatch");
        };
        for(std::size_t i=0;i<input_names.size();++i)validate(true,i,input_names[i],input_shapes[i]);
        for(std::size_t i=0;i<output_names.size();++i)validate(false,i,output_names[i],output_shapes[i]);
    }

    Prediction run(const study_python::Session& state) {
        Inputs owned(state);
        auto wrap=[&](auto& data,std::size_t index) {
            const auto& shape=input_shapes[index];
            return Ort::Value::CreateTensor<float>(memory,data.data(),data.size(),shape.data(),shape.size());
        };
        std::array<Ort::Value,3> inputs={wrap(owned.board,0),wrap(owned.current,1),wrap(owned.next,2)};
        auto outputs=session->Run(Ort::RunOptions{nullptr},input_names.data(),inputs.data(),inputs.size(),
                                  output_names.data(),output_names.size());
        if(outputs.size()!=output_names.size())throw std::runtime_error("output count mismatch");
        for(std::size_t i=0;i<outputs.size();++i) {
            if(!outputs[i].IsTensor())throw std::runtime_error("output not tensor");
            const auto info=outputs[i].GetTensorTypeAndShapeInfo();
            if(info.GetElementType()!=ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || info.GetShape()!=output_shapes[i])
                throw std::runtime_error("output dtype or shape mismatch");
        }
        Prediction candidate;
        const auto* scores=outputs[0].GetTensorData<float>();
        candidate.value=outputs[1].GetTensorData<float>()[0];
        if(!std::isfinite(candidate.value) ||
           !bot::choose_finite_legal(scores,owned.legal.data(),owned.legal.size(),candidate.action))
            throw std::runtime_error("nonfinite output or no legal action");
        std::copy_n(scores,candidate.logits.size(),candidate.logits.begin());
        return candidate; // owned values survive output Ort::Value destruction
    }
};
Policy::Policy()=default;
Policy::~Policy()=default;
bool Policy::loaded() const noexcept {return impl_!=nullptr;}
bool Policy::load(const std::string& path,std::string& error) {
    try {
        auto candidate=std::make_unique<Impl>(path);
        impl_=std::move(candidate);
        error.clear();
        return true;
    } catch(const std::exception& e) {
        impl_.reset();
        error=e.what();
        return false;
    }
}
bool Policy::infer(const study_python::Session& state,Prediction& result,std::string& error) {
    try {
        if(!impl_)throw std::runtime_error("model not loaded");
        auto candidate=impl_->run(state);
        result=candidate;
        error.clear();
        return true;
    } catch(const std::exception& e) {
        error=e.what();
        return false;
    }
}
}
