#pragma once
#include "net/paced_match.h"
#include "net/framing.h"
#include <cstddef>
#include <cstdint>
namespace study_net {
// Binds a trusted admission result to one byte stream. No actor field is read
// from the wire. The authority and this stream share one event-loop owner.
class PacedInputStream {
public:
    struct Report {
        std::size_t handled = 0, stored = 0, duplicates = 0, denied = 0;
        PacedReply last{};
    };
    PacedInputStream(PacedMatch& authority, std::uint64_t authenticated_actor)
        : authority_(authority), actor_(authenticated_actor) {}
    // now_ns is sampled by the owner when consuming received bytes.
    // Caller limits each socket read to 16 bytes. Drain parser before next read.
    // A malformed frame boundary latches failure. Semantic denials consume that frame; the match decides whether to stop.
    // Transport framing errors must cause the owner to discard this match.
    bool feed(const std::uint8_t* bytes, std::size_t count, std::int64_t now_ns, Report& report) noexcept {
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
            report.last = authority_.submit(actor_, frame, now_ns);
            if (report.last.input == InputDecision::stored) ++report.stored;
            else if (report.last.input == InputDecision::duplicate) ++report.duplicates;
            else ++report.denied;
        }
    }
    bool failed() const noexcept { return failed_; }
private:
    PacedMatch& authority_;
    const std::uint64_t actor_;
    FrameParser parser_;
    bool failed_ = false;
};
} // namespace study_net
