// server/reactor_relay.cpp — 단일 reactor 루프로 도는 릴레이 (실험용 바이너리)
//
// 스레드 모델(tetris_relay)과 같은 wire 프로토콜을 구현하되, 연결마다 스레드를 두는
// 대신 한 스레드가 net::Reactor 로 모든 소켓의 준비성을 기다린다. 스레드 모델에서
// 각 루프가 하던 일은 여기서 "연결 상태 객체 + 이벤트 핸들러"가 된다.
//
//   accept 루프            → listen fd 의 readable 핸들러
//   playerConnThread       → Stage::FirstFrame 핸들러 (+ 인증은 오프로드)
//   matcher 스레드          → 큐에 두 명이 차는 순간 루프가 직접 페어링
//   queueLobbyThread       → Stage::Lobby 핸들러
//   forwarderLoop × 2      → 양쪽 Conn 의 readable 핸들러 하나씩
//
// 단일 스레드가 모든 소켓을 소유하므로 스레드 모델의 동기화가 대부분 사라진다 —
// 방향별 send mutex, 요약 수집 mutex, forwarder_count/disconnect_side atomic,
// 그리고 "양방향이 동시에 죽을 때 교차검증이 생략되는" 경합까지. 남은 락은 오프로드
// 워커와 공유하는 지점(인증 캐시, 세션 lease)뿐이다.
//
// 샤딩(--loops N): 매치메이킹 큐와 룸 코드 표는 본질적으로 전역이라 루프마다 복제할
// 수 없다 — 서로 다른 루프의 큐에 선 두 사람은 영영 만나지 못하고, 한 루프에서 발급한
// 룸 코드는 다른 루프에서 "없는 방"이 된다. 그래서 나누는 축을 연결이 아니라 매치로
// 잡는다: 앞단 루프 하나가 accept·인증·큐·룸·로비를 전부 소유하고(연결 수명당 몇 번뿐인
// 값싼 일), 포워딩이 시작되는 순간 두 소켓과 채널을 포워딩 샤드로 넘긴다(패킷마다 도는
// 비싼 일). 샤드는 서로 아무것도 공유하지 않으므로 락이 필요 없다 — 유일한 락은 넘겨줄
// 때 쓰는 우편함이다.
//
// 루프 클래스는 하나뿐이다. 앞단이냐 샤드냐는 리스너를 가졌는지의 차이일 뿐이다.
//
// 룸 경로는 스레드 모델의 starter/exit 조건변수 배리어가 통째로 사라진다. 그 배리어는
// 두 스레드가 같은 fd 를 동시에 읽지 않게 하려고 있었는데, 소켓 소유자가 하나뿐이면
// 애초에 막을 것이 없다 — 양쪽 READY 를 관측한 그 자리에서 곧바로 포워딩으로 넘긴다.

#include "../net/framing.h"
#include "../net/reactor.h"
#include "../net/socket.h"
#include "../meta/http_client.h"
#include "ip_admission.h"
#include "log.h"
#include "match_uuid.h"
#include "offload.h"
#include "player_session.h"
#include "timer_queue.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace relay {

// 동시 연결 상한. 스레드 모델의 512 는 자원 판단이 아니라 구조의 산물이었다 —
// 포워딩 워커 512개를 매치당 2개씩 쓰니 연결도 512에서 멎었다. 루프 모델에는
// 그 제약이 없으므로 값을 물려받을 이유도 없다.
//
// 기본값 4096 의 근거는 실측이다. 이 저장소의 벤치(2.0GHz Sandy Bridge, 8스레드)
// 에서 60 tick/s 부하 기준 접속자당 앞단 루프 비용이 약 0.14% 코어였다 — 500명이
// 코어 하나의 68% 다. 단일 루프는 스레드 하나이므로 코어 하나가 곧 천장이고,
// 그 천장은 700명 언저리에 있다. 4096 은 거기서 더 여유를 둔 값이라 이 상한이
// 먼저 걸리는 일은 없다: 실제로 먼저 오는 것은 루프 포화이고, 그때 늘릴 것은
// 이 값이 아니라 --loops 다.
//
// 그래도 상한 자체는 남긴다. 무한대는 상한이 아니라 상한의 부재이고, fd 고갈이나
// 메모리 압박이 거절보다 나은 실패 모드인 적은 없다. 운영자가 자기 하드웨어에
// 맞춰 정할 수 있도록 --max-conns 로 노출한다.
constexpr size_t kDefaultMaxConns = 4096;
size_t           g_max_conns      = kDefaultMaxConns;

// 보류 송신(tx)의 프로세스 전체 예산.
//
// 연결당 상한만으로는 메모리를 보장하지 못한다. 두 값이 곱해지기 때문이다 —
// 연결당 1 MiB 에 연결 4096 개면 최악이 4 GiB 다. 상한을 올릴 때마다 메모리
// 천장이 조용히 따라 오르고, --max-conns 를 만지는 운영자에게는 그 곱셈이 보이지
// 않는다. 커널이 소켓당 wmem 과 별개로 전역 tcp_mem 을 두는 이유와 같다: 연결당
// 상한은 "언제 이 상대를 밀어낼지" 를 정하고, 전역 예산은 "언제 프로세스가
// 위험한지" 를 정한다. 역할이 달라서 하나가 다른 하나를 대신하지 못한다.
//
// 예산을 넘기면 그 순간 넘긴 연결을 끊는다. 가장 많이 쌓인 연결을 골라 끊는 편이
// 더 공정하지만, 그러려면 샤드 스레드들의 표를 가로질러 훑어야 한다. 연결당
// 상한이 이미 한 연결의 몫을 좁게 묶어 두므로, 예산을 넘길 만큼 쌓는 쪽이 곧
// 원인일 가능성이 높다 — 단순한 정책으로 충분하다.
//
// 수신 버퍼(rx)는 예산에 넣지 않는다. 매 읽기마다 상한을 검사해 초과 시 연결을
// 끊으므로 최악이 이미 확정적이다(연결당 64 KiB × --max-conns). 무한정 자랄 수
// 있었던 쪽은 tx 뿐이고, 예산이 필요한 것도 그쪽이다.
constexpr size_t      kDefaultTxBudget = 64 * 1024 * 1024;
size_t                g_tx_budget      = kDefaultTxBudget;
std::atomic<size_t>   g_tx_total{0};

// 대기 중인 meta 인증 왕복의 상한.
//
// 인증은 루프 밖 워커 4개가 도는 유일한 블로킹 구간이라, 여기 줄이 서면 그
// 줄은 곧 로그인 대기 시간이다. 죽은 연결의 작업은 취소되므로(close_conn) 이
// 상한이 세는 것은 "아직 살아서 응답을 기다리는 사람" 뿐이고, 그래서 값은
// 그 사람들이 얼마나 기다려도 되는지로 정한다.
//
// 배포 대상의 meta 는 성능이 매우 제한된 보조 기기라 왕복이 수십~수백 ms 다.
// 왕복 250 ms, 동시 4개 기준으로 64번째 사람은 16번의 왕복, 약 4초를 기다린다 —
// 클라이언트의 접속 타임아웃과 같은 자릿수라, 여기까지 받아 준 사람은 실제로
// 응답을 볼 가망이 있다. 그 뒤에 오는 사람을 줄에 세우면 기다린 끝에 자기
// 클라이언트가 먼저 포기하므로, 세우는 대신 SERVER_REJECT 로 지금 사유를
// 밝히고 보낸다. 거절된 쪽은 다시 시도하면 되고, 줄은 초당 4/왕복(≈16명)씩
// 빠진다.
//
// 반대편 논거도 분명하다: 재시작 직후처럼 정상 사용자가 한꺼번에 몰리면 64를
// 넘긴 사람들이 거절을 받는다. 그래도 조용히 몇십 초를 기다리게 하는 것보다는
// 낫고(클라이언트가 "잠시 후 다시" 를 띄울 수 있다), meta 가 더 빠른 환경에
// 있는 운영자는 --max-pending-auth 로 올릴 수 있다. 워커 수를 대신 늘리지
// 않은 이유는 그쪽이 보조 기기에 동시 요청을 더 밀어 넣어 왕복 자체를 느리게
// 만들기 때문이다.
constexpr size_t      kDefaultMaxPendingAuth = 64;
size_t                g_max_pending_auth     = kDefaultMaxPendingAuth;

// ── 관측 ─────────────────────────────────────────────────────────────────────
// 관측할 수 없는 예산은 운영도 검증도 못 한다. 실제로 그랬다: tx 예산을 도입한
// 직후 회귀 테스트를 쓰려 했는데, 바깥에서 보이는 것이 "접속이 되는가" 뿐이라
// 반납 회계를 통째로 없앤 바이너리가 테스트를 그대로 통과했다. 예산이 바닥나도
// accept 는 계속 되고 막히는 것은 버퍼링뿐이라 증상이 밖으로 안 나오기 때문이다.
//
// 그래서 카운터를 프로세스 전역으로 두고 주기적으로 한 줄에 찍는다. 전역인
// 이유는 샤드 때문이다 — 루프마다 자기 표만 세면 포워딩으로 넘어간 연결이
// 앞단의 수에서 사라져, 어느 줄도 프로세스의 실제 상태를 말하지 못한다.
std::atomic<size_t>   g_conn_count{0};   // 현재 동시 연결 (프로세스 전체)
std::atomic<size_t>   g_match_count{0};  // 현재 활성 매치 (프로세스 전체)
std::atomic<size_t>   g_tx_peak{0};      // tx 사용량의 최고 수위 (예산 여유 판단용)

// 거절 카운터 — 사유별로 나눠야 "무엇이 먼저 걸리는가" 를 말할 수 있다.
// 합계만 있으면 상한을 어느 쪽으로 올려야 하는지 알 수 없다.
std::atomic<uint64_t> g_reject_conn_cap{0};
std::atomic<uint64_t> g_reject_ip_session{0};
std::atomic<uint64_t> g_reject_ip_handshake{0};
std::atomic<uint64_t> g_reject_tx_budget{0};
std::atomic<uint64_t> g_reject_auth_backlog{0};

// 상태 한 줄의 주기.
//
// 10초를 고른 근거는 양쪽 실패 모드다.
//   · 너무 길면(60초+) 정작 필요한 순간에 해상도가 없다. tx 예산 누수나 상한
//     충돌은 몇 초 만에 상태가 바뀌고, 문의 대응은 "그 시각" 에서 출발한다.
//     한 라운드가 10초 남짓인 회귀 테스트도 60초 주기로는 표본을 못 얻는다.
//   · 너무 짧으면(1초) 로그가 다른 모든 줄을 덮는다. 이 릴레이의 나머지 로그는
//     연결 수명당 몇 줄뿐이라, 초당 한 줄이면 한산한 서버의 로그가 사실상
//     상태 줄만 남는다.
// 10초는 하루 8,640줄 — 접속·매치 로그와 같은 자릿수라 어느 쪽도 덮지 않고,
// 저전력 쿼드코어에서 10초에 한 번의 문자열 조립은 측정 대상이 아니다.
// 조사·테스트용으로 --stats-interval-sec 로 조절할 수 있고 0 이면 끈다.
constexpr int         kDefaultStatsIntervalSec = 10;
int                   g_stats_interval_sec     = kDefaultStatsIntervalSec;

