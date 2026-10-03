#pragma once
namespace study_meta {
inline constexpr const char* kProgressionSchema = R"sql(
CREATE TABLE careers (
  player_id TEXT PRIMARY KEY NOT NULL REFERENCES players(id) ON DELETE CASCADE,
  rp INTEGER NOT NULL DEFAULT 0 CHECK(typeof(rp)='integer' AND rp BETWEEN 0 AND 2147483647),
  xp INTEGER NOT NULL DEFAULT 0 CHECK(typeof(xp)='integer' AND xp BETWEEN 0 AND 2147483647)
);
CREATE TABLE match_progress (
  match_id INTEGER NOT NULL REFERENCES matches(id),
  player_id TEXT NOT NULL REFERENCES players(id),
  rp_before INTEGER NOT NULL CHECK(typeof(rp_before)='integer' AND rp_before BETWEEN 0 AND 2147483647),
  rp_after INTEGER NOT NULL CHECK(typeof(rp_after)='integer' AND rp_after BETWEEN 0 AND 2147483647),
  xp_delta INTEGER NOT NULL CHECK(typeof(xp_delta)='integer' AND xp_delta IN (0,50,100)),
  policy INTEGER NOT NULL CHECK(policy IN (0,1)),
  PRIMARY KEY(match_id,player_id)
);
)sql";
// Existing matches retain their BP receipts but receive no retroactive RP/XP.
inline constexpr const char* kProgressionBackfill = R"sql(
INSERT INTO careers(player_id) SELECT id FROM players;
INSERT INTO match_progress(match_id,player_id,rp_before,rp_after,xp_delta,policy)
  SELECT id,player_a,0,0,0,0 FROM matches
  UNION ALL SELECT id,player_b,0,0,0,0 FROM matches;
)sql";
} // namespace study_meta
