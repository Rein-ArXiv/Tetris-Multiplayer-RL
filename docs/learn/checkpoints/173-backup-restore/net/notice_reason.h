#ifndef STUDY_NET_NOTICE_REASON_H
#define STUDY_NET_NOTICE_REASON_H

#include <cstdint>

#include "net/relay_channel.h"

namespace study_net {

// UI-facing reason for a relay channel's current state.
// Pure mapping only: no I/O, clock, keys or remote requests.
enum class NoticeReason : std::uint8_t {
  waiting = 0,
  pending = 1,
  invalid = 2,
  incomplete = 3,
  budget_exhausted = 4,
  saved = 5,
  draw = 6,
  unconfirmed = 7,
  unknown = 8,
};

namespace detail {

inline NoticeReason reason_from_match_state(MatchState s) noexcept {
  switch (s) {
    case MatchState::invalid:
      return NoticeReason::invalid;
    case MatchState::incomplete:
      return NoticeReason::incomplete;
    case MatchState::budget_exhausted:
      return NoticeReason::budget_exhausted;
    default:
      return NoticeReason::unknown;
  }
}

}  // namespace detail

inline NoticeReason notice_reason(const ChannelView& view) noexcept {
  switch (view.saving) {
    case ResultHandoff::Stage::open:
      return NoticeReason::waiting;
    case ResultHandoff::Stage::inflight:
      // In-flight is only "pending" once a finished result exists.
      return view.match.state == MatchState::finished ? NoticeReason::pending
                                                     : NoticeReason::unknown;
    case ResultHandoff::Stage::not_eligible:
      // Sealed without a storable result, including an incomplete match.
      return detail::reason_from_match_state(view.match.state);
    case ResultHandoff::Stage::confirmed:
      if (view.match.state != MatchState::finished) {
        return NoticeReason::unknown;
      }
      switch (view.match.winner) {
        case Winner::host:
        case Winner::peer:
          return NoticeReason::saved;
        case Winner::draw:
          return NoticeReason::draw;
        default:
          return NoticeReason::unknown;
      }
    case ResultHandoff::Stage::unconfirmed:
      return view.match.state == MatchState::finished
                 ? NoticeReason::unconfirmed
                 : NoticeReason::unknown;
    default:
      return NoticeReason::unknown;
  }
}

inline const char* notice_text(NoticeReason reason) noexcept {
  switch (reason) {
    case NoticeReason::waiting:
      return "경기 결과를 판정하는 중입니다.";
    case NoticeReason::pending:
      return "결과를 저장하는 중입니다.";
    case NoticeReason::invalid:
      // Never accuses the participant; the submission is simply not valid.
      return "입력 검증을 통과하지 못해 이번 경기에 보상이 적용되지 않았습니다.";
    case NoticeReason::incomplete:
      return "서버에서 경기가 완결되지 않아 이번 결과를 저장하지 않았습니다.";
    case NoticeReason::budget_exhausted:
      // No verified terminal result; not the same as a normal draw.
      return "진행 한도에 도달해 판정이 종료되었습니다. 보상 결과는 없습니다.";
    case NoticeReason::saved:
      // Server result and storage are confirmed; this is not delivery proof.
      return "경기 결과가 확인되어 저장되었습니다.";
    case NoticeReason::draw:
      return "무승부로 저장되었습니다.";
    case NoticeReason::unconfirmed:
      // Storage is NOT CONFIRMED. Not a rollback and not a lack of reward.
      return "저장 여부를 확인하지 못했습니다. 프로필을 다시 확인해 주세요.";
    case NoticeReason::unknown:
      return "결과 상태를 확인할 수 없습니다.";
    default:
      return "결과 상태를 확인할 수 없습니다.";
  }
}

}  // namespace study_net

#endif  // STUDY_NET_NOTICE_REASON_H