namespace {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

// 스레드 모델과 같은 값을 쓴다 — 두 바이너리의 동작이 갈리지 않게.
constexpr auto   kFirstFrameTimeout = std::chrono::seconds(5);
constexpr auto   kLobbyTimeout      = std::chrono::seconds(30);
constexpr auto   kIdleTimeout       = std::chrono::seconds(15);
constexpr size_t kMaxBytesPerSecond = 64 * 1024;
constexpr size_t kMaxLobbyBufBytes  = 64 * 1024;
// per-IP 상한(핸드셰이크/세션)은 server/ip_admission.h 가 스레드 모델과 공통으로
// 정의한다. 표가 프로세스 전역이라 샤드 스레드가 인계받은 연결을 닫아도 앞단이
// 센 수가 함께 줄어든다 — 루프마다 표를 두면 인계 후 반납이 엉뚱한 표로 간다.
// 보류 송신이 이만큼 쌓이면 그 소켓으로 흘려보내는 쪽의 읽기를 멈춘다(backpressure).
// 스레드 모델은 tcp_send_all 이 최대 5초 잠들며 버텼지만, 루프는 잠들 수 없으므로
// 상대가 안 읽으면 읽기를 멈춰 메모리를 지킨다.
// 락스텝 프레임은 틱당 수십 바이트다 — 60Hz 로 방향당 1 KB/s 도 안 된다. 64 KiB 가
// 밀렸다는 것은 이미 1분 넘게 못 흘려보냈다는 뜻이고, 그쯤이면 경기가 성립하지
// 않는다. 예전 값(256 KiB / 1 MiB)은 게임 트래픽 기준으로 지나치게 컸다.
constexpr size_t kSendHighWater     = 64 * 1024;
// 그리고 하드 상한. 일시정지는 최선의 노력일 뿐 보장이 아니다 — 흘려보내는 쪽이
// 아예 없거나(룸에 혼자 남아 서버가 직접 쓰는 경우) 상대가 영영 안 읽으면 tx 는
// 계속 자란다. 상한 없는 버퍼는 상한이 아니므로 여기서 연결을 끊는다.
constexpr size_t kSendHardCap       = 256 * 1024;
// 백프레셔에서 풀린 뒤 적체를 빼내는 데 주는 면제 시간. 짧게 잡는다 — 커널 버퍼는
// 유한해서 이 안에 다 들어오고, 그 뒤에는 다시 정상 요금이 매겨진다.
constexpr auto   kRateGraceAfterResume = std::chrono::seconds(3);

std::atomic<bool> g_running{true};

void on_signal(int) { g_running.store(false); }

// ── MATCH_SUMMARY (고정 21바이트) ────────────────────────────────────────────
struct Summary {
    uint8_t  won;
    uint32_t my_score, my_lines, opp_score, opp_lines, duration_s;
};

bool parse_summary(const uint8_t* p, size_t n, Summary& out) {
    if (n != 21) return false;
    out.won        = p[0];
    out.my_score   = net::le_read_u32(p + 1);
    out.my_lines   = net::le_read_u32(p + 5);
    out.opp_score  = net::le_read_u32(p + 9);
    out.opp_lines  = net::le_read_u32(p + 13);
    out.duration_s = net::le_read_u32(p + 17);
    return true;
}

std::vector<uint8_t> build_match_result(int32_t before, int32_t after, int32_t delta) {
    std::vector<uint8_t> pl;
    pl.reserve(12);
    net::le_write_u32(pl, (uint32_t)before);
    net::le_write_u32(pl, (uint32_t)after);
    net::le_write_u32(pl, (uint32_t)delta);
    return net::build_frame(net::MsgType::MATCH_RESULT, pl);
}

std::string extract_token(const std::vector<uint8_t>& pl, size_t off) {
    if (pl.size() < off + 1) return {};
    const uint8_t n = pl[off];
    if (n == 0 || pl.size() < off + 1u + n) return {};
    return std::string(pl.begin() + off + 1, pl.begin() + off + 1 + n);
}

// ── 거절 사유 통지 ───────────────────────────────────────────────────────────
// 상한에 걸린 연결을 그냥 close 하면 사용자에게는 원인 모를 끊김이다. 닫기
// 직전에 SERVER_REJECT 를 밀어 넣어 클라이언트가 "서버 만원" 같은 문구를 띄울
// 수 있게 한다. 구버전 호환성은 net/framing.h 의 SERVER_REJECT 주석 참고.
std::vector<uint8_t> build_reject(net::RejectReason reason, const char* text) {
    const size_t n = text ? std::min<size_t>(std::strlen(text), 255) : 0;
    std::vector<uint8_t> pl;
    pl.reserve(2 + n);
    pl.push_back(static_cast<uint8_t>(reason));
    pl.push_back(static_cast<uint8_t>(n));
    pl.insert(pl.end(), text, text + n);
    return net::build_frame(net::MsgType::SERVER_REJECT, pl);
}

// 아직 Conn 이 되지 못한 소켓(accept 직후 거절)용.
//
// 전달은 보장이 아니라 최선의 노력이다. close 시점에 아직 안 읽은 수신
// 데이터가 남아 있으면 커널은 FIN 대신 RST 를 보내고, 그러면 방금 큐에 넣은
// 바이트도 함께 버려진다. 갓 accept 한 소켓에는 클라이언트가 connect 와 함께
// 보낸 첫 프레임이 이미 도착해 있을 수 있으므로, 닫기 전에 수신 큐를 한 번
// 비워 그 창을 좁힌다 — 없애지는 못한다(비운 직후에도 데이터는 도착할 수
// 있다). 그래서 클라이언트는 이 프레임이 없는 경우에도 기존의 일반 문구로
// 물러설 수 있어야 한다.
void reject_socket(net::TcpSocket& s, net::RejectReason reason, const char* text) {
    const auto fr = build_reject(reason, text);
    size_t sent = 0;
    net::tcp_send_some(s, fr.data(), fr.size(), sent);
    std::vector<uint8_t> drain;
    net::tcp_recv_some(s, drain);
    net::tcp_close(s);
}

// ── 룸 코드 ──────────────────────────────────────────────────────────────────
// 헷갈리는 글자(I, O, 0, 1)를 뺀 알파벳 — 사람이 불러 주고 받아 적는 코드다.
constexpr char   kCodeAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr size_t kCodeAlphabetN  = sizeof(kCodeAlphabet) - 1;
constexpr size_t kCodeLen        = 5;
constexpr auto   kRoomGuestWait  = std::chrono::minutes(15);  // 개설 후 게스트 무입장
constexpr auto   kRoomReadyWait  = std::chrono::seconds(60);  // 입장 후 READY 미확정

// ROOM_INFO status
constexpr uint8_t kStatusWaiting  = 0;
constexpr uint8_t kStatusFull     = 1;
constexpr uint8_t kStatusNotFound = 2;
constexpr uint8_t kStatusGoneFull = 3;

// ── 연결 상태 ────────────────────────────────────────────────────────────────
struct Channel;
struct Room;

enum class Stage { FirstFrame, Auth, Queued, Room, Lobby, Forward, Dead };

// 첫 프레임이 정한 진로. 인증이 끝난 뒤 어디로 보낼지 기억해 둔다.
enum class Intent { Queue, RoomCreate, RoomJoin };

struct Conn {
    net::TcpSocket sock;
    int      fd = -1;
    uint32_t id = 0;
    Stage    stage = Stage::FirstFrame;

    std::vector<uint8_t> rx;   // 수신 누적(프레임 경계 파싱 전)
    std::vector<uint8_t> tx;   // 보류 송신(쓰기 준비성 대기)
    bool     want_write = false;
    bool     read_paused = false;   // 상대의 tx 가 차서 읽기를 멈춘 상태

    // per-IP 입장 슬롯. handshake 는 인증이 끝나는 순간(after_auth) 놓아주고,
    // session 은 이 Conn 이 죽을 때까지 붙들고 있는다.
    std::shared_ptr<IpAdmission> handshake_slot;
    std::shared_ptr<IpAdmission> session_slot;

    // 오프로드에 던져 둔 인증 작업의 취소 깃발. Conn 이 사라진 뒤에도 워커가
    // 읽어야 하므로 Conn 이 아니라 shared_ptr 안에 산다. 세우는 쪽은 루프
    // 스레드(close_conn), 읽는 쪽은 워커 스레드라서 atomic 이다.
    std::shared_ptr<std::atomic<bool>> auth_cancel;

    // 서버 전용 프레임 위조를 이미 한 번 로그로 남겼는가 (연결당 한 줄).
    bool warned_server_only = false;

    // 인증 결과
    int64_t     player_id = 0;
    int         elo = 0;
    std::string username, token, icon{"default"};
    std::shared_ptr<PlayerSessionLease> lease;

    // 첫 프레임이 정한 진로 (인증 후 분기)
    Intent      intent = Intent::Queue;
    std::string join_code;

    // 룸
    Room* room = nullptr;
    bool  is_host = false;

    // 매치
    Channel* ch = nullptr;
    bool     is_a = false;
    bool     ready = false;

    // 전달 한도
    TimePoint last_activity{};
    TimePoint byte_window_start{};
    size_t    byte_window = 0;
    // 백프레셔에서 풀려난 직후, 적체를 빼내는 동안 레이트 상한을 면제하는 시각.
    // 멈춰 있는 동안 상대의 커널 버퍼에 쌓인 것은 우리가 안 읽어서 생긴 적체이지
    // 상대가 규정을 넘겨 보낸 게 아니다. 커널 버퍼는 유한하므로 면제도 유한하다.
    TimePoint rate_grace_until{};
};

struct Channel {
    uint32_t    match_id = 0;
    std::string match_uuid;
    uint64_t    seed = 0;
    bool        ranked = false;

    Conn* a = nullptr;   // HOST — 죽으면 nullptr
    Conn* b = nullptr;   // GUEST

    // 소켓 복사본: Conn 이 사라진 뒤에도 MATCH_RESULT 를 보낼 수 있게 채널이
    // 참조 카운트를 하나 붙들고 있는다(TcpSocket 은 shared_ptr<int> 소유 핸들).
    net::TcpSocket sockA, sockB;

    int64_t a_id = 0, b_id = 0;
    int     a_elo = 0, b_elo = 0;
    std::shared_ptr<PlayerSessionLease> a_lease, b_lease;

    std::optional<Summary> sumA, sumB;
    bool summary_handled = false;
    bool finalize_inflight = false;
    // 상대가 사라졌는데 결과 저장이 아직 도는 중이라 살아남은 쪽을 못 닫은 상태.
    // 결과 프레임을 보낸 뒤에 닫아야 하므로 continuation 이 이 표시를 보고 마무리한다.
    bool close_survivor_pending = false;
    int  disconnect_side = 0;  // 1=A, 2=B, 0=미상 — 승패가 아니라 통지 대상 선정용
};

struct Room {
    std::string code;
    Conn* host  = nullptr;
    Conn* guest = nullptr;
};

// ── 루프 ─────────────────────────────────────────────────────────────────────
class RelayLoop {
public:
    RelayLoop(meta::client::MetaClient* meta, std::string meta_note)
        : meta_(meta), meta_note_(std::move(meta_note)) {}

    bool init(uint16_t port) {
        reactor_ = net::Reactor::create();
        if (!reactor_) {
            RLOG_ERROR("[relay] reactor 생성 실패");
            return false;
        }
        offload_ = std::make_unique<Offload>(4, [this] { reactor_->wake(); });

        listen_ = net::tcp_listen(port, 64);
        if (!listen_.valid()) {
            RLOG_ERROR("[relay] port " << port << " listen 실패");
            return false;
        }
        net::tcp_set_nonblocking(listen_);
        if (!reactor_->add(listen_.fd(), net::kRead, &listen_token_)) {
            RLOG_ERROR("[relay] listen fd 등록 실패");
            return false;
        }
        is_front_ = true;   // 상태 줄은 앞단 하나만 찍는다 (카운터가 전역이라 그걸로 충분하다)
        RLOG_INFO("[relay] reactor listening on 0.0.0.0:" << port);
        RLOG_INFO("[relay] " << meta_note_);
        return true;
    }

    // 포워딩 전담 샤드 — 리스너가 없다. 넘겨받은 매치만 돌린다.
    bool init_shard(size_t index) {
        shard_index_ = index;
        reactor_ = net::Reactor::create();
        if (!reactor_) {
            RLOG_ERROR("[relay] shard " << index << " reactor 생성 실패");
            return false;
        }
        offload_ = std::make_unique<Offload>(2, [this] { reactor_->wake(); });
        return true;
    }

    // 앞단이 포워딩을 넘길 샤드 목록. 비어 있으면 앞단이 직접 전달한다(단일 루프).
    void set_shards(std::vector<RelayLoop*> shards) { shards_ = std::move(shards); }

    // 이 백엔드에서 매치를 다른 루프로 넘길 수 있는가(IOCP 는 불가 — reactor.h 참조).
    bool can_shard() const { return reactor_ && reactor_->can_migrate_sockets(); }

    // 앞단 스레드가 호출한다 — 이 클래스에서 유일하게 교차 스레드로 불리는 지점이다.
    // 우편함에 넣고 깨우면 나머지는 샤드 스레드가 자기 문맥에서 처리한다. 소켓 등록도
    // 그때 한다 — Reactor 인스턴스는 소유 스레드 전용이기 때문이다.
    void hand_off(std::unique_ptr<Conn> a, std::unique_ptr<Conn> b,
                  std::unique_ptr<Channel> ch) {
        {
            std::lock_guard<std::mutex> lk(inbox_mu_);
            inbox_.push_back(Handoff{std::move(a), std::move(b), std::move(ch)});
        }
        reactor_->wake();
    }

    void run() {
        std::vector<net::Event>   events;
        std::vector<void*>        expired;
        std::vector<Offload::Cont> conts;

        while (g_running.load()) {
            const TimePoint now = Clock::now();
            int timeout = timers_.timeout_ms(now);
            if (timeout < 0 || timeout > 500) timeout = 500;  // 종료 플래그 확인 주기

            const int n = reactor_->poll(events, timeout);
            if (n < 0) {
                RLOG_ERROR("[relay] poll 오류 — 종료");
                break;
            }

            // 0) 앞단이 넘긴 매치를 이 루프의 소유로 받아들인다
            drain_inbox();

            // 1) 오프로드 완료분을 루프 스레드에서 실행 (소켓 I/O 단일 스레드 유지)
            conts.clear();
            offload_->drain(conts);
            for (auto& c : conts) c();

            // 2) I/O 이벤트
            for (const auto& ev : events) {
                if (ev.token == &listen_token_) { on_accept(); continue; }
                Conn* c = static_cast<Conn*>(ev.token);
                if (!alive(c)) continue;          // 이번 배치에서 이미 죽은 연결
                if (ev.writable) on_writable(c);
                if (!alive(c)) continue;
                if (ev.readable || ev.error) on_readable(c);
            }

            // 3) 만기
            expired.clear();
            timers_.expired(Clock::now(), expired);
            for (void* t : expired) {
                Conn* c = static_cast<Conn*>(t);
                if (!alive(c)) continue;
                on_timeout(c);
            }

            sweep();
            if (is_front_) maybe_emit_stats(Clock::now());
        }

        shutdown();
    }

private:
    // ── 샤드 인계 ────────────────────────────────────────────────────────────
    struct Handoff {
        std::unique_ptr<Conn>    a, b;
        std::unique_ptr<Channel> ch;
    };

