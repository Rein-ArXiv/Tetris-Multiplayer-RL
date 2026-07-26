// ONNX Runtime wrapper의 구현. 입출력 계약은 bot_onnx.h에 있다.
//
// 이 파일은 ONNX Runtime이 없어도 항상 컴파일된다.
// TETRIS_BUILD_BOT=ON일 때만 CMake가 TETRIS_HAS_ONNXRUNTIME을 정의하고
// 실제 구현이 빌드된다. 그렇지 않으면 파일 끝의 stub이 대신 들어가
// Load()가 언제나 실패하고 게임은 heuristic bot으로 넘어간다.
// 덕분에 ONNX Runtime을 받지 않은 사람도 저장소를 그대로 빌드할 수 있다.

#include "bot_onnx.h"

#include "placement.h"
#include "../src/sim_game.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>

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
        } catch (const Ort::Exception& e) {
            if (err_out) *err_out = std::string("Ort::Exception: ") + e.what();
            session.reset();
            return false;
        } catch (const std::exception& e) {
            if (err_out) *err_out = std::string("std::exception: ") + e.what();
            session.reset();
            return false;
        }
        return true;
    }

    bool InferOnce(const SimGame& sim, int& col_out, int& rot_out)
    {
        if (!session) return false;

        float board[kBoardRows * kBoardCols];   // flatten (1, 1, 20, 10)
        float current[kNumPieceTypes];          // (1, 7)
        float nxt[kNumPieceTypes];              // (1, 7)
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

        std::vector<Ort::Value> outs;
        try {
            outs = session->Run(
                Ort::RunOptions{nullptr},
                inputNames.data(), inputs, 3,
                outputNames.data(), outputNames.size());
        } catch (const Ort::Exception&) {
            return false;
        }
        if (outs.empty()) return false;

        // 잘못 export된 모델은 shape을 물어보는 것만으로도 예외를 던진다.
        // 그래서 검증과 데이터 접근을 통째로 try 안에 둔다.
        const float* logits = nullptr;
        try {
            if (!outs[0].IsTensor()) return false;
            const auto info = outs[0].GetTensorTypeAndShapeInfo();
            if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                info.GetElementCount() < static_cast<size_t>(kNumPlacements)) {
                return false;
            }
            logits = outs[0].GetTensorData<float>();
        } catch (const Ort::Exception&) {
            return false;
        }
        // 출력은 항상 40개(10열 x 4회전)여야 한다.

        // 규칙상 둘 수 있는 자리만 남긴다. 모델이 뭘 내놓든 불법 수는 못 고른다.
        auto placements = sim.LegalPlacements();
        if (placements.empty()) return false;

        bool legal[kNumPlacements] = {false};
        for (const auto& p : placements) {
            int a = encode_action(p.col, p.rot);
            if (a >= 0 && a < kNumPlacements) legal[a] = true;
        }

        // 남은 것 중 점수가 제일 높은 자리를 고른다 (greedy).
        int   bestIdx = -1;
        float bestVal = -std::numeric_limits<float>::infinity();
        for (int i = 0; i < kNumPlacements; ++i) {
            if (!legal[i]) continue;
            if (logits[i] > bestVal) {
                bestVal = logits[i];
                bestIdx = i;
            }
        }
        if (bestIdx < 0) {
            // 합법 수는 있는데 전부 -inf인 경우. 모델이 NaN을 뱉으면 이렇게 된다.
            // 게임이 멈추는 것보다는 아무 수나 두는 편이 낫다.
            return fallback_placement(sim, col_out, rot_out);
        }
        decode_action(bestIdx, col_out, rot_out);
        return true;
    }
};

BotOnnx::BotOnnx() : impl_(std::make_unique<Impl>()) {}
BotOnnx::~BotOnnx() = default;

bool BotOnnx::Load(const std::string& onnx_path, std::string* err_out)
{
    if (!impl_) impl_ = std::make_unique<Impl>();
    return impl_->LoadModel(onnx_path, err_out);
}

bool BotOnnx::Infer(const SimGame& sim, int& col_out, int& rot_out)
{
    if (!impl_ || !impl_->session) return false;
    return impl_->InferOnce(sim, col_out, rot_out);
}

bool BotOnnx::IsLoaded() const
{
    return impl_ && impl_->session != nullptr;
}

#else  // !TETRIS_HAS_ONNXRUNTIME

// ONNX Runtime이 없을 때 쓰는 stub. Load는 언제나 실패하고,
// 호출자가 IsLoaded()로 걸러 주므로 Infer까지 오지 않는다.
struct BotOnnx::Impl { bool loaded = false; };

BotOnnx::BotOnnx() : impl_(std::make_unique<Impl>()) {}
BotOnnx::~BotOnnx() = default;

bool BotOnnx::Load(const std::string& onnx_path, std::string* err_out)
{
    (void)onnx_path;
    if (err_out) *err_out = "onnxruntime not vendored — rebuild with TETRIS_HAS_ONNXRUNTIME";
    return false;
}

bool BotOnnx::Infer(const SimGame&, int&, int&) { return false; }
bool BotOnnx::IsLoaded() const { return false; }

#endif  // TETRIS_HAS_ONNXRUNTIME

}  // namespace bot
