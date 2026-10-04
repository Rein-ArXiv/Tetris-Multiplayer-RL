// ONNX Runtime wrapper의 구현. 입출력 계약은 bot_onnx.h에 있다.
//
// 이 파일은 ONNX Runtime이 없어도 항상 컴파일된다.
// TETRIS_BUILD_BOT=ON일 때만 CMake가 TETRIS_HAS_ONNXRUNTIME을 정의하고
// 실제 구현이 빌드된다. 그렇지 않으면 파일 끝의 stub이 대신 들어가
// Load()가 언제나 실패한다. 모델 선택 실패의 처리는 호출자 정책이다.
// 덕분에 ONNX Runtime을 받지 않은 사람도 저장소를 그대로 빌드할 수 있다.

#include "bot_onnx.h"

#include "placement.h"
#include "policy_choice.h"
#include "../src/sim_game.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>

#if defined(TETRIS_HAS_ONNXRUNTIME)
    #include <onnxruntime_cxx_api.h>
#endif

namespace bot {

#if defined(TETRIS_HAS_ONNXRUNTIME)

struct BotOnnx::Impl {
    Ort::Env     env{ORT_LOGGING_LEVEL_WARNING, "tetris_bot"};
    Ort::SessionOptions sessOpts{};
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // 이 이름들은 export_onnx.py가 박아 넣은 것과 한 글자도 달라선 안 된다.
    std::array<const char*, 3> inputNames  = {"board", "current", "next"};
    std::array<const char*, 2> outputNames = {"policy_logits", "value"};

    bool LoadModel(const std::string& path, std::string* err_out)
    {
        try {
            sessOpts.SetIntraOpNumThreads(1);
            sessOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        #if defined(_WIN32)
            // 경로는 UTF-8로 들어온다. u8path를 거치지 않으면 Windows에서
            // 한글 사용자 폴더 같은 경로가 현재 C 로캘 기준으로 잘못 해석돼
            // "파일 없음"이 된다.
            const std::wstring wpath = std::filesystem::u8path(path).wstring();
            session = std::make_unique<Ort::Session>(env, wpath.c_str(), sessOpts);
        #else
            session = std::make_unique<Ort::Session>(env, path.c_str(), sessOpts);
        #endif
            if (session->GetInputCount()!=3 || session->GetOutputCount()!=2)
                throw std::runtime_error("expected 3 inputs and 2 outputs");
            Ort::AllocatorWithDefaultOptions allocator;
            auto validate=[&](bool input,size_t index,const char* name,const std::vector<int64_t>& shape) {
                auto actualName=input ? session->GetInputNameAllocated(index,allocator)
                                      : session->GetOutputNameAllocated(index,allocator);
                auto type=input ? session->GetInputTypeInfo(index) : session->GetOutputTypeInfo(index);
                if(std::strcmp(actualName.get(),name)!=0 || type.GetONNXType()!=ONNX_TYPE_TENSOR)
                    throw std::runtime_error(std::string("incompatible tensor: ")+name);
                auto info=type.GetTensorTypeAndShapeInfo();
                if(info.GetElementType()!=ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || info.GetShape()!=shape)
                    throw std::runtime_error(std::string("incompatible float32 shape: ")+name);
            };
            validate(true,0,"board",{1,1,kBoardRows,kBoardCols});
            validate(true,1,"current",{1,kNumPieceTypes});
            validate(true,2,"next",{1,kNumPieceTypes});
            validate(false,0,"policy_logits",{1,kNumPlacements});
            validate(false,1,"value",{1});
        } catch (const Ort::Exception& e) {
            if (err_out) *err_out = std::string("Ort::Exception: ") + e.what();
            session.reset();
            return false;
        } catch (const std::exception& e) {
            if (err_out) *err_out = std::string("std::exception: ") + e.what();
            session.reset();
            return false;
        }
        if (err_out) err_out->clear();
        return true;
    }