    void drain_inbox() {
        std::vector<Handoff> batch;
        {
            std::lock_guard<std::mutex> lk(inbox_mu_);
            if (inbox_.empty()) return;
            batch.swap(inbox_);
        }
        for (auto& h : batch) {
            Conn* a = h.a.get();
            Conn* b = h.b.get();
            Channel* ch = h.ch.get();
            channels_[ch->match_id] = std::move(h.ch);
            conns_[a] = std::move(h.a);
            conns_[b] = std::move(h.b);
            // fd 자체는 프로세스 전역이지만 관심 등록은 루프마다 따로다.
            if (!reactor_->add(a->fd, net::kRead, a) ||
                !reactor_->add(b->fd, net::kRead, b)) {
                close_conn(a, "샤드 등록 실패");
                close_conn(b, "샤드 등록 실패");
                continue;
            }
            RLOG_DEBUG("[shard " << shard_index_ << "] match=" << ch->match_id
                       << " uuid=" << ch->match_uuid << " 인계 받음");
            begin_forwarding(ch);
        }
    }

    // ── 주기 상태 ────────────────────────────────────────────────────────────
    // 동시 연결·tx 예산·거절 사유별 카운터·활성 매치를 한 줄로 낸다. 이 줄이
    // 없으면 예산 누수를 바깥에서 잴 방법이 없고, 없는 동안 실제로 반납 회계를
    // 제거한 바이너리가 테스트를 통과했다.
    void maybe_emit_stats(TimePoint now) {
        if (g_stats_interval_sec <= 0) return;
        // 기본값(epoch)은 "아직 한 번도 안 찍었다" 는 뜻 — 기동 직후 기준선을
        // 한 줄 남기고 시작한다. 그래야 첫 주기가 지나기 전에 죽은 프로세스도
        // 최소한 출발점은 말한다.
        if (next_stats_.time_since_epoch().count() != 0 && now < next_stats_) return;
        next_stats_ = now + std::chrono::seconds(g_stats_interval_sec);
        RLOG_INFO("[stats] conns=" << g_conn_count.load(std::memory_order_relaxed)
                  << "/" << g_max_conns
                  << " matches=" << g_match_count.load(std::memory_order_relaxed)
                  << " tx=" << g_tx_total.load(std::memory_order_relaxed)
                  << "/" << g_tx_budget
                  << " tx_peak=" << g_tx_peak.load(std::memory_order_relaxed)
                  << " reject_conn_cap="
                  << g_reject_conn_cap.load(std::memory_order_relaxed)
                  << " reject_ip_session="
                  << g_reject_ip_session.load(std::memory_order_relaxed)
                  << " reject_ip_handshake="
                  << g_reject_ip_handshake.load(std::memory_order_relaxed)
                  << " reject_tx_budget="
                  << g_reject_tx_budget.load(std::memory_order_relaxed)
                  // 인증 대기 줄은 밖에서 볼 방법이 이것뿐이다. 이 수가 늘고
                  // 있으면 늦은 것은 릴레이가 아니라 meta 다.
                  << " pending_auth=" << pending_auth_.size()
                  << "/" << g_max_pending_auth
                  << " reject_auth_backlog="
                  << g_reject_auth_backlog.load(std::memory_order_relaxed));
    }

    // ── 수명 관리 ────────────────────────────────────────────────────────────
    bool alive(Conn* c) const {
        auto it = conns_.find(c);
        return it != conns_.end() && it->second->stage != Stage::Dead;
    }

    // 종료 로그의 공통 식별자. 메타 서버의 경기 기록과 같은 키(match_uuid)와
    // 계정 키(player_id)를 붙여야 "그 시각 그 사람이 왜 끊겼는지" 를 맞출 수
    // 있다. 아직 인증 전이거나 매치 전이면 자리를 '-' 로 채운다 — 필드가 있다
    // 없다 하면 grep 이 깨지고, 없는 것과 0 인 것도 구분이 안 된다.
    static std::string ident_of(const Conn* c) {
        std::string s = " player_id=";
        s += std::to_string(c->player_id);
        s += " match=";
        s += c->ch ? std::to_string(c->ch->match_id) : std::string("-");
        s += " match_uuid=";
        s += c->ch ? c->ch->match_uuid : std::string("-");
        return s;
    }

    // 죽은 연결은 즉시 해제하지 않는다 — 같은 배치의 뒤쪽 이벤트가 이 포인터를
    // 들고 있을 수 있다. 표시만 하고 배치 끝(sweep)에서 해제한다.
    void close_conn(Conn* c, const char* why) {
        if (!c || c->stage == Stage::Dead) return;
        // 이 연결로 흘려보내느라 우리가 멈춰 세운 쪽이 있으면 먼저 풀어 준다.
        // room/ch 를 끊기 전에 해야 상대를 찾을 수 있다. 안 풀면 그쪽은 interest 0
        // 으로 등록된 채 아무 이벤트도 못 받아, 유휴 타이머가 걷어갈 때까지 fd 와
        // per-IP 세션 슬롯을 붙들고 남는다.
        pause_peer_read(c, false);
        // ident_of 는 c->ch 를 읽는다 — 아래에서 채널을 끊기 전에 찍어야 한다.
        RLOG_INFO("[conn " << c->id << "] close: " << why << ident_of(c));
        c->stage = Stage::Dead;
        // 보류 송신은 여기서 포기한다 — 소켓을 닫는 마당에 흘려보낼 곳이 없다.
        // 전역 예산도 같이 돌려준다.
        release_tx(c);
        reactor_->remove(c->fd);
        timers_.cancel(c);
        net::tcp_close(c->sock);
        // 인증 슬롯과 인증 작업은 반드시 함께 죽어야 한다. 예전에는 슬롯만
        // 여기서 반납되고 오프로드 큐에 던져 둔 meta 왕복은 그대로 남았다 —
        // 상한이 걸린 곳(연결)과 일이 쌓이는 곳(큐)이 어긋나 있었고, 그 틈이
        // 곧 공격면이었다: 붙어서 QUEUE_JOIN 만 던지고 끊기를 반복하면 어떤
        // 상한에도 닿지 않으면서 큐만 길어지고, 뒤에 줄 선 정상 사용자가 그
        // 길이만큼 굶었다. 깃발을 세워 두면 워커가 이 작업을 집는 순간 왕복을
        // 시작하지 않고 버린다. 스레드 모델이 구조적으로 갖고 있던 성질
        // (인증이 그 연결의 워커에서 돌아 작업과 슬롯의 수명이 같다)을
        // 루프 모델에서 손으로 맞춰 주는 것이다.
        if (c->auth_cancel) {
            c->auth_cancel->store(true, std::memory_order_release);
            c->auth_cancel.reset();
        }
        pending_auth_.erase(c->id);
        c->handshake_slot.reset();
        c->session_slot.reset();
        c->lease.reset();

        if (c->room) {
            Room* r = c->room;
            c->room = nullptr;
            (c->is_host ? r->host : r->guest) = nullptr;
            Conn* peer = c->is_host ? r->guest : r->host;
            if (peer && peer->stage != Stage::Dead) {
                // 상대는 방에 남는다 — 다시 혼자가 됐음을 알리고 대기 데드라인을
                // 게스트 대기 기준으로 되돌린다(스레드 모델의 재무장과 같다).
                peer->ready = false;
                send_room_info(peer, r->code, kStatusGoneFull, 1);
                timers_.arm(peer, Clock::now() + kRoomGuestWait);
            }
            if (!r->host && !r->guest) rooms_.erase(r->code);
        }
        if (c->ch) {
            Channel* ch = c->ch;
            (c->is_a ? ch->a : ch->b) = nullptr;
            if (ch->disconnect_side == 0) ch->disconnect_side = c->is_a ? 1 : 2;
            on_channel_peer_lost(ch);
            // 살아남은 쪽도 정리한다. 스레드 모델은 방향별 스레드가 함께 접히면서
            // 두 소켓이 같이 닫혔는데, 루프 모델에는 그 동반 종료가 없어 남은 쪽이
            // 아무 통지도 못 받은 채 타임아웃까지 기다렸다 — 수락 로비에서 30초,
            // 포워딩 중이면 유휴 15초. 한 번의 접속으로 상대의 시간을 사는 셈이었다.
            if (ch->finalize_inflight) ch->close_survivor_pending = true;
            else close_channel_survivor(ch, "상대 이탈");
        }
        // 큐 대기 중이었다면 큐에서도 뺀다.
        for (auto it = queue_.begin(); it != queue_.end(); ++it) {
            if (*it == c) { queue_.erase(it); break; }
        }
        dying_.push_back(c);
        // 전역 동시 연결 수는 여기서 줄인다. 샤드로 인계된 연결도 결국 이 경로를
        // 지나므로, 어느 루프가 닫든 정확히 한 번 줄어든다.
        g_conn_count.fetch_sub(1, std::memory_order_relaxed);
    }

    void sweep() {
        for (Conn* c : dying_) conns_.erase(c);
        dying_.clear();
        // 양쪽이 사라지고 finalize 도 끝난 채널을 정리한다.
        for (auto it = channels_.begin(); it != channels_.end();) {
            Channel* ch = it->second.get();
            if (!ch->a && !ch->b && !ch->finalize_inflight) {
                // 활성 매치 수는 여기서만 줄인다 — 샤드 인계(extract)는 소유만
                // 옮길 뿐 매치가 끝난 것이 아니다.
                g_match_count.fetch_sub(1, std::memory_order_relaxed);
                it = channels_.erase(it);
            }
            else ++it;
        }
    }

    // ── accept ───────────────────────────────────────────────────────────────
    void on_accept() {
        // 준비된 연결을 다 비운다 — 레벨 트리거라도 한 번에 처리하는 편이 낫다.
        for (;;) {
            net::TcpSocket s = net::tcp_accept(listen_);
            if (!s.valid()) return;

            // 앞단 표(conns_)가 아니라 전역 카운터를 본다. 포워딩이 시작되면
            // 연결이 샤드 루프로 인계돼 앞단 표에서 빠지므로, 표 크기로 재면
            // --loops 가 3 이상일 때 경기 중인 사람들이 통째로 안 세어진다 —
            // 상한이 4096 인데 실제로는 그보다 훨씬 많이 받아들이게 된다.
            // 이 카운터는 등록에 성공한 뒤 늘고 close_conn 에서만 주는데, 샤드로
            // 넘어간 연결도 결국 그 경로를 지나므로 어느 루프가 닫든 정확히 한 번이다.
            if (g_conn_count.load(std::memory_order_relaxed) >= g_max_conns) {
                g_reject_conn_cap.fetch_add(1, std::memory_order_relaxed);
                RLOG_INFO("[relay] 거절: 연결 상한 도달 (" << g_max_conns << ")");
                reject_socket(s, net::RejectReason::ServerFull,
                              "server is full, try again shortly");
                continue;
            }
            std::string key = net::tcp_peer_ip(s);
            if (key.empty()) key = "fd:" + std::to_string(s.fd());  // 공멸 방지
            // 두 상한을 독립적으로 건다 — 세션 슬롯을 못 얻어도, 핸드셰이크
            // 슬롯을 못 얻어도 거절이다.
            auto session_slot =
                IpAdmission::acquire(key, IpAdmission::Kind::Session);
            if (!session_slot) {
                g_reject_ip_session.fetch_add(1, std::memory_order_relaxed);
                RLOG_INFO("[relay] 거절: per-IP session 상한 (" << key << ")");
                reject_socket(s, net::RejectReason::IpSessionLimit,
                              "too many connections from your address");
                continue;
            }
            auto handshake_slot =
                IpAdmission::acquire(key, IpAdmission::Kind::Handshake);
            if (!handshake_slot) {
                g_reject_ip_handshake.fetch_add(1, std::memory_order_relaxed);
                RLOG_INFO("[relay] 거절: per-IP handshake 상한 (" << key << ")");
                reject_socket(s, net::RejectReason::IpHandshakeLimit,
                              "too many connection attempts from your address");
                continue;
            }

            auto c = std::make_unique<Conn>();
            c->sock = std::move(s);
            c->fd   = c->sock.fd();
            c->id   = next_conn_id_++;
            c->handshake_slot = std::move(handshake_slot);
            c->session_slot   = std::move(session_slot);
            c->last_activity = Clock::now();
            Conn* raw = c.get();
            if (!reactor_->add(raw->fd, net::kRead, raw)) {
                RLOG_WARN("[relay] fd 등록 실패 — 거절");
                net::tcp_close(raw->sock);
                continue;   // c 소멸 → 두 슬롯 자동 반납
            }
            timers_.arm(raw, Clock::now() + kFirstFrameTimeout);
            conns_[raw] = std::move(c);
            // 등록에 성공한 뒤에 센다 — close_conn 을 지나지 않고 사라지는
            // 연결(위 등록 실패 경로)까지 세면 카운터가 영영 안 돌아온다.
            g_conn_count.fetch_add(1, std::memory_order_relaxed);
            RLOG_DEBUG("[conn " << raw->id << "] accepted from " << key);
        }
    }

