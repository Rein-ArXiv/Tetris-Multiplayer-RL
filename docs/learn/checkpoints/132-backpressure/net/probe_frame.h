#ifndef STUDY_NET_PROBE_FRAME_H
#define STUDY_NET_PROBE_FRAME_H
#include "net/framing.h"
#include "net/byte_codec.h"
namespace study_net {
// Loopback measurement protocol, not simulation input or an authenticated reply.
inline Frame probe_frame(std::uint32_t round, std::uint32_t slot) {
    Frame frame{};
    frame.type = 60;
    frame.size = 8;
    ByteWriter writer(frame.payload.data(), frame.size);
    writer.u32(round);
    writer.u32(slot);
    return frame;
}
inline bool matches_probe(const Frame& frame, std::uint32_t round, std::uint32_t slot) {
    if (frame.type != 60 || frame.size != 8) return false;
    ByteReader reader(frame.payload.data(), frame.size);
    std::uint32_t got_round = 0, got_slot = 0;
    return reader.u32(got_round) && reader.u32(got_slot) && reader.at_end() &&
           got_round == round && got_slot == slot;
}
} // namespace study_net
#endif
