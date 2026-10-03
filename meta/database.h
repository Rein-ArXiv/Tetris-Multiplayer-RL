#pragma once

// meta/database.h — SQLite 래퍼.
//
// 스레드 모델:
//   cpp-httplib 의 요청 스레드 여러 개에서 동시에 호출될 수 있다. 이 클래스는
//   내부 std::mutex 로 DB 연결에 접근하는 메서드를 직렬화한다. 정적 카탈로그 조회는
//   공유 DB를 사용하지 않는다. 성능 최적화보다는
//   데이터 정합성 + 단순함 우선. SQLite 자체도 SQLITE_THREADSAFE=1 (기본) 로
//   컴파일되어 serialized 모드.
//
// 실패 정책:
//   · open 실패 → 생성자가 std::runtime_error throw. main 이 exit(1).
//   · 스키마/마이그레이션 실패 → 생성자가 연결을 닫고 throw.
//   · 쿼리 실패 → 메서드별 optional/상태값/빈 목록 반환. 호출부의 실패 매핑 확인.
//     모든 실패가 반드시 HTTP 500으로 구분되는 것은 아니다.
//
// 스키마: players, player_icons, matches, elo_history, bot_rewards, schema_migrations.
// WAL + foreign keys + FULL (credential rotation must survive a committed response).

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace meta {

struct Player {
    int64_t     id;
    std::optional<std::string> username;
    int64_t     auth_epoch = 0; // Changes invalidate outstanding admission tickets.
    int         elo;    // RP (0 시작 / 0 바닥 스케일 — meta/elo.h 참조)
    int         wins;
    int         losses;
    int         bp;
    int         xp;     // 누적 경험치 (감소하지 않음 — 레벨은 meta/levels.h 로 유도)
    std::string selected_icon_id;
};

struct IconCatalogEntry {
    std::string id;
    std::string name;
    int         price_bp;
    bool        default_owned;
};

struct MatchRecord {
    std::string                    match_uuid;  // 32 lowercase hex, unique/idempotent
    // winner=std::nullopt → 검증된 무승부 (RP 미반영). 검증실패는 저장하지 않는다.
    int64_t                    player_a;
    int64_t                    player_b;
    std::optional<int64_t>     winner;
    int                        score_a;
    int                        score_b;
    int                        lines_a;
    int                        lines_b;
    int                        duration_s;
};

struct EloDelta {
    int elo_before;
    int elo_after;
    int delta;
};

enum class MatchSaveError { None, IdentityConflict, Database };

struct MatchInsertResult {
    int64_t  match_id;
    EloDelta a;  // player_a 기준 변동 (winner 미정이면 0)
    EloDelta b;  // player_b 기준 변동
};

struct LeaderRow {
    int64_t                    player_id;
    std::optional<std::string> username;
    int                        elo;
    int                        wins;
    int                        losses;
    int                        xp;
};

enum class IconSelectResult {
    Ok,
    UnknownToken,
    InvalidIcon,
    NotOwned,
    DbError,
};

enum class IconPurchaseResult {
    Ok,
    UnknownToken,
    InvalidIcon,
    AlreadyOwned,
    InsufficientBp,
    DbError,
};

enum class AccountChangeResult { Ok, InvalidCredential, InvalidRequest, Conflict, DbError };

class Database {
public:
    // path 가 존재하지 않으면 새로 만들고 스키마 적용. 실패 시 throw.
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // 새 guest 플레이어 생성. token 은 외부에서 생성한 32 hex 문자열.
    // 반환: 성공 시 Player, 실패 시 nullopt (token 충돌 등).
    std::optional<Player> registerGuest(const std::string& token);

    // 토큰으로 플레이어 조회. 못 찾으면 nullopt.
    std::optional<Player> getByToken(const std::string& token);

    // Admission snapshot is valid only while this credential generation is current.
    std::optional<Player> getByEpoch(int64_t player_id, int64_t epoch);

    // Atomic replacement + bounded (one receipt/player) retry acknowledgement.
    // backup: same token, new recovery code; rotate/recover: replace both secrets.
    AccountChangeResult changeAccount(const std::string& operation,
        const std::string& current, const std::string& next_token,
        const std::string& next_recovery, std::optional<Player>& out_player);

    // 아이콘 카탈로그. 가격/기본 지급 여부는 서버 DB가 검증 기준으로 사용한다.
    std::vector<IconCatalogEntry> iconCatalog() const;

    // 아이콘 구매/선택. token 기준으로 플레이어를 찾고, DB 소유권과 BP를 검증한다.
    IconPurchaseResult purchaseIcon(const std::string& token,
                                    const std::string& icon_id,
                                    std::optional<Player>& out_player);
    IconSelectResult   selectIcon  (const std::string& token,
                                    const std::string& icon_id,
                                    std::optional<Player>& out_player);

    // 매치 기록 + RP 업데이트 (winner != nullopt 일 때만). 같은 match_uuid가
    // 같은 내용으로 재전송되면 최초 결과를 반환하고 RP/BP/XP를 다시 적용하지 않는다.
    // 같은 키의 내용이 다르면 IdentityConflict로 거절한다.
    // 단일 트랜잭션 안에서 matches INSERT → players UPDATE × 2 → elo_history × 2.
    // 실패 시 nullopt. 새 쓰기의 중간 실패는 롤백하고, 충돌은 쓰기 전에 거절한다.
    std::optional<MatchInsertResult> saveMatch(const MatchRecord& m,
                                               MatchSaveError* error = nullptr);

    // Verified PvE only: 10 BP/win, up to 100 BP per UTC day. No RP/XP/win-loss.
    // Ticket id is globally unique; the transaction makes retries idempotent.
    std::optional<int> botReward(int64_t player, const std::string& ticket);
    std::optional<int> saveBotWin(int64_t player, const std::string& ticket, const std::string& opponent);

    // RP 내림차순 상위 N명. limit 은 1..100 으로 clamp.
    // Empty vector is a successful empty ranking; nullopt is a failed query.
    std::optional<std::vector<LeaderRow>> leaderboard(int limit);

private:
    void migrateCredentials();
    void execSchema();          // 스키마 CREATE + PRAGMA. 실패 시 throw.

    sqlite3*    db_ = nullptr;
    std::mutex  mu_;            // 공유 DB connection 접근을 직렬화한다.
};

} // namespace meta
