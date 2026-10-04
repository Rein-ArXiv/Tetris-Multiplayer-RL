#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 학습한 정책을 게임 안에서 돌리기 위한 ONNX Runtime wrapper.
//
// 학습은 Colab에서 PyTorch로 하고, 그 결과를 export_onnx.py로 .onnx 파일에
// 내보낸다. 게임 쪽은 그 파일만 읽으면 되므로 PyTorch를 설치할 필요가 없다.
// 대상 OS/CPU에 맞는 ONNX Runtime 공유 라이브러리와 그 시스템 의존성이 필요하다.
//
// float32 I/O: board=(1,1,kBoardRows,kBoardCols), current/next=(1,kNumPieceTypes),
// policy_logits=(1,kNumPlacements), value=(1,). Names/order match export_onnx.py.
// Load checks structural I/O; observation/action semantics require a matching exporter.
// This wrapper does not authenticate models or sandbox an untrusted graph.

class SimGame;

namespace bot {

class BotOnnx {
public:
    BotOnnx();
    ~BotOnnx();

    BotOnnx(const BotOnnx&) = delete;
    BotOnnx& operator=(const BotOnnx&) = delete;

    // .onnx 파일을 읽는다. 파일이 없거나, 깨졌거나, 입출력 이름이 위 계약과
    // 다르면 false. 이 경우 err_out에 개발 로그용 상세 사유가 담긴다.
    // 런타임/파일 오류는 false로 반환한다. 실패하면 미로드 상태, 성공하면 err_out은 빈 값.
    // 진단 문자열 할당까지 noexcept로 보장하지는 않는다. 호출은 한 소유 스레드에서 직렬화한다.
    bool Load(const std::string& onnx_path, std::string* err_out = nullptr);

    // 현재 판을 보고 둘 곳을 정한다.
    // 출력의 타입·형상·유한성을 검사하고 합법 후보 중 첫 최댓값을 고른다.
    // 실행 오류/비유한 출력/빈 후보는 false이며 col_out/rot_out을 보존한다.
    // 대체 정책을 쓸지는 호출자가 결정한다. 로드·추론·파괴를 동시에 호출하지 않는다.
    bool Infer(const SimGame& sim, int& col_out, int& rot_out);

    bool IsLoaded() const;

private:
    // PImpl. onnxruntime 헤더를 .cpp 안에만 두려는 것이다.
    // 이 헤더를 include하는 쪽은 ONNX Runtime 없이도 컴파일된다.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bot
