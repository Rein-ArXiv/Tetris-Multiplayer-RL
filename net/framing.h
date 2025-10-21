#pragma once
#include <cstdint>
#include <vector>

// 메시지 프레이밍: TCP 스트림에서 메시지 경계 구분
// 프레임 구조: [LEN:2][TYPE:1][PAYLOAD:LEN-1][CHECKSUM:4]
// 상세: DOCUMENTATION.md

namespace net {

// 메시지 타입
enum class MsgType : uint8_t {
    HELLO = 1,
    HELLO_ACK = 2,
    SEED = 3,
    INPUT = 4,
    ACK = 5,
    PING = 6,
    PONG = 7,
    HASH = 8,
    GAME_OVER_CHOICE = 9,
};

// 파싱된 메시지 프레임
struct Frame {
    MsgType type;
    std::vector<uint8_t> payload;
};

// FNV-1a 32-bit 해시 (체크섬용)
uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed=2166136261u);

// 스트림 파싱: 누적 버퍼에서 완성된 프레임들 추출 (부분 수신 처리)
bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out);

// 메시지 직렬화: TYPE + PAYLOAD → 프레임 바이트 배열
std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload);

// 리틀엔디안 직렬화/역직렬화
void le_write_u16(std::vector<uint8_t>& v, uint16_t x);
void le_write_u32(std::vector<uint8_t>& v, uint32_t x);
void le_write_u64(std::vector<uint8_t>& v, uint64_t x);
uint16_t le_read_u16(const uint8_t* p);
uint32_t le_read_u32(const uint8_t* p);
uint64_t le_read_u64(const uint8_t* p);

}
