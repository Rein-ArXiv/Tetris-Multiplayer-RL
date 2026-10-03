#pragma once
namespace study_meta {
inline constexpr const char* kRewardSchema = R"sql(
CREATE TABLE wallets (
  player_id TEXT PRIMARY KEY NOT NULL REFERENCES players(id) ON DELETE CASCADE,
  bp INTEGER NOT NULL DEFAULT 0 CHECK(typeof(bp)='integer' AND bp BETWEEN 0 AND 2147483647)
);
CREATE TABLE match_rewards (
  match_id INTEGER NOT NULL REFERENCES matches(id),
  player_id TEXT NOT NULL REFERENCES players(id),
  bp_delta INTEGER NOT NULL CHECK(typeof(bp_delta)='integer' AND bp_delta BETWEEN 0 AND 10),
  policy INTEGER NOT NULL CHECK(policy IN (0,1)),
  PRIMARY KEY(match_id,player_id)
);
)sql";
// Historical matches remain recorded but receive no retroactive BP.
inline constexpr const char* kRewardBackfill = R"sql(
INSERT INTO wallets(player_id) SELECT id FROM players;
INSERT INTO match_rewards(match_id,player_id,bp_delta,policy)
  SELECT id,player_a,0,0 FROM matches
  UNION ALL SELECT id,player_b,0,0 FROM matches;
)sql";
} // namespace study_meta