    // ── 송신(backpressure) ───────────────────────────────────────────────────
    // 논블로킹으로 최대한 보내고 남는 것은 tx 에 쌓는다. 상대가 안 읽어 tx 가
    // 한계를 넘으면 그 소켓으로 흘려보내는 쪽의 읽기를 멈춘다.
    bool queue_send(Conn* dst, const uint8_t* data, size_t len) {
        if (!dst || dst->stage == Stage::Dead) return false;
        size_t sent = 0;
        if (dst->tx.empty()) {
            if (!net::tcp_send_some(dst->sock, data, len, sent)) return false;
        }
        if (sent < len) {
            const size_t added = len - sent;
            dst->tx.insert(dst->tx.end(), data + sent, data + len);
            const size_t total = g_tx_total.fetch_add(added,
                                     std::memory_order_relaxed) + added;
            // 최고 수위. 예산에 얼마나 근접했는지는 순간값만 봐서는 알 수 없다 —
            // 상태 줄 사이에서 치솟았다 빠지면 어느 줄에도 안 남는다.
            // 이 갱신은 "커널이 다 받아 주지 않은" 경로에만 있으므로 정상
            // 포워딩(전량 송신)에서는 실행되지 않는다.
            for (size_t peak = g_tx_peak.load(std::memory_order_relaxed);
                 total > peak;) {
                if (g_tx_peak.compare_exchange_weak(peak, total,
                                                    std::memory_order_relaxed)) break;
            }
            arm_write(dst, true);
            if (dst->tx.size() > kSendHardCap) {
                close_conn(dst, "송신 버퍼 하드 상한 초과");
                return false;
            }
            if (total > g_tx_budget) {
                // 프로세스 전체 예산 초과. 연결당 상한 안에 있어도 여기서 끊는다 —
                // 지킬 대상이 이 연결이 아니라 프로세스이기 때문이다.
                g_reject_tx_budget.fetch_add(1, std::memory_order_relaxed);
                RLOG_WARN("[relay] 거절: tx 전역 예산 초과 (" << total << " > "
                          << g_tx_budget << ")" << ident_of(dst));
                reject_conn(dst, net::RejectReason::TxBudget,
                            "server memory budget exhausted",
                            "tx 전역 예산 초과");
                return false;
            }
            if (dst->tx.size() > kSendHighWater) pause_peer_read(dst, true);
        }
        return true;
    }

    // tx 를 비우고 그만큼 전역 예산을 돌려준다. 연결이 죽는 모든 경로가 여길 지나야
    // 카운터가 새지 않는다.
    static void release_tx(Conn* c) {
        if (c->tx.empty()) return;
        g_tx_total.fetch_sub(c->tx.size(), std::memory_order_relaxed);
        c->tx.clear();
        c->tx.shrink_to_fit();
    }

    // 이미 등록된 연결을 사유와 함께 끊는다.
    //
    // 보류 송신이 남아 있을 수 있으므로 프레임을 그 뒤에 붙인다 — 앞질러 보내면
    // 클라이언트가 받는 바이트 순서가 어긋나 프레임 경계가 깨진다. 붙인 만큼
    // 전역 예산도 함께 올려 둔다. 곧바로 close_conn 이 release_tx 로 남은 전부를
    // 돌려주므로 회계는 맞는다. 한 번 밀어 보고 안 나가면 그대로 포기한다 —
    // 안 읽는 상대를 기다리는 것이 애초에 여기 온 이유다.
    void reject_conn(Conn* c, net::RejectReason reason, const char* text,
                     const char* why) {
        if (!c || c->stage == Stage::Dead) return;
        const auto fr = build_reject(reason, text);
        c->tx.insert(c->tx.end(), fr.begin(), fr.end());
        g_tx_total.fetch_add(fr.size(), std::memory_order_relaxed);
        size_t sent = 0;
        if (net::tcp_send_some(c->sock, c->tx.data(), c->tx.size(), sent) && sent) {
            c->tx.erase(c->tx.begin(), c->tx.begin() + sent);
            g_tx_total.fetch_sub(sent, std::memory_order_relaxed);
        }
        close_conn(c, why);
    }

    // dst 로 흘려보내는 쪽. 포워딩 중이면 채널 상대, 룸 단계면 룸 상대다.
    // 예전에는 채널만 봤는데, 룸 단계 Conn 은 채널이 없어 백프레셔가 통째로
    // 무효였다 — 상대가 안 읽는 동안 CHAT 이 룸 대기 시간 내내 tx 에 쌓였다.
    Conn* feeder_of(Conn* dst) {
        if (Channel* ch = dst->ch) return (dst == ch->a) ? ch->b : ch->a;
        if (Room* r = dst->room)   return dst->is_host ? r->guest : r->host;
        return nullptr;
    }

    void arm_write(Conn* c, bool want) {
        if (c->want_write == want) return;
        c->want_write = want;
        unsigned interest = (c->read_paused ? 0u : net::kRead) |
                            (want ? net::kWrite : 0u);
        reactor_->modify(c->fd, interest, c);
    }

    void pause_peer_read(Conn* dst, bool pause) {
        Conn* src = feeder_of(dst);
        if (!src || src->stage == Stage::Dead) return;
        if (src->read_paused == pause) return;
        src->read_paused = pause;
        unsigned interest = (pause ? 0u : net::kRead) |
                            (src->want_write ? net::kWrite : 0u);
        reactor_->modify(src->fd, interest, src);
        // 재개하는 순간 유휴 데드라인을 새로 건다. 멈춰 있는 동안에는 읽기 이벤트가
        // 없어 last_activity 가 굳어 있었으므로, 그대로 두면 풀자마자 만기로 끊긴다.
        if (!pause) {
            src->last_activity = Clock::now();
            // 바이트 창도 같이 연다. 멈춰 있는 동안 상대의 커널 송신 버퍼에는
            // 백로그가 쌓이고, 재개하는 순간 그게 한꺼번에 들어온다 — 창을 안 열면
            // 우리가 막아 놓고 그 대가를 상대에게 "레이트 초과" 로 청구하게 된다.
            src->byte_window_start = src->last_activity;
            src->byte_window = 0;
            // 창을 여는 것만으론 부족하다. 적체는 한 번의 버스트로 들어오므로 새 창을
            // 그 자리에서 다시 넘긴다. 빼내는 동안은 아예 면제한다.
            src->rate_grace_until = src->last_activity + kRateGraceAfterResume;
            if (src->stage == Stage::Forward) {
                timers_.arm(src, src->last_activity + kIdleTimeout);
            }
        }
    }

    void on_writable(Conn* c) {
        if (c->tx.empty()) { arm_write(c, false); return; }
        size_t sent = 0;
        if (!net::tcp_send_some(c->sock, c->tx.data(), c->tx.size(), sent)) {
            close_conn(c, "send 실패");
            return;
        }
        if (sent) {
            c->tx.erase(c->tx.begin(), c->tx.begin() + sent);
            g_tx_total.fetch_sub(sent, std::memory_order_relaxed);
        }
        if (c->tx.empty()) {
            arm_write(c, false);
            pause_peer_read(c, false);   // 밀림이 풀렸으니 상대 읽기 재개
        } else if (c->tx.size() <= kSendHighWater) {
            pause_peer_read(c, false);
        }
    }

    // ── 수신 ─────────────────────────────────────────────────────────────────
    void on_readable(Conn* c) {
        const size_t before = c->rx.size();
        if (!net::tcp_recv_some(c->sock, c->rx)) {
            close_conn(c, "peer 종료");
            return;
        }
        const size_t got = c->rx.size() - before;
        if (got == 0) return;   // 준비성만 왔고 실제 데이터는 없음

        const TimePoint now = Clock::now();
        c->last_activity = now;
        if (now - c->byte_window_start >= std::chrono::seconds(1)) {
            c->byte_window_start = now;
            c->byte_window = 0;
        }
        c->byte_window += got;
        // 레이트 상한은 단계를 가리지 않는다. 예전에는 Forward 에만 걸려 있었는데,
        // 정작 위험한 쪽은 반대였다 — 큐 대기와 인증 왕복 단계는 rx 를 소비하지 않고
        // 쌓아 두기만 하므로(뒤 단계로 넘겨야 하는 잔여 바이트를 잃지 않으려고),
        // 상한이 없으면 한 연결이 메모리를 무한히 먹는다.
        if (now >= c->rate_grace_until && c->byte_window > kMaxBytesPerSecond) {
            close_conn(c, "byte rate 초과");
            return;
        }
        // 레이트 상한만으로는 "느리게, 오래" 붓는 것을 못 막는다. 누적 버퍼에도
        // 상한을 건다. 재파싱이 매 읽기마다 도는 단계가 있어(on_queued 는 잔여를
        // 보존하려고 사본을 파싱한다) 상한이 없으면 비용이 O(n^2) 로 자라 단일 루프
        // 스레드를 통째로 잡아먹는다 — 진행 중인 모든 매치가 함께 멈춘다.
        if (c->rx.size() > kMaxLobbyBufBytes) {
            close_conn(c, "수신 버퍼 상한 초과");
            return;
        }

        switch (c->stage) {
            case Stage::FirstFrame: on_first_frame(c); break;
            case Stage::Room:       on_room(c);        break;
            case Stage::Lobby:      on_lobby(c);       break;
            case Stage::Forward:    on_forward(c);     break;
            case Stage::Queued:     on_queued(c);      break;
            case Stage::Auth:       break;  // 인증 중 — rx 에 쌓아 두고 나중에 처리
            case Stage::Dead:       break;
        }
    }

    // 첫 프레임: QUEUE_JOIN 만 이관됐다. 인증은 오프로드한다.
    void on_first_frame(Conn* c) {
        std::vector<net::Frame> frames;
        if (!net::parse_frames(c->rx, frames)) {
            // framing.h 의 계약: false 는 "스트림이 어긋났으니 호출자가 닫는다" 다.
            // 무시하면 어긋난 채로 연결이 유지되고, 이후 읽는 바이트는 전부 의미가 없다.
            close_conn(c, "프레이밍 위반");
            return;
        }
        for (size_t i = 0; i < frames.size(); ++i) {
            const net::Frame& f = frames[i];

            // 첫 명령과 같은 recv 로 이미 도착한 프레임/부분 바이트를 보존한다.
            // 버리면 CREATE/JOIN 과 붙어 온 READY 가 유실된다.
            auto keep_residual = [&] {
                std::vector<uint8_t> residual;
                for (size_t j = i + 1; j < frames.size(); ++j) {
                    auto bytes = net::build_frame(frames[j].type, frames[j].payload);
                    residual.insert(residual.end(), bytes.begin(), bytes.end());
                }
                residual.insert(residual.end(), c->rx.begin(), c->rx.end());
                c->rx = std::move(residual);
            };

            if (f.type == net::MsgType::QUEUE_JOIN) {
                std::string tok = extract_token(f.payload, 0);
                keep_residual();
                c->intent = Intent::Queue;
                begin_auth(c, std::move(tok));
                return;
            }
            if (f.type == net::MsgType::QUEUE_CANCEL) {
                close_conn(c, "큐 진입 전 QUEUE_CANCEL");
                return;
            }
            if (f.type == net::MsgType::ROOM_CREATE) {
                std::string tok = extract_token(f.payload, 0);
                keep_residual();
                c->intent = Intent::RoomCreate;
                begin_auth(c, std::move(tok));
                return;
            }
            if (f.type == net::MsgType::ROOM_JOIN) {
                // 페이로드: [code_len:1][code:N][tok_len:1][token:M]
                if (f.payload.empty()) continue;
                const uint8_t n = f.payload[0];
                if (n == 0 || n > kCodeLen || f.payload.size() < 1u + n) continue;
                c->join_code.assign(f.payload.begin() + 1, f.payload.begin() + 1 + n);
                std::string tok = extract_token(f.payload, 1u + n);
                keep_residual();
                c->intent = Intent::RoomJoin;
                begin_auth(c, std::move(tok));
                return;
            }
            // HELLO 등 낯선 프레임은 무시하고 계속 기다린다.
        }
    }

    // 인증이 끝난 뒤 첫 프레임이 정한 진로로 보낸다.
    void after_auth(Conn* c) {
        // 핸드셰이크 예산은 여기서 끝난다. 붙들고 있으면 상한 16 이 "동시 세션"
        // 예산으로 변해 NAT 뒤 다수 사용자나 loopback 테스트가 걸린다.
        // 세션 예산(session_slot)은 그대로 유지된다 — 인증을 통과했다고 해서
        // 한 주소가 전역 상한까지 연결을 쌓을 수 있어서는 안 된다.
        c->handshake_slot.reset();

        switch (c->intent) {
            case Intent::Queue:      enter_queue(c); break;
            case Intent::RoomCreate: room_create(c); break;
            case Intent::RoomJoin:   room_join(c);   break;
        }
    }

