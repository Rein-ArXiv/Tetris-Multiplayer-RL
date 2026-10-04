#pragma once
namespace study_meta {
// id is acceptance order in this append-only study log, not a wall-clock time.
inline constexpr const char* kRecentMatches =
    "SELECT id,match_key FROM matches WHERE player_a=?1 "
    "UNION ALL "
    "SELECT id,match_key FROM matches WHERE player_b=?1 "
    "ORDER BY id DESC LIMIT ?2";

inline constexpr const char* kRecentMatchesOr =
    "SELECT id,match_key FROM matches WHERE player_a=?1 OR player_b=?1 "
    "ORDER BY id DESC LIMIT ?2";

inline constexpr const char* kHistoryIndexes = R"sql(
CREATE INDEX IF NOT EXISTS idx_study_matches_a_recent
  ON matches(player_a,id DESC,match_key);
CREATE INDEX IF NOT EXISTS idx_study_matches_b_recent
  ON matches(player_b,id DESC,match_key);
)sql";
} // namespace study_meta
