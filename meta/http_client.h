#pragma once

// meta/http_client.h — tetris_meta HTTP API 를 호출하기 위한 가벼운 클라이언트.
//
// relay 와 game client 양쪽에서 재사용한다.
//   · game client   : request_guest()  (첫 실행 시 익명 토큰 발급)
//   · tetris_relay  : consume_game_ticket() (QUEUE_JOIN 입장권 소비)
//   · tetris_relay  : post_match()     (경기 결과 저장 + RP 갱신)
//
// 네트워크 실패/서버 에러는 std::nullopt 로 통합 처리 — 호출자가 장애 정책
// (매치 거부 / result 미반영) 적용. 에러 원인은 stderr 로 간단 로그만.
//
// 구현: third_party/httplib.h 의 httplib::Client/SSLClient 위에 thin wrapper.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace meta::client {

// ---- 응답 shape ------------------------------------------------------------
struct GuestInfo {
    int64_t     player_id;
    std::string token;
    int         elo;    // RP (0 시작 스케일)
    int         bp;
    int         xp;     // 누적 경험치 — 레벨은 meta/levels.h 의 level_for_xp 로 유도
    std::string selected_icon_id;
};

struct AuthInfo {
    int64_t     player_id;
    std::string username;   // 비어 있으면 username=null
    int         elo;
    int         bp;
    int         xp;
    std::string selected_icon_id;
};

// GET /v1/icons/catalog 의 행. 서버 meta/database.cpp 의 kIconCatalog 와 대응.
struct IconEntry {
    std::string id;            // "default" / "ruby" / "gold" ...
    std::string name;          // 표시명
    int         price_bp;      // 구매 가격 (BP)
    bool        default_owned; // true 면 모든 플레이어가 기본 보유
};

struct MatchDelta {
    int elo_before;
    int elo_after;
    int delta;
};

struct MatchResult {
    int64_t     match_id;
    MatchDelta  a;
    MatchDelta  b;
};

struct BotChallenge {
    std::string ticket;
    uint64_t seed = 0;
    int input_ticks = 6, think_ticks = 18, min_piece_ticks = 60;
};
struct BotReward { int awarded_bp = 0; int bp = 0; };

// ---- 메타 서버 클라이언트 --------------------------------------------------
class MetaClient {
public:
    // base_url 형식: "http://host:port", "https://host" 등.
    // HTTPS 는 CMake 가 OpenSSL 을 찾은 빌드에서만 valid() == true.
    // 잘못된 URL 이면 valid() == false. 이후 모든 호출은 nullopt 반환.
    explicit MetaClient(const std::string& base_url,
                        std::string relay_secret = {});

    bool valid() const { return valid_; }
    const std::string& baseUrl() const { return base_url_; }

    // verify_token 결과 — 호출자가 "토큰이 잘못된 것" vs "서버 다운/네트워크 실패"
    // 를 구분해 복구 안내 또는 기존 키 재시도를 선택한다. 자동 계정 교체는 금지한다.
    enum class VerifyOutcome {
        Ok,             // info 유효
        UnknownToken,   // 404 응답 — 기존 파일 유지, 복구 안내
        NetworkError,   // 연결 실패 / 타임아웃 / 그 외 — 토큰은 유지하고 다음에 재시도
    };

    // 주요 엔드포인트. timeout_s: 네트워크 전체 deadline. 계획문서의 기본값과 동일.
    std::optional<GuestInfo>  request_guest  (int timeout_s = 5);
    // 기존 호출 호환: outcome 무시 시 nullopt 가 unknown 또는 network 실패.
    // 호출부가 회복 정책을 적용하려면 outcome 인자를 채워서 호출.
    std::optional<AuthInfo>   verify_token   (const std::string& token,
                                              int timeout_s = 3,
                                              VerifyOutcome* out_outcome = nullptr);
    // Game admission only: issue with an account credential, redeem once with
    // relay secret. Never use verify_token's offline/cache behavior for tickets.
    std::optional<std::string> request_game_ticket(const std::string& token);
    std::optional<AuthInfo> consume_game_ticket(const std::string& ticket);

    std::optional<AuthInfo> change_account(const std::string& operation,
        const std::string& credential, const std::string& next_token,
        const std::string& next_recovery, int* status=nullptr);

    // 아이콘 카탈로그 전체. 실패(네트워크/파싱) 시 nullopt.
    std::optional<std::vector<IconEntry>> fetch_icon_catalog(int timeout_s = 5);

    std::optional<BotChallenge> start_bot_challenge(const std::string& token, const std::string& opponent, int* status = nullptr);
    std::optional<BotReward> claim_bot_reward(const std::string& token, const std::string& ticket, const std::string& inputs, int* status = nullptr);

    // out_http_status: 0 = 네트워크 실패, 그 외 HTTP 상태 코드. UI 가
    // 402(insufficient_bp) / 403(not_owned) / 409(already_owned) 를 구분해
    // "구매 확인" 흐름을 만들 수 있게 한다. nullptr 면 무시.
    std::optional<AuthInfo>   purchase_icon  (const std::string& token,
                                              const std::string& icon_id,
                                              int timeout_s = 5,
                                              int* out_http_status = nullptr);
    std::optional<AuthInfo>   select_icon    (const std::string& token,
                                              const std::string& icon_id,
                                              int timeout_s = 5,
                                              int* out_http_status = nullptr);
    std::optional<MatchResult> post_match    (const std::string& match_uuid,
                                              int64_t player_a, int64_t player_b,
                                              std::optional<int64_t> winner,
                                              int score_a, int score_b,
                                              int lines_a, int lines_b,
                                              int duration_s,
                                              int timeout_s = 10);

private:
    std::string base_url_;
    std::string host_;
    int         port_ = 80;
    bool        https_ = false;
    bool        valid_ = false;
    std::string relay_secret_;
};

} // namespace meta::client
