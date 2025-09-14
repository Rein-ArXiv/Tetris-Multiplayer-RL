#include "framing.h"
#include <cstring>

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
    uint64_t x=0; for(int i=7;i>=0;--i){ x = (x<<8) | p[i]; } return x;
}

std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out; out.reserve(2+1+payload.size()+4);
    uint16_t len = (uint16_t)(1 + payload.size());
    le_write_u16(out, len);
    out.push_back((uint8_t)t);
    out.insert(out.end(), payload.begin(), payload.end());
    uint32_t chk = fnv1a32(payload.data(), payload.size());
    le_write_u32(out, chk);
    return out;
}

bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out) {
    size_t offset = 0;
    while (true) {
        if (streamBuf.size() - offset < 2) break;
        uint16_t len = le_read_u16(&streamBuf[offset]);
        if (streamBuf.size() - offset < 2 + len + 4) break;
        uint8_t type = streamBuf[offset+2];
        const uint8_t* payload = &streamBuf[offset+3];
        uint32_t chk = le_read_u32(&streamBuf[offset+2+len]);
        uint32_t calc = fnv1a32(payload, len-1);
        if (chk == calc) {
            Frame f; f.type = (MsgType)type; f.payload.assign(payload, payload + (len-1));
            out.push_back(std::move(f));
        }
        offset += 2 + len + 4;
    }
    if (offset > 0) {
        streamBuf.erase(streamBuf.begin(), streamBuf.begin()+offset);
    }
    return true;
}

}

