#include "framing.h"

// 한계/필드 크기 상수는 framing.h 의 공개 상수(kMaxPayloadBytes 등)를 쓴다.
// 과거엔 여기 익명 네임스페이스에 중복 정의돼 있었는데, relay/Python 미러와 값이
// 어긋나면 스트림 오염으로 오판되는 함정이 있어 헤더로 승격했다 — framing.h 주석 참고.

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
    if (payload.size() > kMaxPayloadBytes) return {};
    // LEN = TYPE(1) + PAYLOAD(N)
    std::vector<uint8_t> out; out.reserve(kFrameLenBytes + kFrameTypeBytes + payload.size() + kFrameChecksumBytes);
    const uint16_t len = static_cast<uint16_t>(kFrameTypeBytes + payload.size());
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
        if (offset + kFrameLenBytes > streamBuf.size()) break;

        // LEN = TYPE + PAYLOAD 길이
        const uint16_t len = le_read_u16(&streamBuf[offset]);

        // Reject an oversized declaration before buffering the payload.
        if (static_cast<size_t>(len) > kMaxPayloadBytes + kFrameTypeBytes) {
            streamBuf.clear();
            return false;
        }

        const size_t need = kFrameLenBytes + static_cast<size_t>(len) + kFrameChecksumBytes;
        if (offset + need > streamBuf.size()) break;

        // A zero length frame has no type byte.
        if (len < kFrameTypeBytes) { offset += need; continue; }

        const uint8_t type = streamBuf[offset + kFrameLenBytes];
        const uint8_t* payload = &streamBuf[offset + kFrameLenBytes + kFrameTypeBytes];
        const size_t payloadLen = static_cast<size_t>(len) - kFrameTypeBytes; // LEN - TYPE(1)

        const size_t chkPos = offset + kFrameLenBytes + static_cast<size_t>(len);
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
