#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 학습한 정책을 게임 안에서 돌리기 위한 ONNX Runtime wrapper.
//
// 학습은 Colab에서 PyTorch로 하고, 그 결과를 export_onnx.py로 .onnx 파일에
// 내보낸다. 게임 쪽은 그 파일만 읽으면 되므로 PyTorch를 설치할 필요가 없다.
// 배포 머신에 필요한 것은 onnxruntime 공유 라이브러리 하나뿐이다.
//
// 학습 쪽과 맞춰야 하는 입출력 계약:
//   입력  "board"   (1, 1, 20, 10) float32 — 칸이 차 있으면 1
//         "current" (1, 7)         float32 — 현재 블록 one-hot
//         "next"    (1, 7)         float32 — 다음 블록 one-hot
//   출력  "policy_logits" (1, 40)  float32 — 40가지 placement의 점수
//         "value"         (1,)     float32 — 학습에만 쓰고 여기선 무시
//
// 이름과 shape이 어긋나면 로드는 되고 추론에서 터진다. 바꿀 일이 있으면
// python/netbot/export_onnx.py의 INPUT_NAMES/OUTPUT_NAMES도 같이 고친다.

class SimGame;

namespace bot {

class BotOnnx {
public:
    BotOnnx();
    ~BotOnnx();

    BotOnnx(const BotOnnx&) = delete;
    BotOnnx& operator=(const BotOnnx&) = delete;

    // .onnx 파일을 읽는다. 파일이 없거나, 깨졌거나, 입출력 이름이 위 계약과
    // 다르면 false. 이 경우 err_out에 화면에 그대로 띄울 수 있는 사유가 담긴다.
    // 실패해도 예외를 던지지 않는다 — 모델이 없는 것은 정상 상황이고
    // 호출자는 heuristic bot으로 넘어가면 된다.
    bool Load(const std::string& onnx_path, std::string* err_out = nullptr);

    // 현재 판을 보고 둘 곳을 정한다.
    // 불법 수의 logit을 -inf로 눌러 놓고 최댓값을 고르므로, 모델이 이상한
    // 값을 내도 규칙에 어긋난 수는 나오지 않는다.
    // 둘 곳이 아예 없으면(게임 오버 직전) false.
    bool Infer(const SimGame& sim, int& col_out, int& rot_out);

    bool IsLoaded() const;

private:
    // PImpl. onnxruntime 헤더를 .cpp 안에만 두려는 것이다.
    // 이 헤더를 include하는 쪽은 ONNX Runtime 없이도 컴파일된다.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bot
