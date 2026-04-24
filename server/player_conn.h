// server/player_conn.h — 연결별 스레드 진입점
//
// 역할: accept 된 소켓에서 QUEUE_JOIN 프레임을 일정 시간 안에 받으면
//       matchmaker 큐에 등록, 타임아웃 또는 연결 실패 시 소켓 닫고 종료.
//       (실제 페어링/릴레이는 matcher 스레드와 relay::startPump 가 담당.)

#pragma once
#include "../net/socket.h"
#include <cstdint>

namespace meta::client { class MetaClient; }

namespace relay {

class Matchmaker;
class RoomRegistry;

// playerConnThread 는 detached 스레드에서 실행됨.
// 첫 프레임이:
//   - QUEUE_JOIN   → matchmaker 에 등록 후 종료
//   - ROOM_CREATE  → RoomRegistry::handleCreate 로 이관 (내부에서 블로킹)
//   - ROOM_JOIN    → RoomRegistry::handleJoin  로 이관 (내부에서 블로킹)
// 타임아웃/EOF/알 수 없는 프레임: tcp_close(sock) 후 종료.
//
// meta: nullptr 이면 unranked 모드 — 토큰 검증 생략, player_id=0 로 매칭.
//       non-null 이면 QUEUE_JOIN/ROOM_* 의 token 페이로드를 meta /v1/auth/verify
//       로 검증. 빈 토큰이거나 검증 실패 시 소켓 close (매치 입장 거부).
void playerConnThread(net::TcpSocket sock, uint32_t conn_id,
                      Matchmaker& mm, RoomRegistry& rr,
                      meta::client::MetaClient* meta);

}  // namespace relay
