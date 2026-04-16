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

namespace relay {

// 페어링된 Match 를 받아 MATCH_FOUND 전송 후 양방향 포워딩 시작.
// 내부에서 소유권 이전 → detached 스레드들이 수명 관리.
void startPump(Match match);

}  // namespace relay
