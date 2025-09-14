#pragma once
#include <cstdint>
#include <vector>

// [NET] 간단한 메시지 프레이밍: [len:u16][type:u8][payload...][chk:u32]
// chk는 FNV-1a 32비트로 페이로드 무결성을 간단히 확인합니다.

namespace net {

enum class MsgType : uint8_t {
    HELLO = 1,
    HELLO_ACK = 2,
    SEED = 3,
    INPUT = 4,
    ACK = 5,
    PING = 6,
    PONG = 7,
};

struct Frame {
    MsgType type;
    std::vector<uint8_t> payload;
};

uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed=2166136261u);

// 스트림 버퍼에서 가능한 만큼 프레임을 파싱해 out에 추가
bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out);

// 프레임을 바이트로 직렬화
std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload);

// 리틀엔디안 도우미
void le_write_u16(std::vector<uint8_t>& v, uint16_t x);
void le_write_u32(std::vector<uint8_t>& v, uint32_t x);
void le_write_u64(std::vector<uint8_t>& v, uint64_t x);

uint16_t le_read_u16(const uint8_t* p);
uint32_t le_read_u32(const uint8_t* p);
uint64_t le_read_u64(const uint8_t* p);

}

