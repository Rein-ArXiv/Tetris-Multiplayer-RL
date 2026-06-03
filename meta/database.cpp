#include "database.h"
#include "elo.h"

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
PRAGMA synchronous  = NORMAL;

CREATE TABLE IF NOT EXISTS players (
  id          INTEGER PRIMARY KEY,
  username    TEXT,
  token       TEXT UNIQUE NOT NULL,
  elo         INTEGER NOT NULL DEFAULT 1200,
  wins        INTEGER NOT NULL DEFAULT 0,
  losses      INTEGER NOT NULL DEFAULT 0,
  bp          INTEGER NOT NULL DEFAULT 0,
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
  player_a    INTEGER NOT NULL REFERENCES players(id),
  player_b    INTEGER NOT NULL REFERENCES players(id),
  winner      INTEGER          REFERENCES players(id),
  score_a     INTEGER NOT NULL,
  score_b     INTEGER NOT NULL,
  lines_a     INTEGER NOT NULL,
  lines_b     INTEGER NOT NULL,
  duration_s  INTEGER NOT NULL,
  created_at  INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS elo_history (
  id          INTEGER PRIMARY KEY,
  player_id   INTEGER NOT NULL REFERENCES players(id),
  match_id    INTEGER NOT NULL REFERENCES matches(id),
  elo_before  INTEGER NOT NULL,
  elo_after   INTEGER NOT NULL,
  delta       INTEGER NOT NULL,
  created_at  INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_players_elo    ON players(elo DESC);
CREATE INDEX IF NOT EXISTS idx_matches_played ON matches(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_elo_pid        ON elo_history(player_id);
CREATE INDEX IF NOT EXISTS idx_player_icons_pid ON player_icons(player_id);
)sql";

const char* kDefaultIconId = "default";

const IconCatalogEntry kIconCatalog[] = {
    {"default", "Default", 0,   true},
    {"ruby",    "Ruby",    100, false},
    {"gold",    "Gold",    250, false},
};

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

std::optional<Player> read_player_by_token(sqlite3* db, const std::string& token)
{
    StmtGuard g;
    const char* sql =
        "SELECT id,username,token,elo,wins,losses,bp,selected_icon_id "
        "FROM players WHERE token=?1";
    if (sqlite3_prepare_v2(db, sql, -1, &g.s, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[db] getByToken prepare: %s\n", sqlite3_errmsg(db));
        return std::nullopt;
    }
    sqlite3_bind_text(g.s, 1, token.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(g.s);
    if (rc == SQLITE_DONE) return std::nullopt;
    if (rc != SQLITE_ROW) {
        std::fprintf(stderr, "[db] getByToken step: rc=%d %s\n",
                     rc, sqlite3_errmsg(db));
        return std::nullopt;
    }

    Player p;
    p.id       = sqlite3_column_int64(g.s, 0);
    p.username = read_nullable_text(g.s, 1);
    p.token    = reinterpret_cast<const char*>(sqlite3_column_text(g.s, 2));
    p.elo      = sqlite3_column_int  (g.s, 3);
    p.wins     = sqlite3_column_int  (g.s, 4);
    p.losses   = sqlite3_column_int  (g.s, 5);
    p.bp       = sqlite3_column_int  (g.s, 6);
    const unsigned char* icon = sqlite3_column_text(g.s, 7);
    p.selected_icon_id = icon ? reinterpret_cast<const char*>(icon) : kDefaultIconId;
    if (!find_icon_def(p.selected_icon_id)) p.selected_icon_id = kDefaultIconId;
    return p;
}

bool player_owns_icon(sqlite3* db, int64_t player_id, const std::string& icon_id)
{
    const IconCatalogEntry* def = find_icon_def(icon_id);
    if (!def) return false;
    if (def->default_owned) return true;

    StmtGuard g;
    const char* sql =
        "SELECT 1 FROM player_icons WHERE player_id=?1 AND icon_id=?2";
    if (sqlite3_prepare_v2(db, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(g.s, 1, player_id);
    sqlite3_bind_text (g.s, 2, icon_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_ROW;
}

bool insert_icon_ownership(sqlite3* db, int64_t player_id, const std::string& icon_id)
{
    StmtGuard g;
    const char* sql =
        "INSERT OR IGNORE INTO player_icons(player_id,icon_id,created_at)"
        " VALUES(?1,?2,?3)";
    if (sqlite3_prepare_v2(db, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(g.s, 1, player_id);
    sqlite3_bind_text (g.s, 2, icon_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(g.s, 3, now_unix());
    return sqlite3_step(g.s) == SQLITE_DONE;
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
    execSchema();
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

    // 기존 tetris.db 를 보존하면서 신규 컬럼을 붙인다. duplicate column 은 이미
    // 마이그레이션된 DB 라는 뜻이므로 무시한다.
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
}

// -----------------------------------------------------------------------------
std::optional<Player>
Database::registerGuest(const std::string& token)
{
    std::lock_guard<std::mutex> lk(mu_);

    StmtGuard g;
    const char* sql =
        "INSERT INTO players(username,token,elo,wins,losses,created_at) "
        "VALUES(NULL,?1,1200,0,0,?2)";
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[db] registerGuest prepare: %s\n", sqlite3_errmsg(db_));
        return std::nullopt;
    }
    sqlite3_bind_text (g.s, 1, token.c_str(), -1, SQLITE_TRANSIENT);
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
    p.token   = token;
    p.elo     = 1200;
    p.wins    = 0;
    p.losses  = 0;
    p.bp      = 0;
    p.selected_icon_id = kDefaultIconId;
    // username 은 기본 NULL
    insert_icon_ownership(db_, p.id, kDefaultIconId);
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

    auto p = read_player_by_token(db_, token);
    if (!p) return IconPurchaseResult::UnknownToken;
    if (player_owns_icon(db_, p->id, icon_id))
        return IconPurchaseResult::AlreadyOwned;
    if (p->bp < icon->price_bp)
        return IconPurchaseResult::InsufficientBp;

    char* err = nullptr;
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "[db] purchaseIcon BEGIN: %s\n", err ? err : "?");
        sqlite3_free(err);
        return IconPurchaseResult::DbError;
    }

    auto rollback = [&] {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return IconPurchaseResult::DbError;
    };

    {
        StmtGuard g;
        const char* sql = "UPDATE players SET bp=bp-?1 WHERE id=?2 AND bp>=?1";
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK)
            return rollback();
        sqlite3_bind_int  (g.s, 1, icon->price_bp);
        sqlite3_bind_int64(g.s, 2, p->id);
        if (sqlite3_step(g.s) != SQLITE_DONE || sqlite3_changes(db_) != 1)
            return rollback();
    }
    if (!insert_icon_ownership(db_, p->id, icon_id)) return rollback();

    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "[db] purchaseIcon COMMIT: %s\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return IconPurchaseResult::DbError;
    }
    out_player = read_player_by_token(db_, token);
    return out_player ? IconPurchaseResult::Ok : IconPurchaseResult::DbError;
}

IconSelectResult
Database::selectIcon(const std::string& token,
                     const std::string& icon_id,
                     std::optional<Player>& out_player)
{
    out_player.reset();
    std::lock_guard<std::mutex> lk(mu_);

    if (!find_icon_def(icon_id)) return IconSelectResult::InvalidIcon;
    auto p = read_player_by_token(db_, token);
    if (!p) return IconSelectResult::UnknownToken;
    if (!player_owns_icon(db_, p->id, icon_id)) return IconSelectResult::NotOwned;

    StmtGuard g;
    const char* sql = "UPDATE players SET selected_icon_id=?1 WHERE id=?2";
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK)
        return IconSelectResult::DbError;
    sqlite3_bind_text (g.s, 1, icon_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(g.s, 2, p->id);
    if (sqlite3_step(g.s) != SQLITE_DONE)
        return IconSelectResult::DbError;

    out_player = read_player_by_token(db_, token);
    return out_player ? IconSelectResult::Ok : IconSelectResult::DbError;
}

// -----------------------------------------------------------------------------
std::optional<MatchInsertResult>
Database::saveMatch(const MatchRecord& m)
{
    std::lock_guard<std::mutex> lk(mu_);

    // 트랜잭션 시작. IMMEDIATE: 쓰기 락 즉시 확보해 reader 때문에 밀리지 않게.
    char* err = nullptr;
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "[db] BEGIN: %s\n", err ? err : "?");
        sqlite3_free(err);
        return std::nullopt;
    }

    auto rollback = [&](const char* why) -> std::optional<MatchInsertResult> {
        std::fprintf(stderr, "[db] saveMatch rollback: %s (%s)\n",
                     why, sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return std::nullopt;
    };

    const int64_t ts = now_unix();

    // 1) INSERT matches
    int64_t match_id = 0;
    {
        StmtGuard g;
        const char* sql =
            "INSERT INTO matches"
            "(player_a,player_b,winner,score_a,score_b,lines_a,lines_b,duration_s,created_at)"
            " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)";
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK)
            return rollback("matches prepare");
        sqlite3_bind_int64(g.s, 1, m.player_a);
        sqlite3_bind_int64(g.s, 2, m.player_b);
        if (m.winner) sqlite3_bind_int64(g.s, 3, *m.winner);
        else          sqlite3_bind_null (g.s, 3);
        sqlite3_bind_int  (g.s, 4, m.score_a);
        sqlite3_bind_int  (g.s, 5, m.score_b);
        sqlite3_bind_int  (g.s, 6, m.lines_a);
        sqlite3_bind_int  (g.s, 7, m.lines_b);
        sqlite3_bind_int  (g.s, 8, m.duration_s);
        sqlite3_bind_int64(g.s, 9, ts);
        if (sqlite3_step(g.s) != SQLITE_DONE) return rollback("matches step");
        match_id = sqlite3_last_insert_rowid(db_);
    }

    // 2) ELO 읽기 + 계산
    auto get_elo = [&](int64_t pid, int& out) -> bool {
        StmtGuard g;
        if (sqlite3_prepare_v2(db_, "SELECT elo FROM players WHERE id=?1", -1,
                               &g.s, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int64(g.s, 1, pid);
        int rc = sqlite3_step(g.s);
        if (rc != SQLITE_ROW) return false;
        out = sqlite3_column_int(g.s, 0);
        return true;
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

    // 3) UPDATE players (winner 가 있을 때만 elo + wins/losses 갱신).
    if (m.winner) {
        auto update_player = [&](int64_t pid, int new_elo, bool won) -> bool {
            StmtGuard g;
            const char* sql = won
                ? "UPDATE players SET elo=?1, wins=wins+1   WHERE id=?2"
                : "UPDATE players SET elo=?1, losses=losses+1 WHERE id=?2";
            if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_int  (g.s, 1, new_elo);
            sqlite3_bind_int64(g.s, 2, pid);
            return sqlite3_step(g.s) == SQLITE_DONE;
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
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return std::nullopt;
    }

    MatchInsertResult r;
    r.match_id = match_id;
    r.a = { elo_a_before, elo_a_after, elo_a_after - elo_a_before };
    r.b = { elo_b_before, elo_b_after, elo_b_after - elo_b_before };
    return r;
}

// -----------------------------------------------------------------------------
std::vector<LeaderRow>
Database::leaderboard(int limit)
{
    std::lock_guard<std::mutex> lk(mu_);

    limit = std::clamp(limit, 1, 100);

    StmtGuard g;
    const char* sql =
        "SELECT id,username,elo,wins,losses FROM players "
        "ORDER BY elo DESC, id ASC LIMIT ?1";
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[db] leaderboard prepare: %s\n", sqlite3_errmsg(db_));
        return {};
    }
    sqlite3_bind_int(g.s, 1, limit);

    std::vector<LeaderRow> rows;
    while (sqlite3_step(g.s) == SQLITE_ROW) {
        LeaderRow r;
        r.player_id = sqlite3_column_int64(g.s, 0);
        r.username  = read_nullable_text(g.s, 1);
        r.elo       = sqlite3_column_int(g.s, 2);
        r.wins      = sqlite3_column_int(g.s, 3);
        r.losses    = sqlite3_column_int(g.s, 4);
        rows.push_back(std::move(r));
    }
    return rows;
}

} // namespace meta
