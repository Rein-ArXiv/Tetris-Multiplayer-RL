#pragma once
#include "net/input_authority.h"
#include "net/framing.h"
#include <cstddef>
#include <cstdint>
namespace study_net {
// Binds a trusted admission result to one byte stream. No actor field is read
// from the wire. The authority and this stream share one event-loop owner.
class BoundInputStream {
public:
    struct Report {
        std::size_t handled = 0, stored = 0, duplicates = 0, denied = 0;
        InputDecision last = InputDecision::inactive;
    };
    BoundInputStream(InputAuthority& authority, std::uint64_t authenticated_actor)
        : authority_(authority), actor_(authenticated_actor) {}
    // Caller limits each socket read to 16 bytes. Drain parser before next read.
    // A malformed frame boundary latches failure. Semantic denials consume only
    // that frame and leave the stream available for a later legitimate request.
    bool feed(const std::uint8_t* bytes, std::size_t count, Report& report) noexcept {
        report = {};
        if (failed_) return false;
        if (count > 16 || (count && !bytes) || !parser_.append(bytes, count)) {
            failed_ = true;
            return false;
        }
        for (;;) {
            Frame frame;
            const auto parsed = parser_.next(frame);
            if (parsed == ParseStatus::need_more) return true;
            if (parsed == ParseStatus::error) { failed_ = true; return false; }
            ++report.handled;
            report.last = authority_.submit(actor_, frame);
            if (report.last == InputDecision::stored) ++report.stored;
            else if (report.last == InputDecision::duplicate) ++report.duplicates;
            else ++report.denied;
        }
    }
    bool failed() const noexcept { return failed_; }
private:
    InputAuthority& authority_;
    const std::uint64_t actor_;
    FrameParser parser_;
    bool failed_ = false;
};
} // namespace study_net
