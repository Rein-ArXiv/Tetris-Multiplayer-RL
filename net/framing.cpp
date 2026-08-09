#include "framing.h"
#include <cstring>

namespace {
    constexpr size_t LEN_FIELD = 2;       // u16
    constexpr size_t TYPE_FIELD = 1;      // u8
    constexpr size_t CHECKSUM_FIELD = 4;  // u32
    constexpr size_t MIN_FRAME_BYTES = LEN_FIELD + TYPE_FIELD + CHECKSUM_FIELD; // 7 bytes
    // Normal chat is the largest message and remains below this limit.
    constexpr size_t MAX_PAYLOAD_BYTES = 4096;
}

namespace net {

uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed) {
    uint32_t h = seed;
    for (size_t i = 0; i < len; ++i) { h ^= data[i]; h *= 16777619u; }
    return h;
}

void le_write_u16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)(x & 0xFF)); v.push_back((uint8_t)((x>>8)&0xFF));
}
void le_write_u32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i=0;i<4;++i) v.push_back((uint8_t)((x>>(8*i))&0xFF));
}
void le_write_u64(std::vector<uint8_t>& v, uint64_t x) {
    for (int i=0;i<8;++i) v.push_back((uint8_t)((x>>(8*i))&0xFF));
}
uint16_t le_read_u16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
uint32_t le_read_u32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
uint64_t le_read_u64(const uint8_t* p) {
    // 리틀엔디안: p[0]이 최하위 바이트
    uint64_t x=0; for (int i=7;i>=0;--i){ x = (x<<8) | p[i]; } return x;
}

std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload) {
    // 발신 측에서도 페이로드 상한을 검사 — 초과 시 빈 벡터로 실패.
    if (payload.size() > MAX_PAYLOAD_BYTES) return {};
    // LEN = TYPE(1) + PAYLOAD(N)
    std::vector<uint8_t> out; out.reserve(LEN_FIELD + TYPE_FIELD + payload.size() + CHECKSUM_FIELD);
    const uint16_t len = static_cast<uint16_t>(TYPE_FIELD + payload.size());
    le_write_u16(out, len);
    out.push_back(static_cast<uint8_t>(t));
    out.insert(out.end(), payload.begin(), payload.end());
    // CHK = FNV-1a32(PAYLOAD)
    const uint32_t chk = payload.empty() ? 0u : fnv1a32(payload.data(), payload.size());
    le_write_u32(out, chk);
    return out;
}

bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out) {
    size_t offset = 0;
    while (true) {
        // Addition avoids unsigned underflow in a subtraction check.
        if (offset + LEN_FIELD > streamBuf.size()) break;

        // LEN = TYPE + PAYLOAD 길이
        const uint16_t len = le_read_u16(&streamBuf[offset]);

        // Reject an oversized declaration before buffering the payload.
        if (static_cast<size_t>(len) > MAX_PAYLOAD_BYTES + TYPE_FIELD) {
            streamBuf.clear();
            return false;
        }

        const size_t need = LEN_FIELD + static_cast<size_t>(len) + CHECKSUM_FIELD;
        if (offset + need > streamBuf.size()) break;

        // A zero length frame has no type byte.
        if (len < TYPE_FIELD) { offset += need; continue; }

        const uint8_t type = streamBuf[offset + LEN_FIELD];
        const uint8_t* payload = &streamBuf[offset + LEN_FIELD + TYPE_FIELD];
        const size_t payloadLen = static_cast<size_t>(len) - TYPE_FIELD; // LEN - TYPE(1)

        const size_t chkPos = offset + LEN_FIELD + static_cast<size_t>(len);
        const uint32_t chk = le_read_u32(&streamBuf[chkPos]);
        const uint32_t calc = (payloadLen == 0) ? 0u : fnv1a32(payload, payloadLen);

        if (chk == calc) {
            Frame f; f.type = static_cast<MsgType>(type);
            f.payload.assign(payload, payload + payloadLen);
            out.push_back(std::move(f));
        }

        offset += need;
    }
    // Keep the incomplete tail for the next receive.
    if (offset > 0) streamBuf.erase(streamBuf.begin(), streamBuf.begin() + offset);
    return true;
}

}
