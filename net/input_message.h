#pragma once
#include "framing.h"
#include "../core/input.h"
#include <limits>

namespace net {
// Borrowed view: payload storage must outlive its consumption. No allocation.
struct InputBatchView {
    uint32_t first_tick{};
    uint16_t count{};
    const uint8_t* masks{};
};

// INPUT = [first_tick:u32LE][count:u16LE][mask:count]. Validate the entire
// message before callers mutate their input queue. Failed output is unchanged.
inline bool decode_input_payload(const std::vector<uint8_t>& payload,
                                 InputBatchView& out) noexcept {
    constexpr size_t header = 6;
    if (payload.size() < header || payload.size() > kMaxPayloadBytes) return false;
    const uint32_t first = le_read_u32(payload.data());
    const uint16_t count = le_read_u16(payload.data() + 4);
    if (count == 0 || payload.size() - header != count) return false;
    // Sequence numbers do not wrap within one accepted batch.
    if (static_cast<uint32_t>(count - 1) >
        (std::numeric_limits<uint32_t>::max)() - first) return false;
    const uint8_t* masks = payload.data() + header;
    for (uint16_t i = 0; i < count; ++i) {
        if (!isValidInputMask(masks[i])) return false;
    }
    out = InputBatchView{first, count, masks};
    return true;
}
} // namespace net