    // 인증은 meta HTTP 왕복이라 루프에서 부르면 그동안 전원이 멈춘다 — 워커로 뺀다.
    void begin_auth(Conn* c, std::string token) {
        c->stage = Stage::Auth;
        timers_.cancel(c);

        if (!meta_) {                    // unranked: 검증 없이 통과
            c->token = std::move(token);
            after_auth(c);
            return;
        }
        if (token.empty()) {
            close_conn(c, "토큰 없음 -> 거절");
            return;
        }

        // 대기 중인 왕복이 이미 상한만큼 있으면 줄을 더 늘리지 않고 거절한다.
        // 취소만으로는 절반이다 — 취소는 "죽은 연결" 의 일을 지울 뿐이고, 살아
        // 있는 연결이 상한까지 몰려오면 줄은 여전히 길어진다. 그때 조용히
        // 세워 두면 그 사람은 어차피 자기 클라이언트의 타임아웃까지 기다렸다
        // 실패하므로, 기다리게 하는 대신 지금 사유를 밝히고 보낸다. 스레드
        // 모델이 워커가 다 찼을 때 하는 것과 같은 선택이다 — 굶기는 대신 거절.
        if (pending_auth_.size() >= g_max_pending_auth) {
            g_reject_auth_backlog.fetch_add(1, std::memory_order_relaxed);
            RLOG_WARN("[relay] 거절: 인증 대기 상한 (" << pending_auth_.size()
                      << "/" << g_max_pending_auth << ")");
            reject_conn(c, net::RejectReason::AuthBacklog,
                        "server is busy authenticating, try again shortly",
                        "인증 대기 상한");
            return;
        }

        // continuation 은 Conn* 이 아니라 conn id 를 포착한다 — 인증 왕복 사이에
        // 그 연결이 끊겨 객체가 사라졌을 수 있기 때문이다. 재개 시점에 id 로 다시
        // 찾고, 없으면 조용히 버린다.
        const uint32_t cid = c->id;
        meta::client::MetaClient* meta = meta_;
        // 취소 깃발은 Conn 보다 오래 산다 — 워커가 작업을 집는 시점에 Conn 은
        // 이미 없을 수 있고, 그때 읽어야 하는 것이 바로 이 값이다.
        c->auth_cancel = std::make_shared<std::atomic<bool>>(false);
        auto cancel = c->auth_cancel;
        pending_auth_.insert(cid);
        const bool queued = offload_->submit(
            [this, meta, token, cid, cancel]() -> Offload::Cont {
                // 큐에서 기다리는 동안 그 연결이 죽었으면 왕복 자체를 하지
                // 않는다. 이 검사가 없으면 이미 아무도 기다리지 않는 응답을
                // 위해 워커 하나가 왕복 한 번(배포 대상에서 수십~수백 ms)을
                // 통째로 쓰고, 그 시간은 뒤에 선 진짜 사용자가 낸다.
                // g_running 도 같이 본다 — 종료 중에는 이 큐를 다 비우느라
                // graceful 종료가 큐 길이만큼 늦어졌다.
                if (cancel->load(std::memory_order_acquire) ||
                    !g_running.load(std::memory_order_relaxed)) {
                    return {};   // continuation 없음 — 루프는 이 작업을 보지도 않는다
                }
                meta::client::MetaClient::VerifyOutcome outcome{};
                auto auth = meta->verify_token(token, 3, &outcome);
                return [this, cid, auth, token]() { resume_auth(cid, auth, token); };
            });
        if (!queued) close_conn(c, "종료 중 — 인증 불가");
    }

    void resume_auth(uint32_t conn_id,
                     std::optional<meta::client::AuthInfo> auth,
                     const std::string& token) {
        pending_auth_.erase(conn_id);
        Conn* c = find_by_id(conn_id);
        if (!c) return;   // 인증 도중 끊겼다
        // 왕복이 끝났으니 취소 깃발도 역할을 다했다. 남겨 두면 이후 close_conn 이
        // 아무도 안 보는 값을 세우게 되고, "깃발이 있다 = 큐에 일이 있다" 라는
        // 읽기가 깨진다.
        c->auth_cancel.reset();
        if (!auth) {
            close_conn(c, "meta verify 실패 -> 거절");
            return;
        }
        c->player_id = auth->player_id;
        c->elo       = auth->elo;
        c->username  = auth->username;
        c->token     = token;
        c->icon      = auth->selected_icon_id.empty() ? "default" : auth->selected_icon_id;
        c->lease     = PlayerSessionLease::acquire(c->player_id);
        if (!c->lease) {
            close_conn(c, "동일 player_id 중복 세션 -> 거절");
            return;
        }
        RLOG_DEBUG("[conn " << c->id << "] authed player_id=" << c->player_id
                   << " elo=" << c->elo);
        // unranked 경로(begin_auth 의 !meta_ 분기)와 반드시 같은 문을 통과해야 한다.
        // 여기서 enter_queue 를 직접 부르면 두 가지가 조용히 깨진다:
        //   - 핸드셰이크 슬롯이 안 풀려 per-IP "동시 핸드셰이크" 예산이 "동시 세션"
        //     예산으로 변한다 (NAT 뒤 다수 사용자가 서로를 굶긴다).
        //   - c->intent 가 무시되어 랭크드 ROOM_CREATE/ROOM_JOIN 이 매치메이킹으로
        //     끌려간다.
        after_auth(c);
    }

    Conn* find_by_id(uint32_t id) {
        for (auto& [ptr, up] : conns_) {
            if (up->id == id && up->stage != Stage::Dead) return ptr;
        }
        return nullptr;
    }

    // ── 룸 ───────────────────────────────────────────────────────────────────
    void send_room_info(Conn* c, const std::string& code,
                        uint8_t status, uint8_t peer_count) {
        std::vector<uint8_t> pl;
        pl.reserve(1 + code.size() + 2);
        pl.push_back((uint8_t)code.size());
        pl.insert(pl.end(), code.begin(), code.end());
        pl.push_back(status);
        pl.push_back(peer_count);
        auto fr = net::build_frame(net::MsgType::ROOM_INFO, pl);
        queue_send(c, fr.data(), fr.size());
    }

    std::string generate_code() {
        for (int attempt = 0; attempt < 32; ++attempt) {
            std::string code(kCodeLen, 'A');
            uint64_t x = next_seed();
            for (size_t i = 0; i < kCodeLen; ++i) {
                code[i] = kCodeAlphabet[x % kCodeAlphabetN];
                x /= kCodeAlphabetN;
                if (x == 0) x = next_seed();
            }
            if (!rooms_.count(code)) return code;
        }
        return {};
    }

    void room_create(Conn* c) {
        std::string code = generate_code();
        if (code.empty()) { close_conn(c, "룸 코드 발급 실패"); return; }
        auto up = std::make_unique<Room>();
        up->code = code;
        up->host = c;
        Room* r = up.get();
        rooms_[code] = std::move(up);

        c->room = r;
        c->is_host = true;
        c->stage = Stage::Room;
        send_room_info(c, code, kStatusWaiting, 1);
        // 게스트가 안 들어오면 슬롯을 무한정 점유하지 않도록 데드라인을 건다.
        timers_.arm(c, Clock::now() + kRoomGuestWait);
        RLOG_DEBUG("[conn " << c->id << "] ROOM_CREATE " << code
                   << " player_id=" << c->player_id);
        if (!c->rx.empty()) on_room(c);
    }

    void room_join(Conn* c) {
        auto it = rooms_.find(c->join_code);
        if (it == rooms_.end()) {
            send_room_info(c, c->join_code, kStatusNotFound, 0);
            close_conn(c, "존재하지 않는 룸 코드");
            return;
        }
        Room* r = it->second.get();
        if (r->guest || !r->host) {
            send_room_info(c, c->join_code, kStatusFull, r->host ? 1 : 0);
            close_conn(c, "룸이 가득 참");
            return;
        }
        r->guest = c;
        c->room = r;
        c->is_host = false;
        c->stage = Stage::Room;
        RLOG_DEBUG("[conn " << c->id << "] ROOM_JOIN " << r->code
                   << " player_id=" << c->player_id);

        // 양쪽에 "둘 다 있음" 을 알리고, 대기 데드라인을 READY 기준으로 다시 건다.
        send_room_info(r->host, r->code, kStatusWaiting, 2);
        send_room_info(c,       r->code, kStatusWaiting, 2);
        const TimePoint dl = Clock::now() + kRoomReadyWait;
        timers_.arm(r->host, dl);
        timers_.arm(c, dl);

        // host 를 미리 붙들어 둔다. 아래 on_room(c) 가 양쪽 READY 를 관측하면
        // start_room_match 로 들어가 rooms_.erase 로 Room 을 파괴하므로, 그 뒤에
        // r 을 다시 만지면 use-after-free 다. alive() 가드는 Conn 의 생존만 보지
        // Room 의 생존은 보지 않는다 — r->host 를 꺼내는 순간 이미 늦는다.
        Conn* host = r->host;
        if (!c->rx.empty()) on_room(c);
        // 호스트가 CREATE 와 같은 recv 로 보냈던 READY 가 남아 있을 수 있다.
        // 매치가 이미 시작됐다면 host->room 이 nullptr 이라 on_room 이 곧바로 반환한다.
        if (alive(host) && !host->rx.empty()) on_room(host);
    }

    void on_room(Conn* c) {
        Room* r = c->room;
        if (!r) return;
        std::vector<net::Frame> frames;
        if (!net::parse_frames(c->rx, frames)) {
            close_conn(c, "룸 단계 프레이밍 위반");   // 위 on_first_frame 주석 참고
            return;
        }
        for (const auto& f : frames) {
            Conn* peer = c->is_host ? r->guest : r->host;
            if (f.type == net::MsgType::READY) {
                const bool ready = !f.payload.empty() && f.payload[0] != 0;
                c->ready = ready;
                if (peer && peer->stage != Stage::Dead) {
                    std::vector<uint8_t> pl{(uint8_t)(ready ? 1 : 0)};
                    auto fr = net::build_frame(net::MsgType::READY, pl);
                    queue_send(peer, fr.data(), fr.size());
                }
            } else if (f.type == net::MsgType::ROOM_LEAVE) {
                close_conn(c, "ROOM_LEAVE");
                return;
            } else if (f.type == net::MsgType::CHAT) {
                if (peer && peer->stage != Stage::Dead) {
                    auto fr = net::build_frame(net::MsgType::CHAT, f.payload);
                    queue_send(peer, fr.data(), fr.size());
                }
            }
            // 그 밖의 프레임은 룸 단계에서 무시한다.
        }
        if (r->host && r->guest && r->host->ready && r->guest->ready) {
            start_room_match(r);
        }
    }

    // 룸에서는 READY 가 이미 확정됐으므로 로비를 건너뛰고 바로 포워딩으로 간다.
    void start_room_match(Room* r) {
        Conn* host = r->host;
        Conn* guest = r->guest;
        // 키를 값으로 복사한 뒤 지운다. r->code 를 그대로 넘기면 지워질 원소 안의
        // 문자열을 키로 쓰는 셈이라, 노드가 파괴되는 순간 참조가 죽는다.
        const std::string code = r->code;
        rooms_.erase(code);             // 방은 역할을 다했다
        host->room = nullptr;
        guest->room = nullptr;

        Channel* ch = make_channel(host, guest);
        if (!send_match_found(host,  1, ch->seed, host->icon,  guest->icon, ch->match_uuid) ||
            !send_match_found(guest, 2, ch->seed, guest->icon, host->icon,  ch->match_uuid)) {
            close_conn(host,  "MATCH_FOUND 송신 실패");
            close_conn(guest, "MATCH_FOUND 송신 실패");
            return;
        }
        begin_forwarding(ch);
    }

    // ── 큐 ───────────────────────────────────────────────────────────────────
    void enter_queue(Conn* c) {
        c->stage = Stage::Queued;
        // 큐에 세우기 전에 이미 도착해 있는 QUEUE_CANCEL 을 먼저 본다. 순서를
        // 뒤집으면(넣고 → 짝짓고 → 취소 확인) 상대가 이미 대기 중일 때 취소한
        // 사람이 그 자리에서 매칭돼 버린다. 스레드 모델도 페어링 직전에 대기자
        // 생존을 확인해 같은 것을 막는다.
        if (!c->rx.empty()) {
            on_queued(c);
            if (!alive(c)) return;
        }
        queue_.push_back(c);
        RLOG_DEBUG("[conn " << c->id << "] queued (" << queue_.size() << " 대기)"
                   << " player_id=" << c->player_id);
        try_pair();
    }

    // 큐 대기 중에는 QUEUE_CANCEL 만 본다. 그 외 바이트는 쌓아 두고 매치 성립 시
    // 로비 버퍼로 넘어간다(프레임이 세그먼트 경계에 걸쳐도 유실되지 않게).
    void on_queued(Conn* c) {
        std::vector<uint8_t> copy = c->rx;
        std::vector<net::Frame> frames;
        if (!net::parse_frames(copy, frames)) {
            // 프레이밍 계약 위반(과대 길이 선언 등)은 스트림이 이미 어긋났다는 뜻이고,
            // framing.h 의 계약도 "false 면 호출자가 닫는다" 다. 여기서 무시하면
            // 피해가 이 연결에서 끝나지 않는다: 사본을 파싱하는 구조라 그 바이트가
            // 진짜 버퍼 머리에 영원히 남아 이후 QUEUE_CANCEL 을 다시는 볼 수 없고,
            // 그 상태로 정직한 상대와 매칭된 뒤 로비에서 같은 헤더에 걸려 죽는다 —
            // 7바이트로 두 사람을 함께 가두는 셈이다.
            close_conn(c, "큐 대기 중 프레이밍 위반");
            return;
        }
        for (const auto& f : frames) {
            if (f.type == net::MsgType::QUEUE_CANCEL) {
                close_conn(c, "QUEUE_CANCEL");
                return;
            }
        }
    }

    void try_pair() {
        while (queue_.size() >= 2) {
            Conn* a = queue_.front(); queue_.pop_front();
            Conn* b = queue_.front(); queue_.pop_front();
            if (!alive(a)) { if (alive(b)) queue_.push_front(b); continue; }
            if (!alive(b)) { queue_.push_front(a); continue; }
            start_match(a, b);
        }
    }

