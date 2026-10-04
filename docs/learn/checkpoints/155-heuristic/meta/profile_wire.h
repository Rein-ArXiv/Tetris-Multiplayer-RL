#ifndef STUDY_META_PROFILE_WIRE_H
#define STUDY_META_PROFILE_WIRE_H

#include <cstdint>
#include <optional>
#include <string>

#include "meta/wire.h"

namespace study_meta {

// Profile fields contain no credential; access policy belongs to the service.
struct PublicProfile {
  std::uint64_t player_id = 0;
  int rp = 0;
  int xp = 0;
  int bp = 0;
};

// Explicit wire bound for the nonnegative 32-bit progression counters.
inline constexpr std::uint64_t kMaxInt32 = 2147483647ULL;

// Serialize a public profile as a flat JSON object with exactly four fields.
inline Json profile_json(const PublicProfile& p) {
  Json j = Json::object();
  j["player_id"] = p.player_id;  // full 64-bit id, never narrowed
  j["rp"] = p.rp;
  j["xp"] = p.xp;
  j["bp"] = p.bp;
  return j;
}

// Parse a flat 4-field JSON object into a validated PublicProfile.
inline std::optional<PublicProfile> parse_profile(const std::string& body) {
  // Require a strict, flat object with exactly the four known fields.
  std::optional<Json> json = object(body, 4);
  if (!json) return std::nullopt;

  // Every field must be a non-negative integer; number() rejects the rest.
  std::optional<std::uint64_t> player_id = number(*json, "player_id");
  std::optional<std::uint64_t> rp = number(*json, "rp");
  std::optional<std::uint64_t> xp = number(*json, "xp");
  std::optional<std::uint64_t> bp = number(*json, "bp");
  if (!player_id || !rp || !xp || !bp) return std::nullopt;

  // player_id must be positive; int fields must fit before any cast.
  if (*player_id == 0) return std::nullopt;
  if (*rp > kMaxInt32 || *xp > kMaxInt32 || *bp > kMaxInt32) return std::nullopt;

  PublicProfile p;
  p.player_id = *player_id;  // preserve full 64-bit id unchanged
  p.rp = static_cast<int>(*rp);
  p.xp = static_cast<int>(*xp);
  p.bp = static_cast<int>(*bp);
  return p;
}

}  // namespace study_meta

#endif  // STUDY_META_PROFILE_WIRE_H
