#ifndef STUDY_META_REWARD_POLICY_H
#define STUDY_META_REWARD_POLICY_H

#include "net/match_submission.h"

#include <cstdint>
#include <stdexcept>

namespace study_meta {

// Fixed study policy version 1 BP awards.
// These are study defaults, not production balance values.
struct Awards {
  int a;
  int b;
};

// Maps a match winner to the version 1 study awards.
// draw -> {0, 0}; a wins -> {10, 3}; b wins -> {3, 10}.
inline Awards awards_for(std::uint8_t winner) {
  switch (winner) {
    case study_net::MatchRecord::draw:
      return Awards{0, 0};
    case study_net::MatchRecord::a:
      return Awards{10, 3};
    case study_net::MatchRecord::b:
      return Awards{3, 10};
    default:
      throw std::invalid_argument("awards_for: unknown winner code");
  }
}

// Upper bound for a credited balance; keeps arithmetic within int range.
inline constexpr int kBalanceLimit = 2147483647;

// Credits delta to before, rejecting negative inputs and overflow.
// The precondition test avoids computing before + delta directly.
inline int credited_balance(int before, int delta) {
  if (before < 0 || delta < 0 || delta > kBalanceLimit - before) {
    throw std::overflow_error("credited_balance: invalid or overflowing credit");
  }
  return before + delta;
}

}  // namespace study_meta

#endif  // STUDY_META_REWARD_POLICY_H
