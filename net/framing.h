#pragma once
#include <cstdint>
#include <vector>

// 메시지 프레이밍: TCP 스트림에서 메시지 경계 구분
// 프레임 구조: [LEN:2][TYPE:1][PAYLOAD:LEN-1][CHECKSUM:4]
// 상세: ARCHITECTURE.md §7.2 (MsgType 표) 및 §11/§12 (메타 + 랭킹 흐름)

namespace net {

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

    // 릴레이/매치메이킹 확장 (클라 ↔ 릴레이 서버 간에만 사용 — 릴레이가
    // MATCH_FOUND 를 보낸 후에는 투명하게 바이트 스트림만 포워딩하므로
    // 게임 루프는 이 두 타입을 직접 소비하지 않는다.)
    //
    // QUEUE_JOIN / ROOM_CREATE / ROOM_JOIN 은 모두 tetris_meta 인증 토큰을
    // 같이 실어 보낸다. 토큰은 32 hex chars (플랫폼 user-data 경로에 저장).
    // 토큰이 없거나 relay 가 meta 에 연결 못 하면 소켓을 즉시 close.
    QUEUE_JOIN    = 10,  // C→S : [tok_len:1][token:N]   (tok_len==0 이면 미인증)
    QUEUE_CANCEL  = 11,  // C→S : 빈 페이로드 (매치메이킹 큐 취소)
    MATCH_FOUND   = 12,  // S→C : [role:1][seed:8 LE]  role: 1=HOST, 2=GUEST

    // 커스텀 룸 (Section D)
    //   플레이어가 5자리 코드로 방을 만들어 친구와 페어링.
    //   서버가 둘 다 Ready 상태를 확인하면 MATCH_FOUND 로 기존 릴레이 경로 진입.
    ROOM_CREATE = 13,  // C→S : [tok_len:1][token:N]
    ROOM_JOIN   = 14,  // C→S : [code_len:1][code:N][tok_len:1][token:N]
    ROOM_INFO   = 15,  // S→C : [code_len:1][code:N][status:1][peer_count:1]
                       //   status: 0=waiting 1=full 2=notfound 3=gonefull(상대 퇴장)
    ROOM_LEAVE  = 16,  // C→S : 빈 페이로드
    READY       = 17,  // C→S, S→C(forward) : [ready:1]  (1=ready, 0=not)

    // Section K — 메타데이터/ELO 연동. 투명 릴레이 구간이 아니라 relay 가
    // 가로채서 meta 로 POST /v1/matches 를 날리고 MATCH_RESULT 로 돌려준다.
    MATCH_SUMMARY = 18,  // C→S : [won:1][my_score:4 LE][my_lines:4 LE]
                         //        [opp_score_observed:4 LE][opp_lines_observed:4 LE]
                         //        [duration_s:4 LE]  (총 21 바이트)
    MATCH_RESULT  = 19,  // S→C : [elo_before:4 LE][elo_after:4 LE][delta:4 LE signed]
                         //   delta=0 이면 ranking offline (meta 장애).

    CHAT        = 20,  // 양방향 : [text_len:2 LE][utf8:N] (릴레이가 통과 포워딩)
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
