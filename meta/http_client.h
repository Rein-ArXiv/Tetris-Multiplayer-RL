#pragma once

// meta/http_client.h — tetris_meta HTTP API 를 호출하기 위한 가벼운 클라이언트.
//
// relay 와 game client 양쪽에서 재사용한다.
//   · game client   : request_guest()  (첫 실행 시 익명 토큰 발급)
//   · tetris_relay  : verify_token()   (QUEUE_JOIN 수신 후 인증)
//   · tetris_relay  : post_match()     (경기 결과 저장 + ELO 갱신)
//
// 네트워크 실패/서버 에러는 std::nullopt 로 통합 처리 — 호출자가 장애 정책
// (매치 거부 / result 미반영) 적용. 에러 원인은 stderr 로 간단 로그만.
//
// 구현: third_party/httplib.h 의 httplib::Client/SSLClient 위에 thin wrapper.

#include <cstdint>
#include <optional>
#include <string>

namespace meta::client {

// ---- 응답 shape ------------------------------------------------------------
struct GuestInfo {
    int64_t     player_id;
    std::string token;
    int         elo;
    int         bp;
    std::string selected_icon_id;
};

struct AuthInfo {
    int64_t     player_id;
    std::string username;   // 비어 있으면 username=null
    int         elo;
    int         bp;
    std::string selected_icon_id;
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
    // 를 구분해야 자동 재발급(stale 토큰)을 할 수 있다.
    enum class VerifyOutcome {
        Ok,             // info 유효
        UnknownToken,   // 200 OK 가 아니라 404 응답 — 새 guest 발급 필요
        NetworkError,   // 연결 실패 / 타임아웃 / 그 외 — 토큰은 유지하고 다음에 재시도
    };

    // 주요 엔드포인트. timeout_s: 네트워크 전체 deadline. 계획문서의 기본값과 동일.
    std::optional<GuestInfo>  request_guest  (int timeout_s = 5);
    // 기존 호출 호환: outcome 무시 시 nullopt 가 unknown 또는 network 실패.
    // 호출부가 회복 정책을 적용하려면 outcome 인자를 채워서 호출.
    std::optional<AuthInfo>   verify_token   (const std::string& token,
                                              int timeout_s = 3,
                                              VerifyOutcome* out_outcome = nullptr);
    std::optional<AuthInfo>   purchase_icon  (const std::string& token,
                                              const std::string& icon_id,
                                              int timeout_s = 5);
    std::optional<AuthInfo>   select_icon    (const std::string& token,
                                              const std::string& icon_id,
                                              int timeout_s = 5);
    std::optional<MatchResult> post_match    (int64_t player_a, int64_t player_b,
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

// ---- 클라이언트 토큰 저장 (플랫폼별 user-data 디렉토리) --------------------
//
// Windows: %APPDATA%\Tetris\token
// macOS:   $HOME/Library/Application Support/Tetris/token
// Linux:   $XDG_DATA_HOME/Tetris/token  (fallback: $HOME/.local/share/Tetris/token)

// 전체 경로 반환. 디렉토리 생성까지는 하지 않는다 (save 시점에 생성).
std::string token_file_path();

// 파일에서 토큰 읽기. 없거나 손상이면 빈 문자열.
std::string load_token();

// 토큰 저장 (부모 디렉토리 자동 생성). 실패 시 false.
bool save_token(const std::string& token);

} // namespace meta::client
