// server/relay.h — 페어링된 두 소켓 간 양방향 바이트 포워딩
//
// 설계:
//   1) startPump(Match) 호출 시점에 양쪽 클라이언트에 MATCH_FOUND 프레임 전송.
//   2) 이후 두 개의 detached 스레드가 각 방향(A→B, B→A)으로 바이트를 흘려보냄.
//      — 프레임 파싱 안 함. 체크섬 검증 안 함. 그냥 recv→send 파이프.
//      — lockstep 게임 결정론은 클라가 책임지므로 서버는 투명 중계자.
//   3) 한쪽이 끊기면 반대쪽 스레드도 read EOF 를 받아 종료 →
//      마지막 스레드가 양 fd 닫고 자원 해제 (shared_ptr refcount 로 관리).

#pragma once
#include "matchmaker.h"

namespace meta::client { class MetaClient; }

namespace relay {

// 페어링된 Match 를 받아 MATCH_FOUND 전송 후 양방향 포워딩 시작.
// 내부에서 소유권 이전 → detached 스레드들이 수명 관리.
//
// meta: non-null 이고 양쪽 player_id != 0 일 때만 경기 종료 후
//   MATCH_SUMMARY 교차검증 + /v1/matches POST + MATCH_RESULT 송신.
//   nullptr 이거나 unranked 매치면 MATCH_SUMMARY 도 투명 포워딩.
//
// 커스텀 룸 경로 전용 — 양쪽이 이미 룸 로비에서 READY 로 수락한 상태라
// 바로 게임 포워딩을 연다.
void startPump(Match match, meta::client::MetaClient* meta);

// 랜덤 큐 페어링 전용 — MATCH_FOUND 를 보낸 뒤 양쪽이 READY(1) 을 보낼 때까지
// 대기하는 "수락 로비" 단계를 끼워넣는다.
//   · 30초 안에 양쪽 READY(1) 수신 → 게임 포워딩 시작 (startPump 와 동일 경로).
//   · 한쪽이 READY(0) / QUEUE_CANCEL / EOF / 타임아웃 → 양 소켓 close.
//   · 수락 로비 동안 READY 는 상대에게 그대로 forward 해 UI 반영 가능.
// matcher 스레드를 블록하지 않도록 내부에서 자체 스레드 detach.
void startQueuePump(Match match, meta::client::MetaClient* meta);

}  // namespace relay
