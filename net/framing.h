#pragma once
#include <cstdint>
#include <vector>

/**
 * 메시지 프레이밍 시스템
 *
 * TCP는 바이트 스트림이므로 메시지 경계가 없습니다. 프레이밍은 스트림에서
 * 개별 메시지를 구분하는 기법입니다.
 *
 * 프레임 구조 (리틀엔디안):
 * ┌────────┬────────┬─────────────┬──────────┐
 * │ LEN    │ TYPE   │ PAYLOAD     │ CHECKSUM │
 * │ 2bytes │ 1byte  │ LEN-1 bytes │ 4bytes   │
 * └────────┴────────┴─────────────┴──────────┘
 *
 * - LEN: TYPE(1) + PAYLOAD 길이 (최대 65535)
 * - TYPE: 메시지 타입 (HELLO, INPUT 등)
 * - PAYLOAD: 메시지 내용 (가변 길이)
 * - CHECKSUM: PAYLOAD의 FNV-1a 해시 (무결성 검증)
 *
 * 프레이밍이 필요한 이유:
 * - TCP 스트림에서 "Hello" + "World" 전송 시
 * - 수신측에서 "Hell" + "oWorld" 또는 "HelloWorld"로 도착 가능
 * - 프레임 헤더로 메시지 경계를 명확히 구분
 *
 * 학습 포인트:
 * - 네트워크 바이트 순서 (엔디안) 통일의 중요성
 * - 부분 수신 처리 (프레임이 여러 recv()에 걸쳐 도착)
 * - 무결성 검사로 네트워크 오류 감지
 */

namespace net {

/**
 * 게임 프로토콜 메시지 타입
 *
 * 멀티플레이어 게임에서 주고받는 메시지들:
 * - 연결 설정: HELLO, HELLO_ACK, SEED
 * - 게임 데이터: INPUT (플레이어 입력)
 * - 신뢰성: ACK (수신 확인)
 * - 연결 관리: PING, PONG (연결 확인)
 * - 디버깅: HASH (게임 상태 동기화 확인)
 */
enum class MsgType : uint8_t {
    HELLO = 1,      // 연결 초기화 메시지
    HELLO_ACK = 2,  // HELLO 응답
    SEED = 3,       // 게임 시드 공유 (RNG 동기화용)
    INPUT = 4,      // 플레이어 입력 데이터
    ACK = 5,        // 수신 확인 응답
    PING = 6,       // 연결 상태 확인
    PONG = 7,       // PING 응답
    HASH = 8,       // 게임 상태 해시 (디버깅용)
};

/**
 * 파싱된 메시지 프레임
 *
 * 원시 바이트에서 추출한 구조화된 메시지
 * payload는 메시지 타입별로 다른 구조를 가집니다.
 */
struct Frame {
    MsgType type;                    // 메시지 타입
    std::vector<uint8_t> payload;    // 메시지 내용
};

/**
 * FNV-1a 해시 함수 - 빠른 무결성 검사용
 *
 * @param data 해시할 데이터
 * @param len 데이터 길이
 * @param seed 초기값 (FNV-1a 표준값)
 * @return 32비트 해시값
 *
 * FNV-1a 특징:
 * - 단순하고 빠른 비암호화 해시
 * - 작은 데이터에 대해 좋은 분산 특성
 * - 네트워크 오류나 메시지 손상 감지용
 */
uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed=2166136261u);

/**
 * 스트림에서 완성된 프레임들을 파싱
 *
 * @param streamBuf 누적된 수신 바이트 버퍼 (입출력 겸용)
 * @param out 파싱된 프레임들을 추가할 배열
 * @return 파싱 성공 여부
 *
 * 스트림 파싱 과정:
 * 1. LEN 필드(2바이트)를 읽을 수 있는지 확인
 * 2. 전체 프레임이 도착했는지 확인 (LEN + 체크섬 크기)
 * 3. 체크섬 검증
 * 4. 유효한 프레임이면 out에 추가
 * 5. 파싱된 바이트는 streamBuf에서 제거
 *
 * 부분 수신 처리:
 * - 프레임이 불완전하면 streamBuf에 그대로 둠
 * - 다음 recv()로 더 많은 바이트가 오면 재시도
 */
bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out);

/**
 * 메시지를 프레임으로 직렬화
 *
 * @param t 메시지 타입
 * @param payload 메시지 내용
 * @return 송신 준비된 바이트 배열
 *
 * 직렬화 과정:
 * 1. LEN 계산 (TYPE 1바이트 + PAYLOAD 크기)
 * 2. LEN을 리틀엔디안으로 직렬화
 * 3. TYPE 바이트 추가
 * 4. PAYLOAD 복사
 * 5. PAYLOAD의 FNV-1a 해시 계산 후 추가
 */
std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload);

/**
 * 리틀엔디안 직렬화/역직렬화 헬퍼
 *
 * 네트워크 통신에서 바이트 순서(엔디안) 통일이 중요합니다:
 * - 빅엔디안: 상위 바이트부터 저장 (네트워크 바이트 순서)
 * - 리틀엔디안: 하위 바이트부터 저장 (x86 CPU 기본값)
 *
 * 이 프로젝트는 리틀엔디안을 사용하여 x86 플랫폼에서 효율적입니다.
 * 실제 게임에서는 빅엔디안이 더 일반적일 수 있습니다.
 */

// 정수를 리틀엔디안 바이트로 변환하여 벡터에 추가
void le_write_u16(std::vector<uint8_t>& v, uint16_t x);
void le_write_u32(std::vector<uint8_t>& v, uint32_t x);
void le_write_u64(std::vector<uint8_t>& v, uint64_t x);

// 리틀엔디안 바이트에서 정수로 변환
uint16_t le_read_u16(const uint8_t* p);
uint32_t le_read_u32(const uint8_t* p);
uint64_t le_read_u64(const uint8_t* p);

}
