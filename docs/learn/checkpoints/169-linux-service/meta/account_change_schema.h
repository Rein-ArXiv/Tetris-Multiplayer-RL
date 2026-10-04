#pragma once
namespace study_meta {
inline constexpr const char* kAccountChangeColumns = R"sql(
ALTER TABLE account_keys ADD COLUMN recovery_hash TEXT
 CHECK(recovery_hash IS NULL OR (typeof(recovery_hash)='text' AND length(recovery_hash)=64
 AND instr(recovery_hash,char(0))=0 AND recovery_hash NOT GLOB '*[^0-9a-f]*'));
ALTER TABLE account_keys ADD COLUMN auth_epoch INTEGER NOT NULL DEFAULT 0
 CHECK(typeof(auth_epoch)='integer' AND auth_epoch>=0);
ALTER TABLE account_keys ADD COLUMN last_change_hash TEXT
 CHECK(last_change_hash IS NULL OR (typeof(last_change_hash)='text' AND length(last_change_hash)=64
 AND instr(last_change_hash,char(0))=0 AND last_change_hash NOT GLOB '*[^0-9a-f]*'));
)sql";
inline constexpr const char* kAccountChangeIndexes = R"sql(
CREATE UNIQUE INDEX IF NOT EXISTS account_recovery_key ON account_keys(recovery_hash) WHERE recovery_hash IS NOT NULL;
CREATE UNIQUE INDEX IF NOT EXISTS account_change_receipt ON account_keys(last_change_hash) WHERE last_change_hash IS NOT NULL;
)sql";
} // namespace study_meta
