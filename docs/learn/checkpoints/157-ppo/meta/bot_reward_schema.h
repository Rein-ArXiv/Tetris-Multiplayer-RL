#pragma once
namespace study_meta {
inline constexpr const char* kBotRewardSchema=R"sql(
CREATE TABLE bot_rewards (
  ticket TEXT PRIMARY KEY NOT NULL CHECK(length(ticket)=32 AND ticket NOT GLOB '*[^0-9a-f]*'),
  player_id TEXT NOT NULL REFERENCES players(id),
  opponent TEXT NOT NULL,
  revision INTEGER NOT NULL CHECK(typeof(revision)='integer' AND revision>0),
  day INTEGER NOT NULL CHECK(typeof(day)='integer' AND day>=0),
  awarded INTEGER NOT NULL CHECK(typeof(awarded)='integer' AND awarded>=0 AND awarded<=2147483647)
);
)sql";
inline constexpr const char* kBotRewardIndex=
    "CREATE INDEX IF NOT EXISTS bot_reward_day ON bot_rewards(player_id,day)";
} // namespace study_meta