    uint64_t next_seed() {
        // xorshift64 — 서버 내부 RNG (스레드 모델과 같은 방식)
        if (seed_state_ == 0) seed_state_ = 0x9E3779B97F4A7C15ULL ^ (uint64_t)Clock::now().time_since_epoch().count();
        seed_state_ ^= seed_state_ << 13;
        seed_state_ ^= seed_state_ >> 7;
        seed_state_ ^= seed_state_ << 17;
        return seed_state_;
    }

    // 큐·룸 두 경로가 공유하는 채널 생성. 스테이지는 호출자가 정한다 — 큐는
    // READY 핸드셰이크(로비)를 거치고, 룸은 이미 READY 라 곧장 포워딩으로 간다.
    Channel* make_channel(Conn* a, Conn* b) {
        auto up = std::make_unique<Channel>();
        Channel* ch = up.get();
        ch->match_id   = next_match_id_++;
        ch->match_uuid = new_match_uuid();
        ch->seed       = next_seed();
        ch->a = a; ch->b = b;
        ch->sockA = a->sock; ch->sockB = b->sock;
        ch->a_id = a->player_id; ch->b_id = b->player_id;
        ch->a_elo = a->elo;      ch->b_elo = b->elo;
        ch->a_lease = a->lease;  ch->b_lease = b->lease;
        ch->ranked = (meta_ != nullptr) && a->player_id != 0 && b->player_id != 0;
        channels_[ch->match_id] = std::move(up);
        // 활성 매치 수 — 줄이는 곳은 sweep 하나뿐이다(샤드 인계는 소유 이전일 뿐).
        g_match_count.fetch_add(1, std::memory_order_relaxed);

        a->ch = ch; a->is_a = true;
        b->ch = ch; b->is_a = false;

        RLOG_INFO("[relay] match=" << ch->match_id << " uuid=" << ch->match_uuid
                  << " paired conn " << a->id << " x " << b->id
                  << " player_id=" << a->player_id << " x " << b->player_id
                  << (ch->ranked ? " (ranked)" : " (unranked)"));
        return ch;
    }

    void start_match(Conn* a, Conn* b) {
        Channel* ch = make_channel(a, b);
        a->stage = Stage::Lobby;
        b->stage = Stage::Lobby;

        if (!send_match_found(a, 1, ch->seed, a->icon, b->icon, ch->match_uuid) ||
            !send_match_found(b, 2, ch->seed, b->icon, a->icon, ch->match_uuid)) {
            close_conn(a, "MATCH_FOUND 송신 실패");
            close_conn(b, "MATCH_FOUND 송신 실패");
            return;
        }
        const TimePoint deadline = Clock::now() + kLobbyTimeout;
        timers_.arm(a, deadline);
        timers_.arm(b, deadline);

        // 매칭 직전에 이미 도착해 있던 바이트를 로비 규칙으로 처리한다.
        if (!a->rx.empty()) on_lobby(a);
        if (alive(b) && !b->rx.empty()) on_lobby(b);
    }

    bool send_match_found(Conn* c, uint8_t role, uint64_t seed,
                          const std::string& my_icon, const std::string& peer_icon,
                          const std::string& uuid) {
        const std::string my   = my_icon.empty()   ? "default" : my_icon;
        const std::string peer = peer_icon.empty() ? "default" : peer_icon;
        std::vector<uint8_t> pl;
        pl.push_back(role);
        net::le_write_u64(pl, seed);
        pl.push_back((uint8_t)std::min<size_t>(my.size(), 255));
        pl.insert(pl.end(), my.begin(), my.begin() + std::min<size_t>(my.size(), 255));
        pl.push_back((uint8_t)std::min<size_t>(peer.size(), 255));
        pl.insert(pl.end(), peer.begin(), peer.begin() + std::min<size_t>(peer.size(), 255));
        pl.push_back((uint8_t)std::min<size_t>(uuid.size(), 255));
        pl.insert(pl.end(), uuid.begin(), uuid.begin() + std::min<size_t>(uuid.size(), 255));
        auto fr = net::build_frame(net::MsgType::MATCH_FOUND, pl);
        return queue_send(c, fr.data(), fr.size());
    }

    // ── 로비(READY 핸드셰이크) ───────────────────────────────────────────────
    void on_lobby(Conn* c) {
        Channel* ch = c->ch;
        if (!ch) return;
        Conn* peer = c->is_a ? ch->b : ch->a;

        if (c->rx.size() > kMaxLobbyBufBytes) {
            close_conn(c, "로비 버퍼 초과");
            return;
        }

        // READY/QUEUE_CANCEL 만 소비한다. 게임 프레임을 만나면 멈추고 rx 에 남겨
        // 포워딩 단계가 그대로 이어받는다.
        while (c->rx.size() >= 6 && !c->ready) {
            const uint16_t len = (uint16_t)c->rx[0] | ((uint16_t)c->rx[1] << 8);
            if ((size_t)len > net::kMaxPayloadBytes + 1u) {
                close_conn(c, "로비 프레임 길이 초과");
                return;
            }
            const size_t total = 2u + (size_t)len + 4u;
            if (c->rx.size() < total) break;      // 미완성 — 더 기다린다
            if (len < 1) { c->rx.erase(c->rx.begin(), c->rx.begin() + total); continue; }

            const uint8_t type = c->rx[2];
            if (type != (uint8_t)net::MsgType::READY &&
                type != (uint8_t)net::MsgType::QUEUE_CANCEL) {
                break;   // 게임 프레임 — 포워딩으로 넘긴다
            }
            const size_t payload_len = (size_t)len - 1u;
            const uint32_t chk  = net::le_read_u32(c->rx.data() + 2u + len);
            const uint32_t calc = payload_len == 0 ? 0u
                                : net::fnv1a32(c->rx.data() + 3, payload_len);
            if (chk != calc) { c->rx.erase(c->rx.begin(), c->rx.begin() + total); continue; }

            const bool is_ready = (type == (uint8_t)net::MsgType::READY);
            const uint8_t v = (is_ready && payload_len >= 1) ? c->rx[3] : 0;
            c->rx.erase(c->rx.begin(), c->rx.begin() + total);

            if (!is_ready || v == 0) {           // 취소/거절 — 상대에게 알리고 종료
                if (peer && peer->stage != Stage::Dead) {
                    std::vector<uint8_t> pl{0};
                    auto fr = net::build_frame(net::MsgType::READY, pl);
                    queue_send(peer, fr.data(), fr.size());
                }
                close_conn(c, "로비에서 취소/거절");
                return;
            }
            // READY(1) — 상대에게 전달
            c->ready = true;
            if (peer && peer->stage != Stage::Dead) {
                std::vector<uint8_t> pl{1};
                auto fr = net::build_frame(net::MsgType::READY, pl);
                if (!queue_send(peer, fr.data(), fr.size())) {
                    close_conn(peer, "READY 전달 실패");
                    return;
                }
            }
        }

        if (ch->a && ch->b && ch->a->ready && ch->b->ready) begin_forwarding(ch);
    }

    // 포워딩 시작 지점이자 샤딩의 경계다. 앞단이라면 여기서 매치를 통째로 샤드에
    // 넘긴다 — 이 뒤로는 패킷마다 도는 비싼 일만 남고, 그 일에는 공유 상태가 없다.
    void begin_forwarding(Channel* ch) {
        Conn* a = ch->a; Conn* b = ch->b;
        if (!a || !b) return;

        if (!shards_.empty()) {
            RelayLoop* target = shards_[next_shard_ % shards_.size()];
            ++next_shard_;
            // 이 루프의 관심에서 떼고 타이머를 접은 뒤 소유권을 통째로 옮긴다.
            reactor_->remove(a->fd);
            reactor_->remove(b->fd);
            timers_.cancel(a);
            timers_.cancel(b);
            auto na = conns_.extract(a);
            auto nb = conns_.extract(b);
            auto nc = channels_.extract(ch->match_id);
            if (!na || !nb || !nc) return;   // 있을 수 없는 상태 — 방어
            target->hand_off(std::move(na.mapped()), std::move(nb.mapped()),
                             std::move(nc.mapped()));
            return;
        }

        const TimePoint now = Clock::now();
        for (Conn* c : {a, b}) {
            c->stage = Stage::Forward;
            c->last_activity = now;
            c->byte_window_start = now;
            c->byte_window = 0;
            timers_.arm(c, now + kIdleTimeout);
        }
        RLOG_INFO("[relay] match=" << ch->match_id << " uuid=" << ch->match_uuid
                  << " forwarding 시작");
        // 로비에서 남은 바이트(READY 이후 도착한 게임 프레임)를 지금 흘려보낸다.
        if (!a->rx.empty()) on_forward(a);
        if (alive(b) && !b->rx.empty()) on_forward(b);
    }

    // ── 포워딩 ───────────────────────────────────────────────────────────────
    // 클라이언트가 서버 전용 프레임을 올려보냈다. 연결당 한 줄만 남긴다 —
    // 프레임마다 찍으면 위조 프레임을 쏟아붓는 것만으로 로그를 밀어낼 수 있고,
    // 그건 이 결함을 고치면서 새로 만드는 또 하나의 값싼 공격이다.
    void note_server_only(Conn* c, uint8_t type) {
        if (c->warned_server_only) return;
        c->warned_server_only = true;
        RLOG_WARN("[relay] 서버 전용 프레임을 클라이언트가 보내 폐기 type="
                  << (int)type << " (이 연결의 이후 위반은 조용히 버린다)"
                  << ident_of(c));
    }

    // unranked 포워딩. 프레임 경계만 훑어 서버 전용 프레임을 버리고, 통과한
    // 프레임들은 "붙어 있는 구간째로" 한 번에 민다. false = 송신이 실패했다.
    //
    // 왜 이 모양인가. 예전 이 경로는 받은 바이트를 그대로 흘려보냈고(프레임당
    // 0.035µs), 그래서 클라이언트가 위조한 S→C 프레임도 그대로 나갔다. 거르려면
    // 경계를 알아야 하는데, 랭크드 경로를 그대로 가져다 쓰면 프레임당 2.67µs —
    // 70배다. 다만 그 70배의 내역은 경계 판정이 아니다. 랭크드가 프레임마다
    // 하는 일은 넷이다: 체크섬 계산(페이로드 전체를 훑는다), 페이로드 파싱,
    // 버퍼 머리에서의 erase(O(n) 이동), 그리고 프레임당 send 한 번.
    //
    // 여기서는 그 넷을 전부 피한다. 읽는 것은 헤더 3바이트뿐이고(LEN 2 + TYPE 1),
    // 체크섬은 계산하지 않으며(서버 전용인지 판단하는 데 필요 없다), 통과한
    // 프레임은 복사하지 않고 구간의 끝만 늘렸다가 배치 끝에 한 번 send 하고,
    // erase 도 배치당 한 번이다. rx 가 이미 누적 버퍼라 별도 복사도 없다. 위조가
    // 없는 정상 트래픽에서 늘어나는 것은 프레임당 헤더 세 바이트를 읽는 비용뿐이다.
    // (위의 두 숫자는 이 저장소의 기존 측정치다. 이 구현을 다시 잰 값은 아니다 —
    // 벤치 python/tools/relay_shard_bench.py 는 Linux 전용이라 배포 대상에서
    // 돌려 확인해야 한다.)
    //
    // 대가가 없지는 않다. 잘린 프레임의 꼬리를 다음 읽기까지 들고 있어야 하므로
    // (경계를 모르면 거를 수 없다) 세그먼트 경계에 걸린 프레임은 예전보다 한 번
    // 늦게 나간다. 랭크드 경로가 이미 그렇게 동작하고 있고, 락스텝 프레임은 수십
    // 바이트라 한 세그먼트에 통째로 들어오는 것이 보통이다. 안전을 위해 이 정도는
    // 낸다 — 거르지 않는 빠른 경로는 "빠르다" 가 아니라 "신뢰 경계가 없다" 다.
    bool forward_screened(Conn* c, Conn* peer, Channel* ch) {
        size_t pos  = 0;   // 경계 판정이 끝난 위치
        size_t sent = 0;   // 여기까지는 보냈거나(통과) 버렸다(위반)
        bool   drop_rest = false;

        while (c->rx.size() - pos >= 2) {
            const uint8_t* p   = c->rx.data() + pos;
            const uint16_t len = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
            if ((size_t)len > net::kMaxPayloadBytes + 1u) {
                // 랭크드 경로와 같은 정책이다: 여기서부터는 경계를 믿을 수 없으니
                // 남은 바이트를 버린다. 앞의 정상 구간은 이미 보냈다.
                RLOG_WARN("[relay] match=" << ch->match_id
                          << " uuid=" << ch->match_uuid
                          << " 과대 프레임 — 스트림 폐기");
                drop_rest = true;
                break;
            }
            const size_t total = 2u + (size_t)len + 4u;
            if (c->rx.size() - pos < total) break;   // 미완성 — 뒤를 기다린다
            // len < 1 은 타입 바이트조차 없는 프레임이라 판정 대상이 아니다.
            // 예전처럼 그대로 흘려보낸다(구간에 남겨 둔다) — 무해하고, 여기서
            // 정책을 새로 만들면 거르기와 무관한 동작 변화가 섞인다.
            if (len >= 1 && net::is_server_only_type(p[2])) {
                if (pos > sent &&
                    !queue_send(peer, c->rx.data() + sent, pos - sent)) return false;
                note_server_only(c, p[2]);
                sent = pos + total;                  // 이 프레임만 건너뛴다
            }
            pos += total;
        }
        if (pos > sent && !queue_send(peer, c->rx.data() + sent, pos - sent))
            return false;
        if (drop_rest)  c->rx.clear();
        else if (pos)   c->rx.erase(c->rx.begin(), c->rx.begin() + pos);
        return true;
    }

