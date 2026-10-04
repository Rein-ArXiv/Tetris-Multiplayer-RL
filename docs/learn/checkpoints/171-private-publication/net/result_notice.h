#pragma once
#include "net/notice_reason.h"
#include "net/byte_codec.h"
#include <stdexcept>
namespace study_net {
// Stable study wire: key:u64, reason:u8. Not the root MATCH_RESULT encoding.
inline constexpr std::uint8_t kResultNoticeType=43;
struct ResultNotice { std::uint64_t key=0;NoticeReason reason=NoticeReason::unknown; };
inline bool encode_notice(const ResultNotice& notice,Frame& out) noexcept {
    if(!notice.key)return false;
    Frame next;next.type=kResultNoticeType;
    ByteWriter writer(next.payload.data(),next.payload.size());
    if(!writer.u64(notice.key) || !writer.u8(static_cast<std::uint8_t>(notice.reason)))return false;
    next.size=writer.position();out=next;return true;
}
inline bool decode_notice(const Frame& frame,ResultNotice& out) noexcept {
    if(frame.type!=kResultNoticeType || frame.size!=9)return false;
    ByteReader reader(frame.payload.data(),frame.size);
    std::uint64_t key;std::uint8_t raw;
    if(!reader.u64(key) || !key || !reader.u8(raw) || !reader.at_end())return false;
    // Future reasons remain readable as unknown; no optimistic success fallback.
    out={key,raw<=static_cast<std::uint8_t>(NoticeReason::unknown)
                 ?static_cast<NoticeReason>(raw):NoticeReason::unknown};
    return true;
}
// This consumer is bound to an admitted server connection and one expected key.
// Matching an ID does not authenticate an arbitrary remote sender.
class NoticeView {
public:
    explicit NoticeView(std::uint64_t expected):expected_(expected) {
        if(!expected)throw std::invalid_argument("expected result key");
    }
    bool accept(const Frame& frame) noexcept {
        ResultNotice next;
        if(!decode_notice(frame,next) || next.key!=expected_)return false;
        reason_=next.reason;return true;
    }
    NoticeReason reason()const noexcept{return reason_;}
    const char* text()const noexcept{return notice_text(reason_);}
private:
    const std::uint64_t expected_;
    NoticeReason reason_=NoticeReason::waiting;
};
} // namespace study_net
