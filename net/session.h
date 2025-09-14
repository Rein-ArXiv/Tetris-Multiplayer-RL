#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <string>
#include "socket.h"
#include "framing.h"

/**
 * P2P Lockstep 네트워크 세션
 *
 * 이 클래스는 결정론적 멀티플레이어 게임을 위한 1:1 네트워크 세션을 구현합니다.
 *
 * Lockstep 동기화란?
 * - 모든 플레이어의 입력이 도착할 때까지 게임 시뮬레이션을 대기
 * - 입력을 받은 후에야 다음 틱으로 진행 (결정론 보장)
 * - 네트워크 지연만큼 입력 지연이 발생하지만 완벽한 동기화 가능
 *
 * 역할 분담:
 * - 메인 스레드: 게임 로직 실행, 입력 생성
 * - I/O 스레드: 네트워크 송수신, 메시지 파싱
 * - Accept 스레드: 호스트에서 클라이언트 연결 대기
 *
 * 핸드셰이크 과정:
 * 1. 호스트가 포트에서 대기, 클라이언트가 연결
 * 2. HELLO 메시지 교환 (프로토콜 버전 확인)
 * 3. 호스트가 SEED 메시지 전송 (게임 파라미터 공유)
 * 4. 양쪽 모두 ready 상태가 되면 게임 시작
 *
 * 게임 진행:
 * - 매 틱마다 양쪽이 INPUT 메시지 교환
 * - 안전 틱까지만 시뮬레이션 진행 (입력 지연 고려)
 * - 주기적으로 상태 해시 비교 (동기화 검증)
 */

namespace net {

/**
 * 네트워크 역할 구분
 *
 * Host: 서버 역할, 게임 파라미터 결정
 * Peer: 클라이언트 역할, 호스트 파라미터 수용
 */
enum class Role : uint8_t { Host=1, Peer=2 };

/**
 * 게임 시작 파라미터
 *
 * 결정론적 게임을 위해 모든 플레이어가 동일한 조건으로 시작해야 합니다.
 * 호스트가 이 값들을 결정하고 SEED 메시지로 클라이언트에 전달합니다.
 */
struct SeedParams {
    uint64_t seed{0};           // RNG 초기 시드 (블록 순서 결정)
    uint32_t start_tick{120};   // 시작 전 대기 틱 수 (연결 안정화)
    uint8_t input_delay{2};     // 입력 지연 틱 수 (네트워크 지연 버퍼) - 2틱으로 감소
    Role role{Role::Host};      // 네트워크 역할
};

class Session {
public:
    Session();
    ~Session();

    /**
     * 네트워크 연결 설정
     */

    /**
     * 호스트 모드 시작 - 클라이언트 연결을 기다립니다
     *
     * @param port 대기할 포트 번호
     * @param sp 게임 시작 파라미터 (클라이언트에게 전송됨)
     * @return 성공 여부
     *
     * 호스트의 역할:
     * - 네트워크 서버 역할 (포트 바인딩, 연결 수락)
     * - 게임 파라미터 결정권 (시드, 지연 등)
     * - 연결 후 즉시 HELLO, SEED 메시지 전송
     */
    bool Host(uint16_t port, const SeedParams& sp);

    /**
     * 클라이언트 모드 시작 - 호스트에 연결을 시도합니다
     *
     * @param host 호스트 주소 (IP 또는 도메인)
     * @param port 호스트 포트
     * @return 성공 여부
     *
     * 클라이언트의 역할:
     * - 호스트에 연결 요청
     * - HELLO 메시지 전송, SEED 메시지 수신 대기
     * - 호스트가 정한 게임 파라미터 수용
     */
    bool Connect(const std::string& host, uint16_t port);

    /**
     * 세션 상태 조회
     *
     * 메인 게임 루프에서 네트워크 상태에 따라 다른 화면을 표시합니다.
     */
    bool isConnected() const { return connected; }  // TCP 연결 완료
    bool isReady() const { return ready; }          // 핸드셰이크 완료, 게임 시작 가능
    bool isListening() const { return listening; }  // 호스트 모드에서 연결 대기 중
    bool hasFailed() const { return connectionFailed; } // 연결 실패 여부
    SeedParams params() const { return seedParams; } // 협상된 게임 파라미터

    /**
     * 게임 데이터 송신
     */

    /**
     * 플레이어 입력을 상대방에게 전송
     *
     * @param tick 입력이 적용될 틱 번호
     * @param mask 입력 비트마스크 (좌/우/아래/회전/드롭)
     *
     * 입력 전송 타이밍:
     * - 매 틱마다 호출 (입력이 없어도 0으로 전송)
     * - 네트워크 스레드가 비동기로 실제 전송 처리
     * - TCP 신뢰성으로 순서와 도착 보장
     */
    void SendInput(uint32_t tick, uint8_t mask);

    /**
     * 게임 상태 해시 전송 (동기화 검증용)
     *
     * @param tick 해시를 계산한 틱
     * @param hash 게임 상태 해시값
     *
     * 디버깅/검증 목적:
     * - 양쪽 게임 상태가 일치하는지 확인
     * - desync 발생 시점 정확히 파악
     * - 결정론 버그 디버깅에 필수
     */
    void SendHash(uint32_t tick, uint64_t hash);

    /**
     * 게임 데이터 수신
     */

