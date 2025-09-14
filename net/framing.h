#pragma once
#include <cstdint>
#include <vector>

// [NET] 간단한 메시지 프레이밍
// 바이트 레이아웃(리틀엔디안):
//   [LEN: u16][TYPE: u8][PAYLOAD: (LEN-1) bytes][CHK: u32]
// - LEN에는 TYPE(1바이트)+PAYLOAD 길이가 들어갑니다.
// - CHK는 PAYLOAD만 대상으로 한 FNV-1a 32비트 해시(무결성 확인용, 학습 목적)
// - 스트림(TCP)에서 경계가 보장되지 않으므로, 수신 버퍼에서 필요한 바이트가 모일 때까지 파싱합니다.

namespace net {

enum class MsgType : uint8_t {
    HELLO = 1,
    HELLO_ACK = 2,
    SEED = 3,
    INPUT = 4,
    ACK = 5,
    PING = 6,
    PONG = 7,
    HASH = 8,  // [NET] 상태 해시 교환(학습용 디버그)
};

struct Frame {
    MsgType type;
    std::vector<uint8_t> payload;
};

// 간단 FNV-1a 32비트 해시(무결성 체크)
uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed=2166136261u);

// 스트림 버퍼에서 가능한 만큼 프레임을 파싱해 out에 추가
// streamBuf(누적 수신 버퍼)에서 완성된 프레임을 가능한 만큼 파싱해 out에 추가합니다.
// 파싱한 바이트는 streamBuf에서 제거됩니다.
bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out);

// 프레임을 바이트로 직렬화
// 프레임을 직렬화하여 송신용 바이트 배열을 반환합니다.
std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload);

// 리틀엔디안 도우미(네트워크-내부 통일된 엔디안 사용)
void le_write_u16(std::vector<uint8_t>& v, uint16_t x);
void le_write_u32(std::vector<uint8_t>& v, uint32_t x);
void le_write_u64(std::vector<uint8_t>& v, uint64_t x);

uint16_t le_read_u16(const uint8_t* p);
uint32_t le_read_u32(const uint8_t* p);
uint64_t le_read_u64(const uint8_t* p);

}