    void on_forward(Conn* c) {
        Channel* ch = c->ch;
        if (!ch) return;
        Conn* peer = c->is_a ? ch->b : ch->a;
        timers_.arm(c, Clock::now() + kIdleTimeout);

        if (!ch->ranked) {
            // unranked: 경계는 훑되 내용은 보지 않는다 — 서버 전용 프레임만
            // 걸러내고 나머지는 원본 바이트 그대로 흘려보낸다.
            if (!c->rx.empty() && !forward_screened(c, peer, ch)) {
                close_conn(peer ? peer : c, "전달 실패");
                return;
            }
            return;
        }

        // ranked: MATCH_SUMMARY 만 가로채고 나머지는 원본 바이트 그대로 전달한다.
        size_t consumed = 0;
        while (c->rx.size() - consumed >= 2) {
            const uint8_t* p = c->rx.data() + consumed;
            const size_t avail = c->rx.size() - consumed;
            const uint16_t len = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
            if ((size_t)len > net::kMaxPayloadBytes + 1u) {
                RLOG_WARN("[relay] match=" << ch->match_id
                          << " uuid=" << ch->match_uuid
                          << " 과대 프레임 — 스트림 폐기");
                consumed = c->rx.size();
                break;
            }
            const size_t total = 2u + (size_t)len + 4u;
            if (avail < total) break;
            if (len < 1) { consumed += total; continue; }

            if (p[2] == (uint8_t)net::MsgType::MATCH_SUMMARY) {
                const size_t payload_len = (size_t)len - 1u;
                const uint32_t chk  = net::le_read_u32(p + 2u + len);
                const uint32_t calc = payload_len == 0 ? 0u : net::fnv1a32(p + 3, payload_len);
                Summary s{};
                if (chk == calc && parse_summary(p + 3, payload_len, s)) {
                    auto& slot = c->is_a ? ch->sumA : ch->sumB;
                    if (!slot) slot = s;
                    RLOG_DEBUG("[relay] match=" << ch->match_id
                               << " uuid=" << ch->match_uuid << " MATCH_SUMMARY from "
                               << (c->is_a ? "A" : "B")
                               << " player_id=" << c->player_id
                               << " won=" << (int)s.won);
                }
                consumed += total;   // 가로챔 — 상대에게 보내지 않는다
                continue;
            }
            if (net::is_server_only_type(p[2])) {
                note_server_only(c, p[2]);
                consumed += total;   // 버림 — 상대에게 보내지 않는다
                continue;
            }
            if (!queue_send(peer, p, total)) {
                close_conn(peer ? peer : c, "전달 실패");
                return;
            }
            consumed += total;
        }
        if (consumed) c->rx.erase(c->rx.begin(), c->rx.begin() + consumed);

        if (ch->sumA && ch->sumB && !ch->summary_handled) finalize_ranked(ch);
    }

    // ── 만기 ─────────────────────────────────────────────────────────────────
    void on_timeout(Conn* c) {
        switch (c->stage) {
            case Stage::FirstFrame: close_conn(c, "첫 프레임 타임아웃"); break;
            case Stage::Room:       close_conn(c, "룸 대기 타임아웃");   break;
            case Stage::Lobby:      close_conn(c, "로비 타임아웃");      break;
            case Stage::Forward:
                // 우리가 백프레셔로 입을 막아 둔 연결은 유휴가 아니다 — 읽기 이벤트가
                // 안 나는 게 당연하다. 여기서 끊으면 "느리게 읽는 상대" 때문에 "정상
                // 플레이어" 가 끊긴다. 멈춰 있을 수 있는 시간은 tx 하드 상한이 따로
                // 묶으므로, 여기서는 데드라인만 다시 건다.
                if (c->read_paused) { timers_.arm(c, Clock::now() + kIdleTimeout); break; }
                close_conn(c, "idle 타임아웃");
                break;
            default: break;
        }
    }

    // ── finalize ─────────────────────────────────────────────────────────────
    // 상대가 사라졌다. 요약 수집 상태에 따라 세 갈래 — 스레드 모델과 같은 정책이다.
    // 상대가 사라진 매치에서 살아남은 쪽을 통지하고 닫는다.
    // 결과 프레임(랭크드)이 있다면 그것을 보낸 뒤에 불러야 한다 — tcp_close 는
    // shutdown(RDWR) 이라 먼저 닫으면 결과가 나가지 못한다.
    void close_channel_survivor(Channel* ch, const char* why) {
        Conn* s = ch->a ? ch->a : ch->b;
        if (!s || s->stage == Stage::Dead) return;
        if (s->stage == Stage::Lobby) {
            // 수락 로비에서는 "상대가 수락하지 않았다" 를 READY(0) 으로 알린다.
            // 스레드 모델이 보내던 것과 같은 프레임이라 클라이언트가 이미 안다.
            std::vector<uint8_t> pl{0};
            auto fr = net::build_frame(net::MsgType::READY, pl);
            queue_send(s, fr.data(), fr.size());
        }
        close_conn(s, why);
    }

    void on_channel_peer_lost(Channel* ch) {
        if (!ch->ranked || ch->summary_handled || ch->finalize_inflight) return;
        if (ch->sumA && ch->sumB) { finalize_ranked(ch); return; }
        if (!ch->sumA && !ch->sumB) {
            // 무경기 — meta 에 보내지 않는다(담합 RP 파밍·동시 단절 오염 차단).
            ch->summary_handled = true;
            send_result_frames(ch, ch->a_elo, ch->a_elo, 0, ch->b_elo, ch->b_elo, 0);
            RLOG_INFO("[relay] match=" << ch->match_id << " uuid=" << ch->match_uuid
                      << " player_id=" << ch->a_id << " x " << ch->b_id
                      << " 요약 없음 -> meta 미전송 (delta=0)");
            return;
        }
        finalize_forfeit(ch);
    }

    void finalize_ranked(Channel* ch) {
        if (ch->summary_handled || ch->finalize_inflight) return;
        ch->summary_handled = true;
        const Summary a = *ch->sumA, b = *ch->sumB;
        const bool exclusive = (a.won ^ b.won) != 0;
        const bool scores_ok = (a.my_score == b.opp_score) && (b.my_score == a.opp_score);
        const bool lines_ok  = (a.my_lines == b.opp_lines) && (b.my_lines == a.opp_lines);
        std::optional<int64_t> winner;
        if (exclusive && scores_ok && lines_ok) {
            winner = (a.won == 1) ? ch->a_id : ch->b_id;
        } else {
            RLOG_WARN("[relay] match=" << ch->match_id << " uuid=" << ch->match_uuid
                      << " player_id=" << ch->a_id << " x " << ch->b_id
                      << " 교차검증 실패 -> winner=null");
        }
        post_result(ch, winner, (int)a.my_score, (int)b.my_score,
                    (int)a.my_lines, (int)b.my_lines,
                    (int)std::max(a.duration_s, b.duration_s));
    }

    void finalize_forfeit(Channel* ch) {
        ch->summary_handled = true;
        const bool haveA = ch->sumA.has_value();
        const Summary s = haveA ? *ch->sumA : *ch->sumB;
        // 한쪽 요약만 있으면 그 요약의 won 을 존중한다 — 끊긴 순서로 승자를 정하면
        // 승리 요약을 낸 직후 회선이 끊긴 쪽이 패자로 뒤집힌다.
        const int64_t winner = haveA ? (s.won ? ch->a_id : ch->b_id)
                                     : (s.won ? ch->b_id : ch->a_id);
        const int scoreA = (int)(haveA ? s.my_score : s.opp_score);
        const int scoreB = (int)(haveA ? s.opp_score : s.my_score);
        const int linesA = (int)(haveA ? s.my_lines : s.opp_lines);
        const int linesB = (int)(haveA ? s.opp_lines : s.my_lines);
        post_result(ch, winner, scoreA, scoreB, linesA, linesB, (int)s.duration_s);
    }

    // meta POST 는 블로킹이라 워커로 뺀다. 결과 프레임 송신은 루프가 한다.
    void post_result(Channel* ch, std::optional<int64_t> winner,
                     int sa, int sb, int la, int lb, int dur) {
        if (!meta_) return;
        ch->finalize_inflight = true;
        const uint32_t mid = ch->match_id;
        meta::client::MetaClient* meta = meta_;
        const std::string uuid = ch->match_uuid;
        const int64_t aid = ch->a_id, bid = ch->b_id;

        const bool queued = offload_->submit(
            [this, meta, uuid, aid, bid, winner, sa, sb, la, lb, dur, mid]() -> Offload::Cont {
                auto res = meta->post_match(uuid, aid, bid, winner, sa, sb, la, lb, dur);
                return [this, mid, res]() { on_result_saved(mid, res); };
            });
        if (!queued) {
            // 종료 중이라 저장할 수 없다 — 결과를 삼키지 말고 남긴다.
            RLOG_WARN("[relay] match=" << mid << " uuid=" << uuid
                      << " 종료 중 — meta 저장 생략");
            ch->finalize_inflight = false;
        }
    }

    void on_result_saved(uint32_t match_id,
                         std::optional<meta::client::MatchResult> res) {
        auto it = channels_.find(match_id);
        if (it == channels_.end()) return;
        Channel* ch = it->second.get();
        ch->finalize_inflight = false;
        if (res) {
            send_result_frames(ch, res->a.elo_before, res->a.elo_after, res->a.delta,
                                   res->b.elo_before, res->b.elo_after, res->b.delta);
            RLOG_INFO("[relay] match=" << match_id << " uuid=" << ch->match_uuid
                      << " meta 저장 완료");
        } else {
            send_result_frames(ch, ch->a_elo, ch->a_elo, 0, ch->b_elo, ch->b_elo, 0);
            RLOG_WARN("[relay] match=" << match_id << " uuid=" << ch->match_uuid
                      << " meta POST 실패 — delta=0");
        }
        // 결과를 보낸 지금이 남은 쪽을 닫을 자리다. 상대가 이미 사라졌을 때
        // close_conn 이 여기로 미뤄 둔 일이다.
        if (ch->close_survivor_pending) {
            ch->close_survivor_pending = false;
            close_channel_survivor(ch, "상대 이탈");
        }
    }

    // 채널이 붙들고 있는 소켓 복사본으로 보낸다 — Conn 이 이미 사라졌어도 된다.
    void send_result_frames(Channel* ch, int ab, int aa, int ad, int bb, int ba, int bd) {
        auto frA = build_match_result(ab, aa, ad);
        auto frB = build_match_result(bb, ba, bd);
        size_t sent = 0;
        if (ch->disconnect_side != 1 && ch->sockA.valid())
            net::tcp_send_some(ch->sockA, frA.data(), frA.size(), sent);
        if (ch->disconnect_side != 2 && ch->sockB.valid())
            net::tcp_send_some(ch->sockB, frB.data(), frB.size(), sent);
    }

    // ── 종료 ─────────────────────────────────────────────────────────────────
    void shutdown() {
        RLOG_INFO("[relay] shutting down...");
        // 새 job 을 막고 이미 큐에 있는 것(진짜 끝난 경기의 결과 저장)은 마친다.
        // 큐 깊이는 그 순간 종료된 매치 수로 한정되고, 새 연결을 받지 않으므로
        // 드레인 중에 자라지 않는다.
        offload_->shutdown();
        std::vector<Offload::Cont> conts;
        offload_->drain(conts);
        for (auto& c : conts) c();

        for (auto& [ptr, up] : conns_) {
            if (up->stage != Stage::Dead) net::tcp_close(up->sock);
        }
        conns_.clear();
        channels_.clear();
        net::tcp_close(listen_);
        RLOG_INFO("[relay] done");
    }

    meta::client::MetaClient* meta_ = nullptr;
    std::string meta_note_;

    std::unique_ptr<net::Reactor> reactor_;
    std::unique_ptr<Offload>      offload_;
    TimerQueue                    timers_;

    net::TcpSocket listen_;
    char           listen_token_ = 0;
    bool           is_front_ = false;   // 리스너를 가진 루프 — 상태 줄 담당
    TimePoint      next_stats_{};       // epoch = 아직 한 번도 안 찍음

    std::unordered_map<Conn*, std::unique_ptr<Conn>>     conns_;
    std::unordered_map<uint32_t, std::unique_ptr<Channel>> channels_;
    std::unordered_map<std::string, std::unique_ptr<Room>>  rooms_;
    std::deque<Conn*>          queue_;
    std::vector<Conn*>         dying_;
    std::unordered_set<uint32_t> pending_auth_;

    // 샤딩. shards_ 는 앞단만 채운다(샤드에서는 비어 있어 재인계가 일어나지 않는다).
    std::vector<RelayLoop*> shards_;
    size_t                  shard_index_ = 0;
    size_t                  next_shard_  = 0;
    std::mutex              inbox_mu_;
    std::vector<Handoff>    inbox_;

    uint32_t next_conn_id_  = 1;
    uint32_t next_match_id_ = 1;
    uint64_t seed_state_    = 0;
};

} // namespace
} // namespace relay

