#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// 메시지 프레이밍: TCP 스트림에서 메시지 경계 구분
// 프레임 구조: [LEN:2][TYPE:1][PAYLOAD:LEN-1][CHECKSUM:4]
// 상세: ARCHITECTURE.md §7.2 (MsgType 표) 및 §11/§12 (메타 + 랭킹 흐름)

namespace net {

// 프레임 한계/헤더 필드 크기 — C++ 쪽 단일 진실 공급원(과거엔 framing.cpp 익명
// 네임스페이스에 있던 값을 공개 승격했고, server/relay.cpp 도 이 상수를 참조한다).
// 주의: 같은 값이 Python 미러(python/netbot/framing.py)에 중복돼 있고, 패리티
//   테스트(python/tests/test_framing_parity.py)가 이 값을 고정한다. 한쪽만 올리면
//   반대편 파서가 정상 프레임의 LEN 을 한도 초과로 보고 스트림 오염으로 오판해
//   연결을 끊으므로, 반드시 양쪽을 함께 바꿔야 한다.
// kMaxPayloadBytes 는 단순 메모리 최적화가 아니라 wire 보안 경계다: 악성 길이
//   선언을 받은 파서가 끝없이 body 를 기다리며 수신 버퍼를 키우는 것을 막는다.
//   정상 메시지 중 가장 큰 CHAT 도 이 한도 아래에 들어온다.
constexpr std::size_t kMaxPayloadBytes    = 4096;  // PAYLOAD 상한 (바이트)
constexpr std::size_t kFrameLenBytes      = 2;     // LEN 필드 (u16 LE)
constexpr std::size_t kFrameTypeBytes     = 1;     // TYPE 필드 (u8)
constexpr std::size_t kFrameChecksumBytes = 4;     // CHECKSUM 필드 (u32 LE, FNV-1a)

// 메시지 타입
enum class MsgType : uint8_t {
    HELLO = 1,
    HELLO_ACK = 2,
    SEED = 3,
    INPUT = 4,
    ACK = 5,
    PING = 6,
    PONG = 7,
    HASH = 8,
    GAME_OVER_CHOICE = 9,

    // 릴레이/매치메이킹 확장. 큐·룸 제어는 relay와 Session이 소비하고,
    // 매치 중 일반 게임 프레임은 전달한다. ranked MATCH_SUMMARY만 relay가
    // 결과 검증을 위해 가로챈다. SimGame은 이 제어 타입을 직접 소비하지 않는다.
    //
    // QUEUE_JOIN / ROOM_CREATE / ROOM_JOIN 은 모두 tetris_meta 인증 토큰을
    // 같이 실어 보낸다. 토큰은 32 hex chars (플랫폼 user-data 경로에 저장).
    // ranked relay(--meta)는 토큰이 없거나 검증에 실패하면 소켓을 close한다.
    // unranked relay(meta 없음)는 tok_len==0을 허용한다.
    QUEUE_JOIN    = 10,  // C→S : [tok_len:1][token:N]   (tok_len==0 이면 미인증)
    QUEUE_CANCEL  = 11,  // C→S : 빈 페이로드 (매치메이킹 큐 취소)
    MATCH_FOUND   = 12,  // S→C : [role:1][seed:8 LE][my_icon_len:1][my_icon:N]
                         //        [peer_icon_len:1][peer_icon:N][uuid_len:1][uuid:N]
                         //        role: 1=HOST, 2=GUEST. 구 클라이언트는 UUID를 무시한다.

    // 커스텀 룸
    //   플레이어가 5자리 코드로 방을 만들어 친구와 페어링.
    //   서버가 둘 다 Ready 상태를 확인하면 MATCH_FOUND 로 릴레이 경로에 진입.
    ROOM_CREATE = 13,  // C→S : [tok_len:1][token:N]
    ROOM_JOIN   = 14,  // C→S : [code_len:1][code:N][tok_len:1][token:N]
    ROOM_INFO   = 15,  // S→C : [code_len:1][code:N][status:1][peer_count:1]
                       //   status: 0=waiting 1=full 2=notfound 3=gonefull(상대 퇴장)
    ROOM_LEAVE  = 16,  // C→S : 빈 페이로드
    READY       = 17,  // C→S, S→C(forward) : [ready:1]  (1=ready, 0=not)

