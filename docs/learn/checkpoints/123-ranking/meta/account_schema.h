#pragma once
namespace study_meta {
inline constexpr const char* kAccountSchema = R"sql(
CREATE TABLE account_keys (
  player_id TEXT PRIMARY KEY NOT NULL REFERENCES players(id) ON DELETE CASCADE,
  token_hash TEXT NOT NULL UNIQUE CHECK(typeof(token_hash)='text' AND length(token_hash)=64
    AND instr(token_hash,char(0))=0 AND token_hash NOT GLOB '*[^0-9a-f]*')
);
)sql";
// No backfill: fixture IDs cannot be claimed by presenting their public ID.
} // namespace study_meta