namespace {

// 숫자 인자 파싱. std::stoi 는 숫자가 아닌 입력에 예외를 던지는데, 여기서 잡지
// 않으면 terminate 로 죽어 "std::invalid_argument" 스택 덤프만 남는다. 오타 하나
// 든 systemd 유닛이 그런 식으로 죽으면 운영자는 무엇이 틀렸는지 알 수 없다.
// 스레드 모델(server/main.cpp)이 from_chars 로 이미 이렇게 하고 있어, 두 바이너리
// 의 잘못된 입력 처리도 같은 모양이 된다. 부분 파싱("12abc")도 거절한다 —
// from_chars 가 멈춘 위치가 끝이 아니면 입력 전체가 숫자가 아니었다는 뜻이다.
bool parse_int_arg(const std::string& s, const char* what, int lo, int hi, int& out)
{
    int value = 0;
    const auto* first = s.data();
    const auto* last  = s.data() + s.size();
    const auto  res   = std::from_chars(first, last, value);
    if (res.ec != std::errc{} || res.ptr != last) {
        RLOG_ERROR("[relay] " << what << " 는 정수여야 합니다: " << s);
        return false;
    }
    if (value < lo || value > hi) {
        RLOG_ERROR("[relay] " << what << " 는 " << lo << ".." << hi
                   << " 범위여야 합니다: " << value);
        return false;
    }
    out = value;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    uint16_t port = 7777;
    int loops = 1;                      // 1 = 단일 루프(앞단이 포워딩까지)
    std::string meta_url, meta_secret;

    // 환경변수 먼저, 인자 나중 — systemd 유닛에 기본값을 박아 두고 조사할 때만
    // 명령줄로 덮어쓰는 흐름이 자연스럽다. 잘못된 값은 조용히 무시하지 않고
    // 알린 뒤 기본값을 쓴다(기동은 막지 않는다 — 로그 설정 하나로 서버가 안
    // 뜨는 것이 더 나쁜 실패다).
    if (const char* env = std::getenv("TETRIS_RELAY_LOG_LEVEL")) {
        relay::LogLevel lv{};
        if (relay::parse_log_level(env, lv)) relay::set_log_level(lv);
        else RLOG_WARN("[relay] TETRIS_RELAY_LOG_LEVEL 값을 알 수 없어 무시합니다: "
                       << env);
    }

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                RLOG_ERROR("[relay] " << what << " 인자 누락");
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--port") {
            int n = 0;
            if (!parse_int_arg(next("--port"), "--port", 1, 65535, n)) return 2;
            port = (uint16_t)n;
        }
        else if (a == "--loops") {
            int n = 0;
            if (!parse_int_arg(next("--loops"), "--loops", 1, 256, n)) return 2;
            loops = n;
        }
        else if (a == "--meta")        meta_url = next("--meta");
        else if (a == "--meta-secret") meta_secret = next("--meta-secret");
        else if (a == "--max-sessions-per-ip") {
            int n = 0;
            if (!parse_int_arg(next("--max-sessions-per-ip"),
                               "--max-sessions-per-ip", 1, 100000, n)) return 2;
            relay::IpAdmission::set_session_limit((size_t)n);
        }
        else if (a == "--max-conns") {
            int n = 0;
            if (!parse_int_arg(next("--max-conns"),
                               "--max-conns", 2, 1000000, n)) return 2;
            relay::g_max_conns = (size_t)n;
        }
        else if (a == "--max-tx-mib") {
            int n = 0;
            if (!parse_int_arg(next("--max-tx-mib"),
                               "--max-tx-mib", 1, 65536, n)) return 2;
            relay::g_tx_budget = (size_t)n * 1024u * 1024u;
        }
        else if (a == "--max-pending-auth") {
            int n = 0;
            if (!parse_int_arg(next("--max-pending-auth"),
                               "--max-pending-auth", 1, 100000, n)) return 2;
            relay::g_max_pending_auth = (size_t)n;
        }
        else if (a == "--log-level") {
            const std::string v = next("--log-level");
            relay::LogLevel lv{};
            if (!relay::parse_log_level(v, lv)) {
                RLOG_ERROR("[relay] --log-level 은 error|warn|info|debug 여야 합니다: "
                           << v);
                return 2;
            }
            relay::set_log_level(lv);
        }
        else if (a == "--stats-interval-sec") {
            int n = 0;
            if (!parse_int_arg(next("--stats-interval-sec"),
                               "--stats-interval-sec", 0, 86400, n)) return 2;
            relay::g_stats_interval_sec = n;
        }
        else if (a == "-h" || a == "--help") {
            std::cout <<
                "Usage: tetris_relay_reactor [--port N] [--loops N] [--meta URL]\n"
                "                            [--meta-secret S] [--max-sessions-per-ip N]\n"
                "                            [--max-conns N] [--max-tx-mib N]\n"
                "                            [--max-pending-auth N]\n"
                "                            [--log-level L] [--stats-interval-sec N]\n"
                "  이벤트 루프(epoll/IOCP) 릴레이. 큐 경로와 커스텀 룸 경로를 모두 지원.\n"
                "\n"
                "  --loops N   루프 스레드 수 (기본 1). 앞단 루프 하나가 accept·인증·큐·\n"
                "              룸·로비를 전부 소유하고, 포워딩은 샤드 (N-1)개가 나눠 갖는다.\n"
                "              앞단은 샤드가 하나라도 있으면 포워딩을 전부 넘기고 자기는\n"
                "              하지 않으므로 포워딩의 실효 병렬도는 loops-1 이다. 그래서\n"
                "              --loops 2 는 --loops 1 과 일꾼 수가 같아 이득이 없고,\n"
                "              릴레이가 이를 단일 루프로 낮춰 실행한다. 실제로 나누려면 3 이상.\n"
                "              소켓을 루프 사이로 옮길 수 없는 백엔드(Windows IOCP)에서는\n"
                "              값과 무관하게 단일 루프로 실행한다.\n"
                "  --max-sessions-per-ip N\n"
                "              한 주소가 연결 수명 동안 붙들 수 있는 동시 연결 수\n"
                "              (기본 " << relay::kMaxSessionsPerIp << "). 인증이 끝나면 반납하는 per-IP 핸드셰이크\n"
                "              예산(" << relay::kMaxHandshakesPerIp << ")과는 별개로 검사한다.\n"
                "  --max-conns N\n"
                "              프로세스 전체 동시 연결 상한 (기본 "
                                    << relay::kDefaultMaxConns << "). 먼저 오는 것은\n"
                "              보통 이 상한이 아니라 루프 포화이며, 그때 늘릴 것은\n"
                "              --loops 다. 이 값은 fd 를 지키는 마지막 방어선이다.\n"
                "  --max-tx-mib N\n"
                "              보류 송신의 프로세스 전체 예산, MiB (기본 "
                                    << relay::kDefaultTxBudget / (1024 * 1024) << ").\n"
                "              연결당 상한만으로는 메모리가 --max-conns 와 곱해져\n"
                "              천장이 조용히 따라 오른다. 이 예산이 그 곱셈을 끊는다 —\n"
                "              넘기면 그 순간 넘긴 연결을 끊는다.\n"
                "  --max-pending-auth N\n"
                "              meta 인증 왕복을 동시에 몇 명까지 기다리게 할지 (기본 "
                                    << relay::kDefaultMaxPendingAuth << ").\n"
                "              끊긴 연결의 인증 작업은 취소되므로 이 수는 아직 살아서\n"
                "              기다리는 사람만 센다. 넘기면 세우는 대신 사유를 밝히고\n"
                "              거절한다 — meta 가 느릴수록 낮게 잡아야 줄이 짧아진다.\n"
                "  --log-level L\n"
                "              error|warn|info|debug (기본 info). 환경변수\n"
                "              TETRIS_RELAY_LOG_LEVEL 로도 정할 수 있고 이 인자가 이긴다.\n"
                "              info 는 거절·종료·매치 수명·주기 상태까지, debug 는\n"
                "              접속 하나하나와 인증·큐·룸 진행까지 남긴다. 포워딩\n"
                "              경로에는 어느 레벨에서도 로그가 없다.\n"
                "  --stats-interval-sec N\n"
                "              상태 한 줄의 주기, 초 (기본 "
                                    << relay::kDefaultStatsIntervalSec << ", 0=끔).\n"
                "              동시 연결·활성 매치·tx 예산 사용량과 최고 수위·사유별\n"
                "              거절 카운터를 한 줄에 낸다.\n";
            return 0;
        }
    }

    // --loops 2 는 아무것도 사지 못한다. 포워딩 일꾼 수가 loops-1 이므로
    // loops=2 의 일꾼은 1개 — 앞단이 직접 포워딩하는 loops=1 과 같은 수다.
    // 측정에서도 두 구성의 차이는 0.14%(374,360 vs 373,827 frames/s)로 오차
    // 범위였고, 늘어나는 것은 스레드 하나와 매치마다 도는 우편함 인계뿐이다.
    //
    // 거절(exit 2)이 아니라 1 로 낮추는 쪽을 골랐다: --loops 2 는 틀린 설정이
    // 아니라 무의미한 설정이고, 튜닝 값 하나 때문에 기동을 거부하면 이미 그
    // 인자를 박아 둔 배포·벤치 스크립트가 통째로 멈춘다. 낮춘 구성은 같은
    // 처리량에 스레드는 하나 적고 인계도 없으므로 엄격히 낫다. 다만 조용히
    // 넘어가면 사용자는 손해를 본 줄도 모르므로 이유를 찍는다.
    if (loops == 2) {
        RLOG_INFO("[relay] --loops 2 는 포워딩 일꾼이 1개로 --loops 1 과 같고"
                  " (실효 병렬도 = loops-1) 스레드와 매치 인계 비용만 늘어"
                  " 단일 루프로 실행합니다 — 실제로 나누려면 --loops 3 이상");
        loops = 1;
    }
    if (meta_secret.empty()) {
        if (const char* env = std::getenv("TETRIS_RELAY_SECRET")) meta_secret = env;
    }

    std::signal(SIGINT,  relay::on_signal);
    std::signal(SIGTERM, relay::on_signal);
#if defined(_WIN32)
    std::signal(SIGBREAK, relay::on_signal);
#endif

    if (!net::net_init()) {
        RLOG_ERROR("[relay] net_init 실패");
        return 1;
    }

    std::unique_ptr<meta::client::MetaClient> meta;
    std::string note = "meta=none (unranked mode)";
    if (!meta_url.empty()) {
        // secret 없이 ranked 로 뜨면 meta 가 POST /v1/matches 를 거절하므로 경기
        // 결과를 하나도 저장하지 못한다. 그 상태로 조용히 도는 대신 기동을 거부한다.
        if (meta_secret.empty()) {
            RLOG_ERROR("[relay] refusing to start: --meta set but no relay secret. "
                       << "Set --meta-secret or TETRIS_RELAY_SECRET (meta rejects "
                       << "POST /v1/matches without it).");
            net::net_shutdown();
            return 2;
        }
        meta = std::make_unique<meta::client::MetaClient>(meta_url, meta_secret);
        if (!meta->valid()) {
            RLOG_ERROR("[relay] invalid --meta URL: " << meta_url);
            net::net_shutdown();
            return 2;
        }
        note = "meta=" + meta_url;
    }

    // 앞단 루프 하나 + 포워딩 샤드 (loops-1)개 — 포워딩의 실효 병렬도가 loops-1
    // 인 이유가 이것이다. --loops 1 이면 단일 루프 모드로 앞단이 포워딩까지 직접
    // 한다 — 이 저장소 규모에서는 그쪽이 기본이다.
    relay::RelayLoop front(meta.get(), note);
    if (!front.init(port)) {
        net::net_shutdown();
        return 1;
    }
    RLOG_INFO("[relay] per-IP limits: handshakes=" << relay::kMaxHandshakesPerIp
              << " sessions=" << relay::IpAdmission::session_limit());

    if (loops > 1 && !front.can_shard()) {
        // 소켓을 다른 완료 포트로 옮길 수 없는 백엔드(IOCP)에서는 인계가 성립하지
        // 않는다. 조용히 반쯤 도는 대신 이유를 밝히고 단일 루프로 물러선다.
        RLOG_INFO("[relay] 이 플랫폼의 reactor 백엔드는 루프 간 소켓 이동을 "
                  "지원하지 않아 단일 루프로 실행합니다");
        loops = 1;
    }

    std::vector<std::unique_ptr<relay::RelayLoop>> shards;
    std::vector<relay::RelayLoop*>                 shard_ptrs;
    for (int i = 1; i < loops; ++i) {
        auto s = std::make_unique<relay::RelayLoop>(meta.get(), note);
        if (!s->init_shard((size_t)i)) {
            net::net_shutdown();
            return 1;
        }
        shard_ptrs.push_back(s.get());
        shards.push_back(std::move(s));
    }
    front.set_shards(shard_ptrs);
    if (!shard_ptrs.empty()) {
        RLOG_INFO("[relay] forwarding shards: " << shard_ptrs.size());
    }

    std::vector<std::thread> threads;
    for (auto* s : shard_ptrs) threads.emplace_back([s] { s->run(); });

    front.run();                       // 앞단은 이 스레드에서 돈다
    for (auto& th : threads) th.join(); // g_running 이 내려가면 샤드도 함께 빠져나온다

    net::net_shutdown();
    return 0;
}
