#pragma once
namespace study_meta {
// Canonical decimal TEXT preserves the full uint64_t wire domain without narrowing.
inline constexpr const char* kStudySchema = R"sql(
CREATE TABLE IF NOT EXISTS icons (
  id TEXT PRIMARY KEY NOT NULL CHECK(length(id)>0),
  label TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS players (
  id TEXT PRIMARY KEY NOT NULL
    CHECK(typeof(id)='text'
      AND instr(id,char(0))=0
      AND length(id) BETWEEN 1 AND 20
      AND id NOT GLOB '*[^0-9]*'
      AND (id='0' OR substr(id,1,1)<>'0')
      AND (length(id)<20 OR id<='18446744073709551615')
      AND id<>'0'),
  display_name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS player_icons (
  player_id TEXT NOT NULL REFERENCES players(id) ON DELETE CASCADE,
  icon_id TEXT NOT NULL REFERENCES icons(id),
  PRIMARY KEY(player_id,icon_id)
);
CREATE TABLE IF NOT EXISTS matches (
  id INTEGER PRIMARY KEY CHECK(id>0),
  match_key TEXT NOT NULL UNIQUE
    CHECK(typeof(match_key)='text'
      AND instr(match_key,char(0))=0
      AND length(match_key) BETWEEN 1 AND 20
      AND match_key NOT GLOB '*[^0-9]*'
      AND (match_key='0' OR substr(match_key,1,1)<>'0')
      AND (length(match_key)<20 OR match_key<='18446744073709551615')
      AND match_key<>'0'),
  round TEXT NOT NULL
    CHECK(typeof(round)='text'
      AND instr(round,char(0))=0
      AND length(round) BETWEEN 1 AND 20
      AND round NOT GLOB '*[^0-9]*'
      AND (round='0' OR substr(round,1,1)<>'0')
      AND (length(round)<20 OR round<='18446744073709551615')
      AND round<>'0'),
  player_a TEXT NOT NULL REFERENCES players(id),
  player_b TEXT NOT NULL REFERENCES players(id),
  winner TEXT REFERENCES players(id),
  ticks TEXT NOT NULL
    CHECK(typeof(ticks)='text'
      AND instr(ticks,char(0))=0
      AND length(ticks) BETWEEN 1 AND 20
      AND ticks NOT GLOB '*[^0-9]*'
      AND (ticks='0' OR substr(ticks,1,1)<>'0')
      AND (length(ticks)<20 OR ticks<='18446744073709551615')
      AND ticks<>'0'),
  score_a TEXT NOT NULL
    CHECK(typeof(score_a)='text'
      AND instr(score_a,char(0))=0
      AND length(score_a) BETWEEN 1 AND 20
      AND score_a NOT GLOB '*[^0-9]*'
      AND (score_a='0' OR substr(score_a,1,1)<>'0')
      AND (length(score_a)<20 OR score_a<='18446744073709551615')),
  score_b TEXT NOT NULL
    CHECK(typeof(score_b)='text'
      AND instr(score_b,char(0))=0
      AND length(score_b) BETWEEN 1 AND 20
      AND score_b NOT GLOB '*[^0-9]*'
      AND (score_b='0' OR substr(score_b,1,1)<>'0')
      AND (length(score_b)<20 OR score_b<='18446744073709551615')),
  lines_a INTEGER NOT NULL
    CHECK(typeof(lines_a)='integer'
      AND lines_a BETWEEN 0 AND 4294967295),
  lines_b INTEGER NOT NULL
    CHECK(typeof(lines_b)='integer'
      AND lines_b BETWEEN 0 AND 4294967295),
  CHECK(player_a<>player_b),
  CHECK(winner IS NULL OR winner=player_a OR winner=player_b)
);
)sql";
} // namespace study_meta
