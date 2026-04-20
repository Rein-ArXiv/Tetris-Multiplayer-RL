#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// bot/bot_onnx.h — ONNX Runtime(C++) 기반 정책 추론 래퍼.
//
// 학습(Python PyTorch) 측에서 TetrisPolicyNet 을 export_onnx.py 로 .onnx 로
// 내보낸 뒤, 런타임에서 이 파일이 그 모델을 로드해 forward 를 한 번 돌린다.
// C++ 쪽은 PyTorch 의존성 없음 — onnxruntime.dll/.dylib/.so 하나면 충분.
//
// 학습 스크립트와의 계약(shape):
//   입력:
//     "board"   : (1, 1, 20, 10) float32, 점유 0/1
//     "current" : (1, 7)         float32, one-hot
//     "next"    : (1, 7)         float32, one-hot
//   출력:
//     "policy_logits" : (1, 40) float32
//     "value"         : (1,)    float32   (사용 안 함, 로드만)
//
// 편의 Infer():
//   1) SimGame 에서 observe 로 입력 텐서 채움
//   2) session_.Run()
//   3) LegalPlacements 마스크로 불법 액션 logits 를 -inf 로 치환
//   4) argmax 후 (col, rot) 분해
//   실패 또는 합법 수 0 → false 반환.
// ─────────────────────────────────────────────────────────────────────────────

class SimGame;

namespace bot {

class BotOnnx {
public:
    BotOnnx();
    ~BotOnnx();

    BotOnnx(const BotOnnx&) = delete;
    BotOnnx& operator=(const BotOnnx&) = delete;

    // .onnx 파일 로드. 실패(파일 없음, 손상, 바인딩 불일치) 시 false.
    // 실패 시 err_out 에 사람이 읽을 수 있는 사유가 담긴다.
    bool Load(const std::string& onnx_path, std::string* err_out = nullptr);

    // 로드된 모델로 (col, rot) 산출. 내부에서 observe + masked argmax.
    // 합법 수 0 → false.
    bool Infer(const SimGame& sim, int& col_out, int& rot_out);

    bool IsLoaded() const;

private:
    // PImpl — onnxruntime 헤더를 .cpp 안에만 가두기 위해.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bot