    bool InferOnce(const SimGame& sim, int& col_out, int& rot_out)
    {
        if (!session) return false;

        float board[kBoardRows * kBoardCols];   // row-major occupancy
        float current[kNumPieceTypes];          // catalog-order one-hot
        float nxt[kNumPieceTypes];
        observe(sim, board, current, nxt);

        std::array<int64_t, 4> boardShape = {1, 1, kBoardRows, kBoardCols};
        std::array<int64_t, 2> pieceShape = {1, kNumPieceTypes};

        Ort::Value boardT = Ort::Value::CreateTensor<float>(
            memInfo, board, sizeof(board) / sizeof(float),
            boardShape.data(), boardShape.size());
        Ort::Value curT = Ort::Value::CreateTensor<float>(
            memInfo, current, kNumPieceTypes,
            pieceShape.data(), pieceShape.size());
        Ort::Value nxtT = Ort::Value::CreateTensor<float>(
            memInfo, nxt, kNumPieceTypes,
            pieceShape.data(), pieceShape.size());

        Ort::Value inputs[3] = {std::move(boardT), std::move(curT), std::move(nxtT)};

        // Run is synchronous; input arrays and wrappers remain alive until return.
        auto outs = session->Run(Ort::RunOptions{nullptr}, inputNames.data(),
                                 inputs, inputNames.size(), outputNames.data(), outputNames.size());
        if (outs.size() != outputNames.size()) return false;
        const std::array<std::vector<int64_t>, 2> shapes = {{{1, kNumPlacements}, {1}}};
        for (size_t i = 0; i < outs.size(); ++i) {
            if (!outs[i].IsTensor()) return false;
            const auto info = outs[i].GetTensorTypeAndShapeInfo();
            if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                info.GetShape() != shapes[i]) return false;
        }
        // These pointers borrow output storage. Consume them before outs is destroyed.
        const float* logits = outs[0].GetTensorData<float>();
        if (!std::isfinite(outs[1].GetTensorData<float>()[0])) return false;

        // 규칙상 둘 수 있는 자리만 남긴다. 모델이 뭘 내놓든 불법 수는 못 고른다.
        auto placements = sim.LegalPlacements();
        if (placements.empty()) return false;

        bool legal[kNumPlacements] = {false};
        for (const auto& p : placements) {
            int a = encode_action(p.col, p.rot);
            if (a >= 0 && a < kNumPlacements) legal[a] = true;
        }

        int bestIdx = -1;
        if (!choose_finite_legal(logits, legal, kNumPlacements, bestIdx)) return false;
        decode_action(bestIdx, col_out, rot_out);
        return true;
    }
};

BotOnnx::BotOnnx() = default;
BotOnnx::~BotOnnx() = default;

bool BotOnnx::Load(const std::string& onnx_path, std::string* err_out)
{
    try {
        if (!impl_) impl_ = std::make_unique<Impl>();
        return impl_->LoadModel(onnx_path, err_out);
    } catch (const std::exception& error) {
        impl_.reset();
        if (err_out) *err_out = error.what();
        return false;
    }
}

bool BotOnnx::Infer(const SimGame& sim, int& col_out, int& rot_out)
{
    if (!impl_ || !impl_->session) return false;
    try {
        int col = 0, rot = 0;
        if (!impl_->InferOnce(sim, col, rot)) return false;
        col_out = col;
        rot_out = rot;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool BotOnnx::IsLoaded() const
{
    return impl_ && impl_->session != nullptr;
}

#else  // !TETRIS_HAS_ONNXRUNTIME

// ONNX Runtime이 없을 때 쓰는 stub. Load는 언제나 실패하고,
// 호출자가 IsLoaded()로 걸러 주므로 Infer까지 오지 않는다.
struct BotOnnx::Impl { bool loaded = false; };

BotOnnx::BotOnnx() = default;
BotOnnx::~BotOnnx() = default;

bool BotOnnx::Load(const std::string& onnx_path, std::string* err_out)
{
    (void)onnx_path;
    if (err_out) *err_out = "ONNX Runtime unavailable — fetch the CPU runtime and rebuild with TETRIS_BUILD_BOT=ON";
    return false;
}

bool BotOnnx::Infer(const SimGame&, int&, int&) { return false; }
bool BotOnnx::IsLoaded() const { return false; }

#endif  // TETRIS_HAS_ONNXRUNTIME

}  // namespace bot
