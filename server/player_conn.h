// server/player_conn.h — 연결별 스레드 진입점
//
// 역할: accept 된 소켓에서 QUEUE_JOIN 프레임을 일정 시간 안에 받으면
//       matchmaker 큐에 등록, 타임아웃 또는 연결 실패 시 소켓 닫고 종료.
//       (실제 페어링/릴레이는 matcher 스레드와 relay::startPump 가 담당.)

#pragma once
#include "../net/socket.h"
#include <cstdint>

namespace relay {

class Matchmaker;

// playerConnThread 는 detached 스레드에서 실행됨.
// 성공 시: matchmaker 에 PlayerInfo 등록 후 즉시 종료 (fd 소유권 이전).
// 실패 시: tcp_close(sock) 후 종료.
void playerConnThread(net::TcpSocket sock, uint32_t conn_id, Matchmaker& mm);

}  // namespace relay