    // 메타데이터/RP 연동. relay가 MATCH_SUMMARY를 가로채 결과를 검증하고,
    // meta의 POST /v1/matches 응답을 MATCH_RESULT로 돌려준다.
    MATCH_SUMMARY = 18,  // C→S : [won:1][my_score:4 LE][my_lines:4 LE]
                         //        [opp_score_observed:4 LE][opp_lines_observed:4 LE]
                         //        [duration_s:4 LE]  (총 21 바이트)
    MATCH_RESULT  = 19,  // S→C : [elo_before:4 LE][elo_after:4 LE][delta:4 LE signed]
                         //   필드명 elo_* 는 하위 호환용. 값은 RP이며 delta=0은 무변동.

    CHAT        = 20,  // 양방향 : [text_len:2 LE][utf8:N] (릴레이가 통과 포워딩)

    // 서버가 연결을 거절하며 사유를 밝힌다.
    //   S→C : [reason:1][text_len:1][utf8:N]
    // 상한(전역 연결 수·per-IP·전역 tx 예산)에 걸린 연결은 예전에는 소켓이
    // 그냥 닫혔다. 사용자에게는 원인 모를 끊김이고, 문의가 와도 서버 로그와
    // 맞춰 보기 전에는 아무 말도 못 했다. 이 프레임을 먼저 내려보내면
    // 클라이언트가 "서버 만원" 같은 문구를 띄울 수 있다.
    //
    // 하위 호환: 이 타입을 모르는 구버전 클라이언트도 안전하다. C++ 파서
    //   (net::parse_frames)는 타입을 해석하지 않고 그대로 올려 주며, Session 의
    //   디스패치는 default 로 흘려 무시한다. 큐/룸 대기 루프도 자기가 찾는
    //   타입이 아니면 건너뛴다. Python 미러(netbot/framing.py)는 모르는 타입을
    //   그 프레임만 소비하고 계속 읽는다. 어느 쪽이든 프레임을 무시한 뒤
    //   소켓 종료를 관측하므로, 구버전에서의 동작은 예전과 정확히 같다
    //   (조용한 끊김). 새 클라이언트만 사유를 읽는다.
    //
    // 전달은 최선의 노력이다: 거절은 소켓을 닫기 직전에 일어나므로, 커널이
    //   RST 를 보내는 상황에서는 이 프레임이 유실될 수 있다. 그래서 클라이언트는
    //   이 프레임이 오지 않는 경우에도 기존의 일반 문구로 물러설 수 있어야 한다.
    SERVER_REJECT = 21,
};

// 서버만 만들 수 있는 프레임인가 — 릴레이가 포워딩 경로에서 버릴 대상.
//
// 판단 근거는 위 MsgType 표의 방향 주석이다. "S→C" 로 적힌 타입은 서버가
// 만들어 내려보내는 것이고, 클라이언트가 같은 타입을 올려보낼 자리는 프로토콜
// 어디에도 없다. 상대 클라이언트는 이 타입이 오면 서버가 보냈다고 믿는데,
// 릴레이가 포워딩 중에 그대로 흘려보내면 그 믿음을 상대 플레이어가 위조할 수
// 있게 된다 — 없던 RP 변동을, 없던 매치·룸 상태를 주입하는 길이다.
//
//   MATCH_FOUND   — 짝을 지은 것은 큐를 든 서버뿐이다.
//   ROOM_INFO     — 룸 코드·정원·상태를 아는 것은 룸 표를 든 서버뿐이다.
//   MATCH_RESULT  — RP 변동은 meta 가 계산해 서버가 내려 준다.
//   SERVER_REJECT — "서버가 너를 거절했다" 를 클라이언트가 말할 수는 없다.
//
// 여기 없는 것들의 근거도 같은 표다.
//   · HELLO / HELLO_ACK / SEED / INPUT / ACK / PING / PONG / HASH /
//     GAME_OVER_CHOICE 는 릴레이가 두 클라이언트 사이로 흘려보내는 락스텝
//     프레임이다. 이름과 달리 HELLO_ACK 은 서버가 아니라 상대 피어가 보내는
//     응답이고(net/session.cpp 의 HELLO 처리), 막으면 매치가 시작되지 않는다.
//   · READY 는 표가 "C→S, S→C(forward)" 라고 못 박는다 — 릴레이가 중계하는
//     것이 정상 동작이다.
//   · CHAT 은 양방향이다.
//   · MATCH_SUMMARY 는 C→S 라 클라이언트가 보내는 것이 맞다. 랭크드 릴레이가
//     이것을 가로채는 이유는 "서버 전용이라서" 가 아니라 결과 교차검증에 쓰려고
//     소비하기 때문이므로, 이 목록과는 성격이 다르다.
//
// 위반한 프레임 하나만 버리고 연결은 살린다. 포워딩은 양방향이라 여기서 연결을
// 끊으면 위조한 쪽만이 아니라 상대의 경기까지 함께 끝난다 — 한 사람의 반칙으로
// 무관한 사람의 판을 깨는 것은 이 프레임들을 막아서 지키려던 것과 같은 손해다.
// 버리기만 해도 공격자가 얻는 것은 없다.
constexpr bool is_server_only_type(uint8_t type) {
    return type == static_cast<uint8_t>(MsgType::MATCH_FOUND)  ||
           type == static_cast<uint8_t>(MsgType::ROOM_INFO)    ||
           type == static_cast<uint8_t>(MsgType::MATCH_RESULT) ||
           type == static_cast<uint8_t>(MsgType::SERVER_REJECT);
}

// SERVER_REJECT 의 reason 코드. 값은 wire 규약이므로 재사용/재번호 금지 —
// 새 사유는 뒤에 덧붙인다. 모르는 코드를 받은 클라이언트는 함께 실린 텍스트를
// 쓰거나 일반 문구로 물러선다.
enum class RejectReason : uint8_t {
    ServerFull       = 1,  // 프로세스 전체 동시 연결 상한
    IpSessionLimit   = 2,  // per-IP 동시 세션 상한
    IpHandshakeLimit = 3,  // per-IP 동시 핸드셰이크 상한
    TxBudget         = 4,  // 프로세스 전체 보류 송신 예산
    AuthBacklog      = 5,  // 대기 중인 meta 인증 왕복 상한
    RoomGuessLimit   = 6,  // per-IP 룸 코드 오답 예산 소진
    QueueNoShow      = 7,  // per-IP 매칭 후 무응답 예산 소진
};

// 파싱된 메시지 프레임
struct Frame {
    MsgType type;
    std::vector<uint8_t> payload;
};

// FNV-1a 32-bit 해시 (체크섬용)
uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t seed=2166136261u);

// 스트림 파싱: 누적 버퍼에서 완성된 프레임들 추출 (부분 수신 처리)
bool parse_frames(std::vector<uint8_t>& streamBuf, std::vector<Frame>& out);

// 메시지 직렬화: TYPE + PAYLOAD → 프레임 바이트 배열
std::vector<uint8_t> build_frame(MsgType t, const std::vector<uint8_t>& payload);

// 리틀엔디안 직렬화/역직렬화
void le_write_u16(std::vector<uint8_t>& v, uint16_t x);
void le_write_u32(std::vector<uint8_t>& v, uint32_t x);
void le_write_u64(std::vector<uint8_t>& v, uint64_t x);
uint16_t le_read_u16(const uint8_t* p);
uint32_t le_read_u32(const uint8_t* p);
uint64_t le_read_u64(const uint8_t* p);

}
