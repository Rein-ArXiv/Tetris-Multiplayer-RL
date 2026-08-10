// server/relay.h — 페어링된 두 소켓의 로비와 양방향 게임 전송
//
// 설계:
//   1) startPump/startQueuePump가 양쪽에 MATCH_FOUND(role, seed, icons,
//      match_uuid)를 보내고, 커스텀 룸은 바로 게임으로, 랜덤 큐는 양쪽
//      READY(1)를 확인한 뒤 게임으로 전환한다.
//   2) 게임 중에는 방향별 worker(A→B, B→A)가 같은 Channel을 공유한다.
//      unranked 매치는 raw byte를 전달한다. ranked 매치는 프레임 경계를 읽어
//      MATCH_SUMMARY만 체크섬·크기 검증 후 가로채고, 나머지 wire byte는 그대로
//      전달한다. lockstep 상태 계산 자체는 여전히 클라이언트 책임이다.
//   3) 방향별 15초 idle timeout과 64 KiB/s 상한으로 멈춘 연결과 flood를
//      끊는다. 한쪽 소켓 오류/EOF가 먼저 관측되면 상대 방향도 종료시키고,
//      ranked 매치는 그 단절을 서버 관측 기권으로 meta에 기록한다. 두 정상
//      MATCH_SUMMARY가 모이면 교차검증한 결과를 멱등 match_uuid로 저장한다.
//   4) 소켓, 계정 session lease, summary, 송신 mutex는 shared Channel이 소유한다.
//      마지막 forwarder가 양 소켓을 shutdown하고 Channel 소멸이 실제 handle과
//      lease를 해제한다. worker는 전역 WorkerGroup이 추적해 서버 종료 때 drain한다.

#pragma once
#include "matchmaker.h"

namespace meta::client { class MetaClient; }

namespace relay {

// 페어링된 Match 를 받아 MATCH_FOUND 전송 후 양방향 포워딩 시작.
// 내부에서 소유권 이전. worker는 종료 시까지 relay runtime이 추적한다.
//
// meta: non-null 이고 양쪽 player_id != 0 일 때만 경기 종료 후
//   MATCH_SUMMARY 교차검증 + /v1/matches POST + MATCH_RESULT 송신.
//   nullptr 이거나 unranked 매치면 MATCH_SUMMARY를 포함한 raw byte를 전달한다.
//
// 커스텀 룸 경로 전용 — 양쪽이 이미 룸 로비에서 READY 로 수락한 상태라
// 바로 게임 포워딩을 연다.
void startPump(Match match, meta::client::MetaClient* meta);

// 랜덤 큐 페어링 전용 — MATCH_FOUND 를 보낸 뒤 양쪽이 READY(1) 을 보낼 때까지
// 대기하는 "수락 로비" 단계를 끼워넣는다.
//   · 30초 안에 양쪽 READY(1) 수신 → 게임 포워딩 시작 (startPump 와 동일 경로).
//   · 한쪽이 READY(0) / QUEUE_CANCEL / EOF / 타임아웃 → 양 소켓 close.
//   · 수락 로비 동안 READY 는 상대에게 그대로 forward 해 UI 반영 가능.
// matcher 스레드를 블록하지 않도록 내부 worker에서 실행한다.
void startQueuePump(Match match, meta::client::MetaClient* meta);

// 서버 종료 프로토콜. beginShutdown 이후에는 새 pump를 받지 않고 기존 lobby와
// forwarder가 루프를 빠져나온다. waitForShutdown은 모든 pump worker가 끝날 때까지
// 기다리므로, worker가 참조하는 MetaClient와 net 전역 상태보다 먼저 호출해야 한다.
void beginShutdown();
void waitForShutdown();
bool isShuttingDown();

}  // namespace relay
