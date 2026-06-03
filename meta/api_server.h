#pragma once

// meta/api_server.h — cpp-httplib 위에 라우팅을 얹어 4개 엔드포인트 제공.
//
//   POST /v1/guest           익명 guest 플레이어 생성
//   POST /v1/auth/verify     토큰 검증 (relay 가 QUEUE_JOIN 수신 후 호출)
//   POST /v1/matches         경기 결과 저장 + ELO 업데이트 (relay secret 보호 가능)
//   GET  /v1/leaderboard     상위 N명 조회
//   GET  /v1/icons/catalog   아이콘 카탈로그
//   POST /v1/icons/buy       BP 차감 + 아이콘 소유권 부여
//   POST /v1/icons/select    소유한 아이콘을 selected_icon_id 로 저장
//   GET  /healthz            운영 상태 확인
//
// CORS: GET /v1/leaderboard 는 브라우저 정적 페이지에서 직접 fetch 가능하도록
//   `Access-Control-Allow-Origin: *` 를 전 응답에 달아둔다 (OPTIONS 도 처리).
// 서버는 blocking 호출 → main 스레드에서 listen() 실행.

#include "database.h"

#include <cstdint>
#include <string>

namespace meta {

class ApiServer {
public:
    explicit ApiServer(Database& db, std::string relay_secret = {});

    // 포트 리스닝을 시작 (블로킹). 호출자가 main 에서 직접 부름.
    // host 는 "0.0.0.0" 또는 "127.0.0.1".
    // 반환: 리스닝 실패 시 false, 정상 종료 시 true.
    bool listen(const std::string& host, int port);

private:
    Database& db_;
    std::string relay_secret_;
};

} // namespace meta
