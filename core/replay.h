#pragma once
#include <vector>
#include <cstdint>
#include <string>

// [NET] 간단한 리플레이 포맷: 세션 시드 + 틱별 입력(p1,p2)
struct FrameInputs {
    uint8_t p1{0};
    uint8_t p2{0};
};

struct ReplayData {
    uint64_t seed{0};
    std::vector<FrameInputs> frames;
};

// 매우 단순한 텍스트 포맷으로 저장/로드(의존성 최소화)
namespace ReplayIO {
    bool Save(const std::string& path, const ReplayData& rp);
    bool Load(const std::string& path, ReplayData& out);
}