    /**
     * 상대방 입력 조회
     *
     * @param tick 조회할 틱 번호
     * @param outMask 입력 비트마스크 반환값
     * @return 해당 틱 입력이 도착했는지 여부
     *
     * Lockstep 사용법:
     * - 틱 N을 진행하기 전에 양쪽 입력 모두 확인
     * - 상대방 입력이 아직 도착하지 않았으면 대기
     * - 안전 틱 계산으로 어느 틱까지 진행 가능한지 판단
     */
    bool GetRemoteInput(uint32_t tick, uint8_t& outMask);

    /**
     * 상대방이 전송한 최근 상태 해시 조회
     *
     * @param tick 해시 틱 번호 반환값
     * @param hash 해시값 반환값
     * @return 해시 데이터 존재 여부
     */
    bool GetLastRemoteHash(uint32_t& tick, uint64_t& hash) const;

    /**
     * 틱 진행 상황 조회 (안전 틱 계산용)
     *
     * Lockstep에서 "안전 틱"은 양쪽 입력이 모두 도착한 마지막 틱입니다.
     * 안전 틱 = min(로컬 전송 완료 틱, 원격 수신 최신 틱) - 입력 지연
     */
    uint32_t maxRemoteTick() const { return lastRemoteTick; }  // 상대방이 보낸 최신 틱
    uint32_t maxLocalTick() const { return lastLocalTick; }    // 내가 보낸 최신 틱

    /**
     * 세션 종료 - 연결을 정리하고 스레드를 종료합니다
     *
     * 정리 과정:
     * - I/O 스레드와 Accept 스레드 중단 신호
     * - 소켓 종료로 블로킹 호출 해제
     * - 스레드 조인으로 완전 종료 대기
     */
    void Close();

private:
    /**
     * 내부 스레드 함수들
     */

    /**
     * I/O 스레드 메인 루프
     *
     * 역할:
     * - TCP 소켓에서 데이터 수신 (논블로킹)
     * - 프레이밍으로 메시지 경계 복원
     * - 메시지 타입별 처리 (handleFrame 호출)
     * - 송신 큐의 데이터를 TCP로 전송
     *
     * 논블로킹 I/O:
     * - recv()에서 데이터 없으면 즉시 반환
     * - 메인 스레드 블로킹 없이 백그라운드 처리
     * - CPU 절약을 위해 활동 없으면 짧은 대기
     */
    void ioThread();

    /**
     * 수신 메시지 처리기
     *
     * @param f 파싱된 메시지 프레임
     *
     * 메시지별 처리:
     * - HELLO/HELLO_ACK: 연결 확인 응답
     * - SEED: 게임 파라미터 수신 (클라이언트만)
     * - INPUT: 상대방 입력을 맵에 저장
     * - HASH: 상태 해시 수신 및 저장
     * - PING/PONG: 연결 상태 확인
     */
    void handleFrame(const Frame& f);

    /**
     * 호스트 연결 대기 스레드 (호스트 모드 전용)
     *
     * @param port 대기할 포트
     *
     * Accept 과정:
     * - tcp_listen()으로 대기 소켓 생성
     * - tcp_accept()로 클라이언트 연결 대기 (블로킹)
     * - 연결 완료 시 I/O 스레드 시작
     * - HELLO, SEED 메시지 즉시 전송
     */
    void acceptThread(uint16_t port);

    /**
     * 네트워크 리소스
     */
    TcpSocket sock{};        // 메인 통신 소켓 (양방향 데이터)
    TcpSocket listenSock{};  // 호스트 대기 소켓 (연결 수락용)

    /**
     * 스레드 관리
     */
    std::thread th;          // I/O 스레드 핸들
    std::thread ath;         // Accept 스레드 핸들 (호스트만)

    /**
     * 스레드 간 상태 공유 (atomic으로 동기화)
     */
    std::atomic<bool> quit{false};       // 스레드 종료 신호
    std::atomic<bool> connected{false};  // TCP 연결 상태
    std::atomic<bool> ready{false};      // 핸드셰이크 완료 상태
    std::atomic<bool> listening{false};  // 호스트 대기 상태
    std::atomic<bool> connectionFailed{false}; // 연결 실패 상태

    SeedParams seedParams{};             // 협상된 게임 파라미터

    /**
     * I/O 데이터 버퍼
     */
    std::vector<uint8_t> recvBuf;        // 수신 누적 버퍼 (프레이밍용)

    /**
     * 송신 큐 (I/O 스레드에서 비워짐)
     */
    std::mutex sendMu;                           // 송신 큐 접근 동기화
    std::deque<std::vector<uint8_t>> sendQ;      // 전송 대기 메시지들

    /**
     * 수신 데이터 저장소
     */
    std::mutex inMu;                                      // 입력 맵 접근 동기화
    std::unordered_map<uint32_t, uint8_t> remoteInputs;   // 틱별 상대방 입력
    std::atomic<uint32_t> lastRemoteTick{0};              // 상대방 최신 입력 틱
    std::atomic<uint32_t> lastLocalTick{0};               // 내 최신 전송 틱

    // 디버깅용 상태 해시 저장
    std::atomic<uint32_t> lastHashTickRemote{0};          // 상대방 해시 틱
    std::atomic<uint64_t> lastHashRemote{0};              // 상대방 해시값
};

}
