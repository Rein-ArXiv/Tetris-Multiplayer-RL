#include "database.h"
#include "elo.h"
#include "credentials.h"

#include "../third_party/sqlite3.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <string>

namespace meta {

namespace {

// RAII for sqlite3_stmt — 이른 return 을 안전하게 해준다.
struct StmtGuard {
    sqlite3_stmt* s = nullptr;
    ~StmtGuard() { if (s) sqlite3_finalize(s); }
};

// 현재 unix epoch (초). 트랜잭션별로 이 한 번만 호출해 "같은 tick" 의 레코드가
// 같은 created_at 을 공유하도록 한다.
int64_t now_unix()
{
    return static_cast<int64_t>(std::time(nullptr));
}

const char* kSchema = R"sql(
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous  = FULL;
PRAGMA secure_delete = ON;

CREATE TABLE IF NOT EXISTS players (
  id          INTEGER PRIMARY KEY,
  username    TEXT,
  token_hash  TEXT UNIQUE NOT NULL,
  recovery_hash TEXT,
  auth_epoch INTEGER NOT NULL DEFAULT 0,
  last_credential_op TEXT,
  elo         INTEGER NOT NULL DEFAULT 0,
  wins        INTEGER NOT NULL DEFAULT 0,
  losses      INTEGER NOT NULL DEFAULT 0,
  bp          INTEGER NOT NULL DEFAULT 0,
  xp          INTEGER NOT NULL DEFAULT 0,
  selected_icon_id TEXT NOT NULL DEFAULT 'default',
  created_at  INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS player_icons (
  player_id   INTEGER NOT NULL REFERENCES players(id) ON DELETE CASCADE,
  icon_id     TEXT NOT NULL,
  created_at  INTEGER NOT NULL,
  PRIMARY KEY(player_id, icon_id)
);

CREATE TABLE IF NOT EXISTS matches (
  id          INTEGER PRIMARY KEY,
  match_uuid  TEXT UNIQUE,
  player_a    INTEGER NOT NULL REFERENCES players(id),
  player_b    INTEGER NOT NULL REFERENCES players(id),
  winner      INTEGER          REFERENCES players(id),
  score_a     INTEGER NOT NULL,
  score_b     INTEGER NOT NULL,
  lines_a     INTEGER NOT NULL,
  lines_b     INTEGER NOT NULL,
  duration_s  INTEGER NOT NULL,
  created_at  INTEGER NOT NULL,
  elo_a_before INTEGER NOT NULL DEFAULT 0,
  elo_a_after  INTEGER NOT NULL DEFAULT 0,
  elo_b_before INTEGER NOT NULL DEFAULT 0,
  elo_b_after  INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS bot_rewards (
  ticket TEXT PRIMARY KEY,
  player_id INTEGER NOT NULL REFERENCES players(id),
  opponent_id TEXT NOT NULL,
  awarded_bp INTEGER NOT NULL,
  created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_bot_rewards_player_time ON bot_rewards(player_id,created_at);

CREATE TABLE IF NOT EXISTS elo_history (
  id          INTEGER PRIMARY KEY,
  player_id   INTEGER NOT NULL REFERENCES players(id),
  match_id    INTEGER NOT NULL REFERENCES matches(id),
  elo_before  INTEGER NOT NULL,
  elo_after   INTEGER NOT NULL,
  delta       INTEGER NOT NULL,
  created_at  INTEGER NOT NULL
);

-- SQL .dump를 새 DB에 재실행하면 user_version 헤더는 복원되지 않는다. 데이터
-- 테이블의 marker도 함께 기록해 데이터 변환 마이그레이션을 멱등하게 만든다.
CREATE TABLE IF NOT EXISTS schema_migrations (
  name        TEXT PRIMARY KEY,
  applied_at  INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_players_elo    ON players(elo DESC);
CREATE INDEX IF NOT EXISTS idx_matches_played ON matches(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_elo_pid        ON elo_history(player_id);
-- The composite ownership PK already supports the current lookup.
-- Also remove the redundant legacy index when reopening an existing DB.
DROP INDEX IF EXISTS idx_player_icons_pid;
)sql";

const char* kDefaultIconId = "default";

const IconCatalogEntry kIconCatalog[] = {
    {"default", "Default", 0,   true},
    {"ruby",    "Ruby",    100, false},
    {"gold",    "Gold",    250, false},
};

// BP(Battle Point) 적립 — 아이콘 상점의 재화. winner 가 있는(ranked 판정된)
// 매치만 적립한다 (RP 갱신 조건과 동일). 무승부/검증실패는 0.
constexpr int kBpWin  = 30;
constexpr int kBpLoss = 10;

// XP(레벨 경험치) 적립 — BP 와 같은 조건(winner 있는 매치만). 절대 감소하지
// 않는다. 레벨 곡선/최대치는 meta/levels.h.
constexpr int kXpWin  = 100;
constexpr int kXpLoss = 50;

// username 컬럼을 Player 구조체로 복사. NULL 처리.
std::optional<std::string> read_nullable_text(sqlite3_stmt* s, int col)
{
    if (sqlite3_column_type(s, col) == SQLITE_NULL) return std::nullopt;
    const unsigned char* t = sqlite3_column_text(s, col);
    if (!t) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(t));
}

const IconCatalogEntry* find_icon_def(const std::string& icon_id)
{
    for (const auto& icon : kIconCatalog) {
        if (icon.id == icon_id) return &icon;
    }
    return nullptr;
}

// SQLite INTEGER is 64-bit. Validate its storage type and range before narrowing.
bool read_nonnegative_int(sqlite3_stmt* statement, int column, int& out) {
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) return false;
    const auto value = sqlite3_column_int64(statement, column);
    if (value < 0 || value > 2147483647) return false;
    out = static_cast<int>(value);
    return true;
}

std::optional<Player> read_player(sqlite3_stmt *statement) {
    if (sqlite3_step(statement) != SQLITE_ROW)
        return std::nullopt;
    Player p;
    p.id = sqlite3_column_int64(statement, 0);
    p.username = read_nullable_text(statement, 1);
    if (!read_nonnegative_int(statement, 2, p.elo) ||
        !read_nonnegative_int(statement, 3, p.wins) ||
        !read_nonnegative_int(statement, 4, p.losses) ||
        !read_nonnegative_int(statement, 5, p.bp) ||
        !read_nonnegative_int(statement, 6, p.xp)) return std::nullopt;
    auto icon = sqlite3_column_text(statement, 7);
    p.selected_icon_id = icon ? reinterpret_cast<const char *>(icon) : kDefaultIconId;
    if (!find_icon_def(p.selected_icon_id))
        p.selected_icon_id = kDefaultIconId;
    p.auth_epoch = sqlite3_column_int64(statement, 8);
    return p;
}
const char *kPlayerColumns =
    "SELECT id,username,elo,wins,losses,bp,xp,selected_icon_id,auth_epoch FROM players ";
std::optional<Player> read_player_by_token(sqlite3 *db, const std::string &token) {
    if (!credentials::account(token))
        return std::nullopt;
    const auto hash = credentials::digest("account", token);
    StmtGuard g;
    const auto sql = std::string(kPlayerColumns) + "WHERE token_hash=?1";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &g.s, nullptr) != SQLITE_OK)
        return std::nullopt;
    sqlite3_bind_text(g.s, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
    return read_player(g.s);
}

// Shop authorization and mutation share one write transaction across DB connections.
class ShopTransaction {
public:
    explicit ShopTransaction(sqlite3* db) : db_(db) {
        active_ = sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) == SQLITE_OK;
    }
    ~ShopTransaction() {
        if (active_) sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    ShopTransaction(const ShopTransaction&) = delete;
    ShopTransaction& operator=(const ShopTransaction&) = delete;
    bool started() const { return active_; }
    bool commit() {
        if (!active_ || sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) return false;
        active_ = false;
        return true;
    }
private:
    sqlite3* db_;
    bool active_ = false;
};

std::optional<bool> player_owns_icon(sqlite3 *db, int64_t player_id, const std::string &icon_id) {
    const IconCatalogEntry* def = find_icon_def(icon_id);
    if (!def) return false;
    if (def->default_owned) return true;
    StmtGuard g;
    const char* sql =
        "SELECT 1 FROM player_icons WHERE player_id=?1 AND icon_id=?2";
    if (sqlite3_prepare_v2(db, sql, -1, &g.s, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_int64(g.s, 1, player_id);
    sqlite3_bind_text (g.s, 2, icon_id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(g.s);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    return std::nullopt;
}

bool insert_icon_ownership(sqlite3* db, int64_t player_id, const std::string& icon_id)
{
    StmtGuard g;
    const char* sql =
        "INSERT INTO player_icons(player_id,icon_id,created_at)"
        " VALUES(?1,?2,?3)";
    if (sqlite3_prepare_v2(db, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(g.s, 1, player_id);
    sqlite3_bind_text (g.s, 2, icon_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(g.s, 3, now_unix());
    return sqlite3_step(g.s) == SQLITE_DONE && sqlite3_changes(db) == 1;
}

} // namespace

// -----------------------------------------------------------------------------
Database::Database(const std::string& path)
{
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK || !db_) {
        std::string msg = "sqlite3_open failed: ";
        if (db_) msg += sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error(msg);
    }
    // 트랜잭션 밖에서 5초까지 락 대기 (동시 요청 스레드 있을 수 있음).
    sqlite3_busy_timeout(db_, 5000);
    try { execSchema(); }
    catch (...) { sqlite3_close(db_); db_=nullptr; throw; }
}

Database::~Database()
{
    if (db_) sqlite3_close(db_);
}

void Database::execSchema()
{
    char* err = nullptr;
    int rc = sqlite3_exec(db_, kSchema, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = "schema init failed: ";
        if (err) { msg += err; sqlite3_free(err); }
        throw std::runtime_error(msg);
    }

    // 기존 tetris.db 를 보존하면서 신규 컬럼을 붙인다. duplicate column은
    // 같은 이름의 열이 존재한다는 뜻이며, 열 정의 전체의 일치 검사는 아니다.
    auto alter_if_needed = [&](const char* sql) {
        char* alterErr = nullptr;
        int alterRc = sqlite3_exec(db_, sql, nullptr, nullptr, &alterErr);
        if (alterRc == SQLITE_OK) return;
        std::string msg = alterErr ? alterErr : "";
        sqlite3_free(alterErr);
        if (msg.find("duplicate column name") != std::string::npos) return;
        throw std::runtime_error("schema migration failed: " + msg);
    };
    alter_if_needed("ALTER TABLE players ADD COLUMN bp INTEGER NOT NULL DEFAULT 0");
    alter_if_needed("ALTER TABLE players ADD COLUMN selected_icon_id TEXT NOT NULL DEFAULT 'default'");
    alter_if_needed("ALTER TABLE players ADD COLUMN xp INTEGER NOT NULL DEFAULT 0");
    alter_if_needed("ALTER TABLE players ADD COLUMN recovery_hash TEXT");
    alter_if_needed("ALTER TABLE players ADD COLUMN auth_epoch INTEGER NOT NULL DEFAULT 0");
    alter_if_needed("ALTER TABLE players ADD COLUMN last_credential_op TEXT");
    alter_if_needed("ALTER TABLE matches ADD COLUMN match_uuid TEXT");
    alter_if_needed("ALTER TABLE matches ADD COLUMN elo_a_before INTEGER NOT NULL DEFAULT 0");
    alter_if_needed("ALTER TABLE matches ADD COLUMN elo_a_after INTEGER NOT NULL DEFAULT 0");
    alter_if_needed("ALTER TABLE matches ADD COLUMN elo_b_before INTEGER NOT NULL DEFAULT 0");
    alter_if_needed("ALTER TABLE matches ADD COLUMN elo_b_after INTEGER NOT NULL DEFAULT 0");
    // Old tables receive this column through ALTER before the index is created.
    {
        char* indexErr = nullptr;
        const int indexRc = sqlite3_exec(
            db_,
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_matches_uuid ON matches(match_uuid) "
            "WHERE match_uuid IS NOT NULL",
            nullptr, nullptr, &indexErr);
        if (indexRc != SQLITE_OK) {
            std::string msg = indexErr ? indexErr : "";
            sqlite3_free(indexErr);
            throw std::runtime_error("schema index migration failed: " + msg);
        }
    }

    // ── 1회성 스케일 마이그레이션 (user_version 0 → 1) ─────────────────────
    //   구 ELO 스케일(1200 시작)을 RP 스케일(0 시작/0 바닥)로 이관:
    //   elo := max(0, elo - 1200). 신규 DB 는 이 시점에 players 가 비어 있어
    //   no-op 이고, 버전만 1 로 올라간다. (meta/elo.h 참조)
    int userVersion = 0;
    {
        StmtGuard version;
        if (sqlite3_prepare_v2(db_, "PRAGMA user_version", -1, &version.s, nullptr) != SQLITE_OK)
            throw std::runtime_error("schema migration version prepare failed");
        if (sqlite3_step(version.s) != SQLITE_ROW)
            throw std::runtime_error("schema migration version read failed");
        userVersion = sqlite3_column_int(version.s, 0);
        if (userVersion < 0)
            throw std::runtime_error("schema migration version invalid");
    }
    bool rpRebaseApplied = false;
    {
        StmtGuard g;
        if (sqlite3_prepare_v2(db_,
                "SELECT 1 FROM schema_migrations WHERE name='elo_to_rp_v1'",
                -1, &g.s, nullptr) != SQLITE_OK)
            throw std::runtime_error("schema migration marker prepare failed");
        const int markerRc = sqlite3_step(g.s);
        if (markerRc != SQLITE_ROW && markerRc != SQLITE_DONE)
            throw std::runtime_error("schema migration marker read failed");
        rpRebaseApplied = markerRc == SQLITE_ROW;
    }

    if (!rpRebaseApplied) {
        char* mErr = nullptr;
        // user_version>=1 인 기존 DB는 구 구현에서 이미 리베이스됐다. 이 경우
        // 데이터는 다시 건드리지 않고 dump에 보존될 marker만 백필한다.
        const char* migrationSql = userVersion < 1
            ? "BEGIN IMMEDIATE;"
              "UPDATE players SET elo = MAX(0, elo - 1200);"
              "UPDATE elo_history SET "
                "elo_before = MAX(0, elo_before - 1200),"
                "elo_after  = MAX(0, elo_after  - 1200),"
                "delta = MAX(0, elo_after - 1200) - MAX(0, elo_before - 1200);"
              "INSERT INTO schema_migrations(name,applied_at) "
                "VALUES('elo_to_rp_v1',strftime('%s','now'));"
              "PRAGMA user_version = 1;"
              "COMMIT;"
            : "BEGIN IMMEDIATE;"
              "INSERT INTO schema_migrations(name,applied_at) "
                "VALUES('elo_to_rp_v1',strftime('%s','now'));"
              "COMMIT;";
        if (sqlite3_exec(db_, migrationSql,
                nullptr, nullptr, &mErr) != SQLITE_OK) {
            std::string msg = mErr ? mErr : "";
            sqlite3_free(mErr);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw std::runtime_error("elo->rp rebase migration failed: " + msg);
        }
    }
    migrateCredentials();
}

// -----------------------------------------------------------------------------
std::optional<Player>
Database::registerGuest(const std::string& token)
{
    std::lock_guard<std::mutex> lk(mu_);

    if(!credentials::account(token))return std::nullopt;
    const auto hash=credentials::digest("account",token);
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
        return std::nullopt;
    struct RegistrationTransaction {
        sqlite3* db;
        bool committed = false;
        ~RegistrationTransaction() {
            if (!committed) sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
    } transaction{db_};
    StmtGuard g;
    const char* sql =
        "INSERT INTO players(username,token_hash,elo,wins,losses,created_at) "
        "VALUES(NULL,?1,0,0,0,?2)";
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[db] registerGuest prepare: %s\n", sqlite3_errmsg(db_));
        return std::nullopt;
    }
    sqlite3_bind_text (g.s, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(g.s, 2, now_unix());

    int rc = sqlite3_step(g.s);
    if (rc != SQLITE_DONE) {
        // UNIQUE 충돌 등 — 호출자가 새 token 으로 재시도할 수 있도록.
        std::fprintf(stderr, "[db] registerGuest step: rc=%d %s\n",
                     rc, sqlite3_errmsg(db_));
        return std::nullopt;
    }

    Player p;
    p.id      = sqlite3_last_insert_rowid(db_);
    p.elo     = 0;
    p.wins    = 0;
    p.losses  = 0;
    p.bp      = 0;
    p.xp      = 0;
    p.selected_icon_id = kDefaultIconId;
    // username 은 기본 NULL
    if (!insert_icon_ownership(db_, p.id, kDefaultIconId)) {
        // 계정과 기본 소유권은 하나의 등록이다. 부분 성공을 반환하지 않는다.
        std::fprintf(stderr, "[db] registerGuest: default icon ownership insert "
                     "failed for player_id=%lld\n", static_cast<long long>(p.id));
        return std::nullopt;
    }
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
        return std::nullopt;
    transaction.committed = true;
    return p;
}

std::optional<Player>
Database::getByToken(const std::string& token)
{
    std::lock_guard<std::mutex> lk(mu_);
    return read_player_by_token(db_, token);
}

std::vector<IconCatalogEntry>
Database::iconCatalog() const
{
    std::vector<IconCatalogEntry> out;
    for (const auto& icon : kIconCatalog) out.push_back(icon);
    return out;
}

IconPurchaseResult
Database::purchaseIcon(const std::string& token,
                       const std::string& icon_id,
                       std::optional<Player>& out_player)
{
    out_player.reset();
    std::lock_guard<std::mutex> lk(mu_);
    const IconCatalogEntry* icon = find_icon_def(icon_id);
    if (!icon) return IconPurchaseResult::InvalidIcon;
    ShopTransaction transaction(db_);
    if (!transaction.started()) return IconPurchaseResult::DbError;
    auto p = read_player_by_token(db_, token);
    if (!p) return IconPurchaseResult::UnknownToken;
    const auto owned = player_owns_icon(db_, p->id, icon_id);
    if (!owned) return IconPurchaseResult::DbError;
    if (*owned) return IconPurchaseResult::AlreadyOwned;
    if (p->bp < icon->price_bp) return IconPurchaseResult::InsufficientBp;
    {
        StmtGuard g;
        const char* sql = "UPDATE players SET bp=bp-?1 WHERE id=?2 AND typeof(bp)='integer' AND bp>=?1";
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK)
            return IconPurchaseResult::DbError;
        sqlite3_bind_int(g.s, 1, icon->price_bp);
        sqlite3_bind_int64(g.s, 2, p->id);
        if (sqlite3_step(g.s) != SQLITE_DONE || sqlite3_changes(db_) != 1)
            return IconPurchaseResult::DbError;
    }
    if (!insert_icon_ownership(db_, p->id, icon_id)) return IconPurchaseResult::DbError;
    // Read the response before COMMIT, while the authenticated identity is stable.
    auto after = read_player_by_token(db_, token);
    if (!after || !transaction.commit()) return IconPurchaseResult::DbError;
    out_player = std::move(after);
    return IconPurchaseResult::Ok;
}

IconSelectResult
Database::selectIcon(const std::string& token,
                     const std::string& icon_id,
                     std::optional<Player>& out_player)
{
    out_player.reset();
    std::lock_guard<std::mutex> lk(mu_);
    if (!find_icon_def(icon_id)) return IconSelectResult::InvalidIcon;
    ShopTransaction transaction(db_);
    if (!transaction.started()) return IconSelectResult::DbError;
    auto p = read_player_by_token(db_, token);
    if (!p) return IconSelectResult::UnknownToken;
    const auto owned = player_owns_icon(db_, p->id, icon_id);
    if (!owned) return IconSelectResult::DbError;
    if (!*owned) return IconSelectResult::NotOwned;
    {
        StmtGuard g;
        const char* sql = "UPDATE players SET selected_icon_id=?1 WHERE id=?2";
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK)
            return IconSelectResult::DbError;
        sqlite3_bind_text(g.s, 1, icon_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(g.s, 2, p->id);
        if (sqlite3_step(g.s) != SQLITE_DONE || sqlite3_changes(db_) != 1)
            return IconSelectResult::DbError;
    }
    auto after = read_player_by_token(db_, token);
    if (!after || !transaction.commit()) return IconSelectResult::DbError;
    out_player = std::move(after);
    return IconSelectResult::Ok;
}

// -----------------------------------------------------------------------------
std::optional<MatchInsertResult>
Database::saveMatch(const MatchRecord& m, MatchSaveError* error)
{
    if (error) *error = MatchSaveError::Database;
    std::lock_guard<std::mutex> lk(mu_);

    // 쓰기 트랜잭션을 먼저 확보한다. BEGIN/COMMIT의 잠금 대기는 여전히 실패할 수 있다.
    char* err = nullptr;
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "[db] BEGIN: %s\n", err ? err : "?");
        sqlite3_free(err);
        return std::nullopt;
    }

    struct MatchTransaction {
        sqlite3* db;
        bool committed = false;
        ~MatchTransaction() {
            if (!committed) sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
    } transaction{db_};

    // Inspect the key while holding the write transaction, across connections too.
    // Returning a saved result rolls back only this read-only transaction.
    {
        StmtGuard g;
        const char* sql =
            "SELECT id,elo_a_before,elo_a_after,elo_b_before,elo_b_after,"
            "player_a,player_b,winner,score_a,score_b,lines_a,lines_b,duration_s "
            "FROM matches WHERE match_uuid=?1";
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return std::nullopt;
        sqlite3_bind_text(g.s, 1, m.match_uuid.c_str(), -1, SQLITE_TRANSIENT);
        const int lookupRc = sqlite3_step(g.s);
        if (lookupRc != SQLITE_ROW && lookupRc != SQLITE_DONE) return std::nullopt;
        if (lookupRc == SQLITE_ROW) {
            MatchInsertResult r;
            r.match_id = sqlite3_column_int64(g.s, 0);
            int ab, aa, bb, ba;
            if (!read_nonnegative_int(g.s, 1, ab) || !read_nonnegative_int(g.s, 2, aa) ||
                !read_nonnegative_int(g.s, 3, bb) || !read_nonnegative_int(g.s, 4, ba))
                return std::nullopt;
            // A retry is the same operation only when every persisted input
            // agrees. A reused key must not confirm somebody else's result.
            const bool stored_draw = sqlite3_column_type(g.s, 7) == SQLITE_NULL;
            const bool same_winner = stored_draw ? !m.winner
                : m.winner && sqlite3_column_int64(g.s, 7) == *m.winner;
            if (sqlite3_column_int64(g.s, 5) != m.player_a ||
                sqlite3_column_int64(g.s, 6) != m.player_b || !same_winner ||
                sqlite3_column_int(g.s, 8) != m.score_a ||
                sqlite3_column_int(g.s, 9) != m.score_b ||
                sqlite3_column_int(g.s, 10) != m.lines_a ||
                sqlite3_column_int(g.s, 11) != m.lines_b ||
                sqlite3_column_int(g.s, 12) != m.duration_s) {
                if (error) *error = MatchSaveError::IdentityConflict;
                return std::nullopt;
            }
            if (error) *error = MatchSaveError::None;
            r.a = {ab, aa, aa - ab};
            r.b = {bb, ba, ba - bb};
            return r;
        }
    }

    auto rollback = [&](const char* why) -> std::optional<MatchInsertResult> {
        std::fprintf(stderr, "[db] saveMatch rollback: %s (%s)\n",
                     why, sqlite3_errmsg(db_));
        return std::nullopt;
    };

    const int64_t ts = now_unix();

    // 1) INSERT matches
    int64_t match_id = 0;
    {
        StmtGuard g;
        const char* sql =
            "INSERT INTO matches"
            "(match_uuid,player_a,player_b,winner,score_a,score_b,lines_a,lines_b,duration_s,created_at)"
            " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)";
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK)
            return rollback("matches prepare");
        sqlite3_bind_text (g.s, 1, m.match_uuid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(g.s, 2, m.player_a);
        sqlite3_bind_int64(g.s, 3, m.player_b);
        if (m.winner) sqlite3_bind_int64(g.s, 4, *m.winner);
        else          sqlite3_bind_null (g.s, 4);
        sqlite3_bind_int  (g.s, 5, m.score_a);
        sqlite3_bind_int  (g.s, 6, m.score_b);
        sqlite3_bind_int  (g.s, 7, m.lines_a);
        sqlite3_bind_int  (g.s, 8, m.lines_b);
        sqlite3_bind_int  (g.s, 9, m.duration_s);
        sqlite3_bind_int64(g.s, 10, ts);
        if (sqlite3_step(g.s) != SQLITE_DONE) return rollback("matches step");
        match_id = sqlite3_last_insert_rowid(db_);
    }

    // 2) RP 읽기 + 계산(내부 컬럼/함수명 elo 는 하위 호환상 유지)
    auto get_elo = [&](int64_t pid, int& out) -> bool {
        StmtGuard g;
        if (sqlite3_prepare_v2(db_, "SELECT elo FROM players WHERE id=?1", -1,
                               &g.s, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int64(g.s, 1, pid);
        int rc = sqlite3_step(g.s);
        if (rc != SQLITE_ROW) return false;
        return read_nonnegative_int(g.s, 0, out);
    };

    int elo_a_before = 0, elo_b_before = 0;
    if (!get_elo(m.player_a, elo_a_before)) return rollback("select elo_a");
    if (!get_elo(m.player_b, elo_b_before)) return rollback("select elo_b");

    int elo_a_after = elo_a_before;
    int elo_b_after = elo_b_before;
    bool a_won = (m.winner.has_value() && *m.winner == m.player_a);
    bool b_won = (m.winner.has_value() && *m.winner == m.player_b);

    if (m.winner) {
        if (a_won) {
            auto u = elo::update(elo_a_before, elo_b_before);
            elo_a_after = u.new_winner;
            elo_b_after = u.new_loser;
        } else if (b_won) {
            auto u = elo::update(elo_b_before, elo_a_before);
            elo_b_after = u.new_winner;
            elo_a_after = u.new_loser;
        }
    }

    // Draws also need an exact response for idempotent retries.
    {
        StmtGuard g;
        const char* sql =
            "UPDATE matches SET elo_a_before=?1,elo_a_after=?2,"
            "elo_b_before=?3,elo_b_after=?4 WHERE id=?5";
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK)
            return rollback("match result prepare");
        sqlite3_bind_int(g.s, 1, elo_a_before);
        sqlite3_bind_int(g.s, 2, elo_a_after);
        sqlite3_bind_int(g.s, 3, elo_b_before);
        sqlite3_bind_int(g.s, 4, elo_b_after);
        sqlite3_bind_int64(g.s, 5, match_id);
        if (sqlite3_step(g.s) != SQLITE_DONE) return rollback("match result step");
    }

    // 3) UPDATE players (winner 가 있을 때만 elo + wins/losses + bp 갱신).
    if (m.winner) {
        auto update_player = [&](int64_t pid, int new_elo, bool won) -> bool {
            StmtGuard g;
            const char* sql = won
                ? "UPDATE players SET elo=?1, wins=wins+1, bp=bp+?3, xp=xp+?4 WHERE id=?2 "
                  "AND typeof(wins)='integer' AND wins BETWEEN 0 AND 2147483646 "
                  "AND typeof(bp)='integer' AND bp BETWEEN 0 AND 2147483647-?3 "
                  "AND typeof(xp)='integer' AND xp BETWEEN 0 AND 2147483647-?4"
                : "UPDATE players SET elo=?1, losses=losses+1, bp=bp+?3, xp=xp+?4 WHERE id=?2 "
                  "AND typeof(losses)='integer' AND losses BETWEEN 0 AND 2147483646 "
                  "AND typeof(bp)='integer' AND bp BETWEEN 0 AND 2147483647-?3 "
                  "AND typeof(xp)='integer' AND xp BETWEEN 0 AND 2147483647-?4";
            if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_int  (g.s, 1, new_elo);
            sqlite3_bind_int64(g.s, 2, pid);
            sqlite3_bind_int  (g.s, 3, won ? kBpWin : kBpLoss);
            sqlite3_bind_int  (g.s, 4, won ? kXpWin : kXpLoss);
            return sqlite3_step(g.s) == SQLITE_DONE && sqlite3_changes(db_) == 1;
        };
        if (!update_player(m.player_a, elo_a_after, a_won)) return rollback("update player_a");
        if (!update_player(m.player_b, elo_b_after, b_won)) return rollback("update player_b");

        // 4) elo_history 양쪽
        auto insert_history = [&](int64_t pid, int before, int after) -> bool {
            StmtGuard g;
            const char* sql =
                "INSERT INTO elo_history"
                "(player_id,match_id,elo_before,elo_after,delta,created_at)"
                " VALUES(?1,?2,?3,?4,?5,?6)";
            if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_int64(g.s, 1, pid);
            sqlite3_bind_int64(g.s, 2, match_id);
            sqlite3_bind_int  (g.s, 3, before);
            sqlite3_bind_int  (g.s, 4, after);
            sqlite3_bind_int  (g.s, 5, after - before);
            sqlite3_bind_int64(g.s, 6, ts);
            return sqlite3_step(g.s) == SQLITE_DONE;
        };
        if (!insert_history(m.player_a, elo_a_before, elo_a_after)) return rollback("history a");
        if (!insert_history(m.player_b, elo_b_before, elo_b_after)) return rollback("history b");
    }

    // 5) COMMIT
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "[db] COMMIT: %s\n", err ? err : "?");
        sqlite3_free(err);
        return std::nullopt;
    }

    transaction.committed = true;
    MatchInsertResult r;
    r.match_id = match_id;
    r.a = { elo_a_before, elo_a_after, elo_a_after - elo_a_before };
    r.b = { elo_b_before, elo_b_after, elo_b_after - elo_b_before };
    if (error) *error = MatchSaveError::None;
    return r;
}

// -----------------------------------------------------------------------------
std::optional<std::vector<LeaderRow>>
Database::leaderboard(int limit)
{
    std::lock_guard<std::mutex> lk(mu_);

    limit = std::clamp(limit, 1, 100);

    StmtGuard g;
    const char* sql =
        "SELECT id,username,elo,wins,losses,xp FROM players "
        "ORDER BY elo DESC, id ASC LIMIT ?1";
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[db] leaderboard prepare: %s\n", sqlite3_errmsg(db_));
        return {};
    }
    if (sqlite3_bind_int(g.s, 1, limit) != SQLITE_OK) return std::nullopt;

    std::vector<LeaderRow> rows;
    int step = SQLITE_OK;
    while ((step = sqlite3_step(g.s)) == SQLITE_ROW) {
        LeaderRow r;
        r.player_id = sqlite3_column_int64(g.s, 0);
        if (r.player_id <= 0) return std::nullopt;
        r.username  = read_nullable_text(g.s, 1);
        if (!read_nonnegative_int(g.s, 2, r.elo) ||
            !read_nonnegative_int(g.s, 3, r.wins) ||
            !read_nonnegative_int(g.s, 4, r.losses) ||
            !read_nonnegative_int(g.s, 5, r.xp)) return {};
        rows.push_back(std::move(r));
    }
    if (step != SQLITE_DONE) return std::nullopt;
    return rows;
}


std::optional<int> Database::botReward(int64_t player, const std::string& ticket) {
    std::lock_guard<std::mutex> lock(mu_);
    StmtGuard q;
    if(sqlite3_prepare_v2(db_,"SELECT awarded_bp FROM bot_rewards WHERE ticket=?1 AND player_id=?2",-1,&q.s,nullptr)!=SQLITE_OK)return std::nullopt;
    sqlite3_bind_text(q.s,1,ticket.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(q.s,2,player);
    if(sqlite3_step(q.s)!=SQLITE_ROW)return std::nullopt;
    return sqlite3_column_int(q.s,0);
}

std::optional<int> Database::saveBotWin(int64_t player,const std::string& ticket,const std::string& opponent) {
    std::lock_guard<std::mutex> lock(mu_);
    if(sqlite3_exec(db_,"BEGIN IMMEDIATE",nullptr,nullptr,nullptr)!=SQLITE_OK)return std::nullopt;
    struct Transaction {
        sqlite3* db; bool committed=false;
        ~Transaction(){if(!committed)sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);}
    } tx{db_};
    {
        StmtGuard q;
        if(sqlite3_prepare_v2(db_,"SELECT player_id,awarded_bp FROM bot_rewards WHERE ticket=?1",-1,&q.s,nullptr)!=SQLITE_OK)return std::nullopt;
        sqlite3_bind_text(q.s,1,ticket.c_str(),-1,SQLITE_TRANSIENT);
        if(sqlite3_step(q.s)==SQLITE_ROW) {
            if(sqlite3_column_int64(q.s,0)!=player)return std::nullopt;
            return sqlite3_column_int(q.s,1); // RAII rolls back this read-only transaction.
        }
    }
    const int64_t now=now_unix(), day=now/86400*86400;
    int earned=0;
    {
        StmtGuard q;
        if(sqlite3_prepare_v2(db_,"SELECT COALESCE(SUM(awarded_bp),0) FROM bot_rewards WHERE player_id=?1 AND created_at>=?2",-1,&q.s,nullptr)!=SQLITE_OK)return std::nullopt;
        sqlite3_bind_int64(q.s,1,player);sqlite3_bind_int64(q.s,2,day);
        if(sqlite3_step(q.s)!=SQLITE_ROW)return std::nullopt;
        earned=static_cast<int>(std::clamp<int64_t>(100-sqlite3_column_int64(q.s,0),0,10));
    }
    {
        StmtGuard q;
        if(sqlite3_prepare_v2(db_,"INSERT INTO bot_rewards VALUES (?1,?2,?3,?4,?5)",-1,&q.s,nullptr)!=SQLITE_OK)return std::nullopt;
        sqlite3_bind_text(q.s,1,ticket.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int64(q.s,2,player);
        sqlite3_bind_text(q.s,3,opponent.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int(q.s,4,earned);sqlite3_bind_int64(q.s,5,now);
        if(sqlite3_step(q.s)!=SQLITE_DONE)return std::nullopt;
    }
    {
        StmtGuard q;
        if(sqlite3_prepare_v2(db_,"UPDATE players SET bp=bp+?1 WHERE id=?2 AND bp<=2147483647-?1",-1,&q.s,nullptr)!=SQLITE_OK)return std::nullopt;
        sqlite3_bind_int(q.s,1,earned);sqlite3_bind_int64(q.s,2,player);
        if(sqlite3_step(q.s)!=SQLITE_DONE || sqlite3_changes(db_)!=1)return std::nullopt;
    }
    if(sqlite3_exec(db_,"COMMIT",nullptr,nullptr,nullptr)!=SQLITE_OK)return std::nullopt;
    tx.committed=true;return earned;
}

} // namespace meta

namespace meta {
void Database::migrateCredentials() {
    auto exec = [&](const char *sql) {
        if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
            throw std::runtime_error("credential migration failed (close other DB readers and retry)");
    };
    bool legacy = false;
    {
        StmtGuard columns;
        if (sqlite3_prepare_v2(db_, "PRAGMA table_info(players)", -1, &columns.s, nullptr) != SQLITE_OK)
            throw std::runtime_error("credential schema inspection failed");
        int columnRc;
        while ((columnRc = sqlite3_step(columns.s)) == SQLITE_ROW) {
            const auto name = reinterpret_cast<const char *>(sqlite3_column_text(columns.s, 1));
            if (name && std::string(name) == "token")
                legacy = true;
        }
        if (columnRc != SQLITE_DONE)
            throw std::runtime_error("credential schema inspection read failed");
    }
    exec("BEGIN IMMEDIATE");
    try {
        if (legacy) {
            exec("ALTER TABLE players RENAME COLUMN token TO token_hash");
            StmtGuard update;
            if (sqlite3_prepare_v2(db_, "UPDATE players SET token_hash=?1 WHERE id=?2", -1,
                                   &update.s, nullptr) != SQLITE_OK)
                throw std::runtime_error("credential migration prepare failed");
            std::optional<sqlite3_int64> last;
            for (;;) {
                sqlite3_int64 id;
                std::string token;
                {
                    // Finish each SELECT before mutating its table. Keyset progression
                    // uses an unchanged primary key and does not overflow at the last ID.
                    StmtGuard row;
                    const char *sql = last
                        ? "SELECT id,token_hash FROM players WHERE id>?1 ORDER BY id LIMIT 1"
                        : "SELECT id,token_hash FROM players ORDER BY id LIMIT 1";
                    if (sqlite3_prepare_v2(db_, sql, -1, &row.s, nullptr) != SQLITE_OK ||
                        (last && sqlite3_bind_int64(row.s, 1, *last) != SQLITE_OK))
                        throw std::runtime_error("credential migration prepare failed");
                    const int rc = sqlite3_step(row.s);
                    if (rc == SQLITE_DONE) break;
                    if (rc != SQLITE_ROW)
                        throw std::runtime_error("credential migration read failed");
                    if (sqlite3_column_type(row.s, 1) != SQLITE_TEXT)
                        throw std::runtime_error("invalid legacy credential; migration rolled back");
                    const auto raw = sqlite3_column_text(row.s, 1);
                    if (!raw)
                        throw std::runtime_error("credential migration text read failed");
                    const int bytes = sqlite3_column_bytes(row.s, 1);
                    token.assign(reinterpret_cast<const char *>(raw),
                                 static_cast<std::size_t>(bytes));
                    id = sqlite3_column_int64(row.s, 0);
                }
                if (!credentials::account(token))
                    throw std::runtime_error("invalid legacy credential; migration rolled back");
                const auto hash = credentials::digest("account", token);
                if (sqlite3_reset(update.s) != SQLITE_OK ||
                    sqlite3_clear_bindings(update.s) != SQLITE_OK ||
                    sqlite3_bind_text(update.s, 1, hash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
                    sqlite3_bind_int64(update.s, 2, id) != SQLITE_OK ||
                    sqlite3_step(update.s) != SQLITE_DONE || sqlite3_changes(db_) != 1)
                    throw std::runtime_error("credential migration update failed");
                last = id;
            }
        }
        exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_players_recovery ON players(recovery_hash) WHERE "
             "recovery_hash IS NOT NULL");
        exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_players_credential_op ON players(last_credential_op) "
             "WHERE last_credential_op IS NOT NULL");
        exec("INSERT OR IGNORE INTO schema_migrations(name,applied_at) "
             "VALUES('credential_hash_v1',strftime('%s','now'))");
        exec("COMMIT");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }

    bool scrubbed = false;
    {
        StmtGuard marker;
        if (sqlite3_prepare_v2(db_, "SELECT 1 FROM schema_migrations WHERE name='credential_scrub_v1'", -1,
                               &marker.s, nullptr) != SQLITE_OK)
            throw std::runtime_error("credential scrub marker unavailable");
        const int markerRc = sqlite3_step(marker.s);
        if (markerRc != SQLITE_ROW && markerRc != SQLITE_DONE)
            throw std::runtime_error("credential scrub marker read failed");
        scrubbed = markerRc == SQLITE_ROW;
    }
    if (!scrubbed) {
        auto checkpoint = [&] {
            StmtGuard g;
            if (sqlite3_prepare_v2(db_, "PRAGMA wal_checkpoint(TRUNCATE)", -1, &g.s, nullptr) != SQLITE_OK ||
                sqlite3_step(g.s) != SQLITE_ROW || sqlite3_column_int(g.s, 0) != 0)
                throw std::runtime_error("credential WAL scrub busy; close other DB readers and restart");
        };
        // Logical migration is durable first. A second marker makes an interrupted
        // physical scrub retry on restart. This does not erase external backups/SSD history.
        checkpoint();
        exec("VACUUM");
        checkpoint();
        exec("INSERT INTO schema_migrations(name,applied_at) "
             "VALUES('credential_scrub_v1',strftime('%s','now'))");
    }
}

std::optional<Player> Database::getByEpoch(int64_t player_id, int64_t epoch) {
    std::lock_guard<std::mutex> lock(mu_);
    StmtGuard g;
    const auto sql = std::string(kPlayerColumns) + "WHERE id=?1 AND auth_epoch=?2";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &g.s, nullptr) != SQLITE_OK)
        return std::nullopt;
    sqlite3_bind_int64(g.s, 1, player_id);
    sqlite3_bind_int64(g.s, 2, epoch);
    return read_player(g.s);
}

AccountChangeResult Database::changeAccount(const std::string &operation, const std::string &current,
                                            const std::string &next_token, const std::string &next_recovery,
                                            std::optional<Player> &out_player) {
    out_player.reset();
    const bool recovering = operation == "recover", backup = operation == "backup";
    if ((!recovering && !backup && operation != "rotate") ||
        !(recovering ? credentials::recovery(current) : credentials::account(current)) ||
        !credentials::account(next_token) || !credentials::recovery(next_recovery) ||
        (backup && current != next_token) || (operation == "rotate" && current == next_token) ||
        (recovering && current == next_recovery))
        return AccountChangeResult::InvalidRequest;
    const auto oldHash = credentials::digest(recovering ? "recovery" : "account", current);
    const auto nextHash = credentials::digest("account", next_token);
    const auto recoveryHash = credentials::digest("recovery", next_recovery);
    const auto receipt =
        credentials::digest("operation", operation + ":" + current + ":" + next_token + ":" + next_recovery);
    std::lock_guard<std::mutex> lock(mu_);
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK)
        return AccountChangeResult::DbError;
    struct AccountTransaction {
        sqlite3* db;
        std::optional<Player>& output;
        bool committed = false;
        ~AccountTransaction() {
            if (!committed) {
                sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
                output.reset();
            }
        }
    } transaction{db_, out_player};
    auto rollback = [&](AccountChangeResult result) {
        // The guard also rolls back C++ exceptions; output is published only on commit.
        out_player.reset();
        return result;
    };
    {
        // Retry requires every original secret and both replacements; old credentials
        // alone never authenticate. Only the latest receipt per player is retained.
        StmtGuard prior;
        if (sqlite3_prepare_v2(
                db_,
                "SELECT 1 FROM players WHERE last_credential_op=?1 AND token_hash=?2 AND recovery_hash=?3",
                -1, &prior.s, nullptr) != SQLITE_OK)
            return rollback(AccountChangeResult::DbError);
        if (sqlite3_bind_text(prior.s, 1, receipt.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(prior.s, 2, nextHash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(prior.s, 3, recoveryHash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
            return rollback(AccountChangeResult::DbError);
        const int prior_rc = sqlite3_step(prior.s);
        if (prior_rc != SQLITE_ROW && prior_rc != SQLITE_DONE)
            return rollback(AccountChangeResult::DbError);
        if (prior_rc == SQLITE_ROW) {
            out_player = read_player_by_token(db_, next_token);
            if (!out_player || sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
                return rollback(AccountChangeResult::DbError);
            transaction.committed = true;
            return AccountChangeResult::Ok;
        }
    }
    int64_t player_id = 0;
    {
        StmtGuard owner;
        const char *sql = recovering ? "SELECT id,token_hash,recovery_hash FROM players WHERE recovery_hash=?1"
                                     : "SELECT id,token_hash,recovery_hash FROM players WHERE token_hash=?1";
        if (sqlite3_prepare_v2(db_, sql, -1, &owner.s, nullptr) != SQLITE_OK)
            return rollback(AccountChangeResult::DbError);
        if (sqlite3_bind_text(owner.s, 1, oldHash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
            return rollback(AccountChangeResult::DbError);
        const int rc = sqlite3_step(owner.s);
        if (rc == SQLITE_DONE)
            return rollback(AccountChangeResult::InvalidCredential);
        if (rc != SQLITE_ROW)
            return rollback(AccountChangeResult::DbError);
        player_id = sqlite3_column_int64(owner.s, 0);
        const auto current_token = read_nullable_text(owner.s, 1);
        const auto current_recovery = read_nullable_text(owner.s, 2);
        if (!current_token)
            return rollback(AccountChangeResult::DbError);
        // Exact retries were handled above. A new change must retire the values
        // it promises to replace, even if a custom client reuses its candidates.
        if ((recovering && *current_token == nextHash) ||
            (current_recovery && *current_recovery == recoveryHash))
            return rollback(AccountChangeResult::InvalidRequest);
    }
    {
        StmtGuard update;
        if (sqlite3_prepare_v2(db_,
                               "UPDATE players SET "
                               "token_hash=?1,recovery_hash=?2,auth_epoch=auth_epoch+1,last_credential_op=?3 "
                               "WHERE id=?4 AND auth_epoch<9223372036854775807",
                               -1, &update.s, nullptr) != SQLITE_OK)
            return rollback(AccountChangeResult::DbError);
        if (sqlite3_bind_text(update.s, 1, nextHash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(update.s, 2, recoveryHash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(update.s, 3, receipt.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_int64(update.s, 4, player_id) != SQLITE_OK)
            return rollback(AccountChangeResult::DbError);
        const auto rc = sqlite3_step(update.s);
        if (rc != SQLITE_DONE) {
            const int detail = sqlite3_extended_errcode(db_);
            const bool duplicate = detail == SQLITE_CONSTRAINT_UNIQUE || detail == SQLITE_CONSTRAINT_PRIMARYKEY;
            return rollback(duplicate ? AccountChangeResult::Conflict : AccountChangeResult::DbError);
        }
        if (sqlite3_changes(db_) != 1)
            return rollback(AccountChangeResult::DbError);
    }
    out_player = read_player_by_token(db_, next_token);
    if (!out_player || sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
        return rollback(AccountChangeResult::DbError);
    transaction.committed = true;
    return AccountChangeResult::Ok;
}
} // namespace meta
