// bot/bot_onnx.cpp — ONNX Runtime C++ API 래퍼. 헤더 설명은 bot_onnx.h 참조.
//
// 이 파일은 항상 컴파일된다. TETRIS_BUILD_BOT=ON 이면 CMake 가
// TETRIS_HAS_ONNXRUNTIME 을 정의하고 third_party/onnxruntime/ 헤더/라이브러리를
// 연결한다. OFF 이면 아래의 stub 구현이 빌드되어 Load() 가 실패한다.

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

    // 입출력 이름 — Python export_onnx 에서 고정.
    std::array<const char*, 3> inputNames  = {"board", "current", "next"};
    std::array<const char*, 2> outputNames = {"policy_logits", "value"};

    bool LoadModel(const std::string& path, std::string* err_out)
    {
        try {
            sessOpts.SetIntraOpNumThreads(1);
            sessOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        #if defined(_WIN32)
            // BotEntry 경로 문자열은 UTF-8로 정규화된다. u8path를 거쳐야 한글
            // 사용자 폴더 같은 비ASCII 경로가 현재 C locale과 무관하게 보존된다.
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

        // 잘못 export 된 출력은 shape/type 조회 자체가 예외를 던질 수 있다.
        // 모든 검증과 데이터 접근을 ORT 예외 경계 안에 둔다.
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
        // kNumPlacements = 40 고정.

        // 합법 마스크: LegalPlacements 를 돌려 (col, rot) 집합을 bitset 으로.
        auto placements = sim.LegalPlacements();
        if (placements.empty()) return false;

        bool legal[kNumPlacements] = {false};
        for (const auto& p : placements) {
            int a = encode_action(p.col, p.rot);
            if (a >= 0 && a < kNumPlacements) legal[a] = true;
        }

        // masked argmax
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
            // 모든 합법 logits 가 -inf 였다는 뜻 — 정상 케이스는 아님.
            // fallback 으로 사전순 최소 합법 수를 선택.
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

// ORT 바이너리가 준비되기 전에도 빌드가 돌아가도록 스텁. Load 는 항상 실패.
// Infer 경로는 호출자가 IsLoaded 로 가드하므로 도달하지 않는다.
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
