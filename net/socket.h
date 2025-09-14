#pragma once
#include <cstdint>
#include <string>
#include <vector>

/**
 * TCP 소켓 추상화 레이어
 *
 * 이 모듈은 운영체제의 소켓 API를 얇게 감싸서 플랫폼 독립적인 네트워킹을 제공합니다.
 *
 * TCP의 특성:
 * - 연결 지향: 통신 전에 연결을 설정해야 함
 * - 스트림 기반: 메시지 경계가 없는 바이트 스트림
 * - 신뢰성 보장: 패킷 순서, 중복 제거, 에러 검출을 운영체제가 처리
 * - 흐름 제어: 수신자가 처리할 수 있는 속도로 전송
 *
 * 이 레이어의 책임:
 * - 소켓 생성, 연결, 송수신, 정리
 * - 플랫폼별 차이 흡수 (Windows WinSock vs Linux BSD socket)
 * - 논블로킹 I/O 설정
 *
 * 상위 레이어(framing)의 책임:
 * - 메시지 경계 구분
 * - 무결성 검사
 * - 프로토콜별 메시지 구조 정의
 */

namespace net {

/**
 * TCP 소켓 래퍼
 *
 * fd: 운영체제 소켓 파일 디스크립터
 * - Windows: SOCKET 타입 (실제로는 unsigned int)
 * - Linux: int 타입
 * - -1은 유효하지 않은 소켓을 의미
 */
struct TcpSocket {
    int fd{-1};  // 소켓 파일 디스크립터
    bool valid() const { return fd >= 0; }
};

/**
 * 네트워킹 시스템 초기화/정리
 *
 * Windows에서는 WinSock 라이브러리를 초기화해야 소켓 API를 사용할 수 있습니다.
 * Linux에서는 별도 초기화가 필요 없으므로 빈 함수입니다.
 *
 * 프로그램 시작 시 net_init() 호출, 종료 시 net_shutdown() 호출 필요
 */
bool net_init();
void net_shutdown();

/**
 * TCP 서버/클라이언트 연결 설정
 */

/**
 * 서버 소켓 생성 - 지정된 포트에서 클라이언트 연결을 기다립니다
 *
 * @param port 대기할 포트 번호 (1024 이상 권장)
 * @param backlog 대기큐 크기 (동시 연결 요청 수)
 * @return 대기 소켓 (accept()에서 사용)
 *
 * 네트워크 개념:
 * - bind(): 소켓을 특정 포트에 바인딩
 * - listen(): 연결 요청 대기 상태로 전환
 * - SO_REUSEADDR: 포트 재사용 허용 (개발 시 유용)
 */
TcpSocket tcp_listen(uint16_t port, int backlog=1);

/**
 * 클라이언트 연결 수락 - listen 소켓에서 실제 연결을 생성합니다
 *
 * @param server tcp_listen()으로 생성한 서버 소켓
 * @return 클라이언트와 통신할 수 있는 소켓
 *
 * 네트워크 개념:
 * - accept()는 블로킹 함수 - 연결 요청이 올 때까지 대기
 * - 반환된 소켓은 특정 클라이언트와 1:1 통신용
 * - 논블로킹 모드로 설정되어 I/O 스레드에서 안전하게 사용 가능
 */
TcpSocket tcp_accept(const TcpSocket& server);

/**
 * 클라이언트 연결 시도 - 서버에 연결을 요청합니다
 *
 * @param host 서버 주소 (IP 또는 도메인명)
 * @param port 서버 포트
 * @return 서버와 통신할 수 있는 소켓
 *
 * 네트워크 개념:
 * - getaddrinfo(): 호스트명을 IP 주소로 변환
 * - connect(): 3-way handshake로 TCP 연결 설정
 * - 논블로킹 모드로 설정되어 메인 스레드 블로킹 방지
 */
TcpSocket tcp_connect(const std::string& host, uint16_t port);

/**
 * TCP 데이터 송수신
 */

/**
 * 전체 데이터 송신 보장 - 버퍼 전체가 전송될 때까지 반복합니다
 *
 * @param s 송신할 소켓
 * @param data 송신 데이터
 * @param len 데이터 길이
 * @return 성공 여부
 *
 * TCP 특성:
 * - send()는 부분 전송 가능 (버퍼 가득참, 네트워크 지연 등)
 * - 논블로킹 모드에서 WOULDBLOCK 시 재시도
 * - 전체 데이터 전송 완료까지 반복 호출
 */
bool tcp_send_all(const TcpSocket& s, const void* data, size_t len);

/**
 * 수신 가능한 데이터 읽기 - 논블로킹으로 수신 버퍼에 추가합니다
 *
 * @param s 수신할 소켓
 * @param outBuf 누적 수신 버퍼 (이전 데이터에 추가됨)
 * @return 연결 유지 여부 (false면 연결 종료)
 *
 * TCP 스트림 특성:
 * - 메시지 경계가 없음 - "Hello" "World"가 "HelloWor" "ld"로 수신 가능
 * - 수신 버퍼에 누적하여 framing 레이어에서 메시지 경계 복원
 * - 논블로킹 모드에서 데이터 없으면 즉시 반환 (WOULDBLOCK)
 */
bool tcp_recv_some(const TcpSocket& s, std::vector<uint8_t>& outBuf);

/**
 * 소켓 정리 - 연결을 종료하고 리소스를 해제합니다
 *
 * TCP 연결 종료:
 * - close()/closesocket()는 4-way handshake 시작
 * - 상대방도 연결 종료를 확인해야 완전 종료
 * - 소켓 파일 디스크립터 무효화
 */
void tcp_close(TcpSocket& s);

/**
 * 로컬 IP 주소 조회 - 같은 네트워크 내 연결용
 *
 * @return 로컬 네트워크 IP 주소 (예: "192.168.1.100")
 */
std::string get_local_ip();

/**
 * 공인 IP 주소 조회 - 인터넷을 통한 연결용
 *
 * @return 공인 IP 주소 (예: "123.45.67.89") 또는 실패 시 빈 문자열
 *
 * 작동 원리:
 * - 외부 웹 서비스(ipify.org)에 HTTP 요청
 * - 응답에서 공인 IP 주소 파싱
 * - 네트워크 오류 시 빈 문자열 반환
 */
std::string get_public_ip();

}
