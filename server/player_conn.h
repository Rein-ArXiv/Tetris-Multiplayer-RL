// server/player_conn.h — 연결별 스레드 진입점
//
// 역할: accept된 연결의 첫 명령을 읽어 대기열 또는 방 처리로 인계한다.
//       초기 폴링 마감·연결 실패·치명적 파서 오류이면 소켓을 닫는다.
//       다음 단계에는 소켓과 이미 받은 미소비 바이트를 함께 넘긴다.
//       실제 페어링과 경기 전달은 matcher 및 relay 계층이 담당한다.

#pragma once
#include "../net/socket.h"
#include "ip_admission.h"

#include <cstdint>
#include <memory>

namespace meta::client { class MetaClient; }

namespace relay {

class Matchmaker;
class RoomRegistry;

// playerConnThread 는 서버가 활성 수를 추적하는 worker에서 실행됨.
// 첫 프레임이:
//   - QUEUE_JOIN   → matchmaker 에 등록 후 종료
//   - ROOM_CREATE  → RoomRegistry::handleCreate 로 이관 (내부에서 블로킹)
//   - ROOM_JOIN    → RoomRegistry::handleJoin  로 이관 (내부에서 블로킹)
// 타임아웃/EOF/I/O 오류/파서 오류: tcp_close(sock) 후 종료.
// 문법상 정상인 알 수 없는 타입은 기한을 연장하지 않고 무시한다.
//
// meta: nullptr 이면 unranked 모드 — 토큰 검증 생략, player_id=0 로 매칭.
//       non-null 이면 QUEUE_JOIN/ROOM_*의 token 필드에 담긴 일회용 입장권을
//       meta /v1/game-tickets/consume으로 소비한다. 누락/실패면 입장 거부.
//
// handshake_slot: accept 시점에 잡은 per-IP 핸드셰이크 슬롯. 인증이 끝나 진로가
//       정해지는 순간 여기서 반납한다 — 그 뒤로 이 연결은 "핸드셰이크 중"이
//       아니므로, 붙들고 있으면 상한 16 이 사실상 동시 세션 상한이 되어 IP를
//       공유하는 사용자 집단을 서로 굶긴다.
// session_slot: accept 시점에 잡은 per-IP 세션 슬롯. 연결이 죽을 때까지 살아
//       있어야 하므로 소켓과 함께 큐(PlayerInfo) 또는 룸(Entry) 으로 넘긴다.
void playerConnThread(net::TcpSocket sock, uint32_t conn_id,
                      Matchmaker& mm, RoomRegistry& rr,
                      meta::client::MetaClient* meta,
                      std::shared_ptr<IpAdmission> handshake_slot = {},
                      std::shared_ptr<IpAdmission> session_slot = {});

}  // namespace relay
