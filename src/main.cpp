// src/main.cpp — Handmade 버전 (raylib 제거, Win32 + OpenGL 직접 사용)
//
// 변경 사항:
//   #include <raylib.h>     → platform/platform.h + renderer/renderer.h
//   InitWindow()            → platform_init()
//   WindowShouldClose()     → platform_should_close()
//   GetFrameTime()          → platform_begin_frame() 반환값
//   BeginDrawing()          → renderer_begin()
//   EndDrawing()            → renderer_end() + platform_end_frame()
//   IsKeyPressed(KEY_*)     → platform_key_pressed(PKEY_*)
//   IsKeyDown(KEY_*)        → platform_key_down(PKEY_*)
//   GetCharPressed()        → platform_get_char_pressed()
//   DrawRectangle(...)      → draw_rect(...)
//   DrawRectangleRounded()  → draw_rect_rounded(...)
//   DrawTextEx(font, ...)   → draw_text(...)
//   DrawText(...)           → draw_text(...)
//   MeasureTextEx(...)      → measure_text(...)
//   TextFormat(fmt, ...)    → snprintf 로컬 버퍼
//   GetTime()               → platform_get_time()
//   TraceLog(LOG_ERROR, ...) → fprintf(stderr, ...)
//   UpdateMusicStream(...)  → audio/ 모듈 (XAudio2 루프 재생, per-frame 업데이트 불필요)
//   CloseWindow()           → platform_shutdown()
//   WHITE, GRAY, GREEN, YELLOW, RED, RAYWHITE → platform.h 의 동명 상수
//   Vector2                 → float x, y 직접 사용

#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <charconv>
#include "../core/constants.h"
#include "../core/input.h"
#include "../core/replay.h"
#include "../core/hash.h"
#include "../net/session.h"
#include "../net/socket.h"
#include "../net/framing.h"
#include <thread>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <filesystem>
#include <future>
#include <chrono>
#include "game.h"
#include "gui.h"
#include "colors.h"
#include "../platform/platform.h"
#include "../renderer/renderer.h"
#include "../renderer/shake.h"
#include "../renderer/image.h"
#include "../bot/bot_onnx.h"
#include "../bot/placement.h"
#include <deque>

// TextFormat 대체: snprintf 로 포맷 문자열을 임시 버퍼에 쓰고 반환.
// raylib의 TextFormat은 정적 버퍼를 쓰므로 중첩 호출에 주의.
static const char* fmt_buf(const char* fmt, ...)
{
    static char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    return buf;
}

// 키보드 입력 → 비트마스크 (core/input.h 의 INPUT_* 상수)
//
// platform_key_pressed()는 "이번 프레임에 처음 눌림"을 감지하는 엣지 트리거.
// vsync 없이 FPS가 수천이면 60Hz 틱 사이에 수십 프레임이 지나가므로,
// 눌린 프레임과 틱 프레임이 어긋나면 입력이 소실된다.
// → 매 프레임 AccumulateInput()으로 엣지 입력을 누적하고,
//   틱에서 ConsumeInput()으로 소비 + held 키(DOWN/LEFT/RIGHT)를 합산한다.
//   좌우 held 반복은 DAS/ARR로 속도를 제한한다.
static uint8_t s_pendingInput = 0;
static int s_leftHoldTicks = 0;
static int s_rightHoldTicks = 0;

static uint8_t HorizontalRepeatInput()
{
    constexpr int kHorizontalDasTicks = 10; // 60Hz 기준 약 167ms
    constexpr int kHorizontalArrTicks = 2;  // DAS 이후 30칸/초

    const bool leftDown = platform_key_down(PKEY_LEFT);
    const bool rightDown = platform_key_down(PKEY_RIGHT);
    if (leftDown == rightDown)
    {
        s_leftHoldTicks = 0;
        s_rightHoldTicks = 0;
        return 0;
    }

    int& ticks = leftDown ? s_leftHoldTicks : s_rightHoldTicks;
    int& otherTicks = leftDown ? s_rightHoldTicks : s_leftHoldTicks;
    otherTicks = 0;

    uint8_t bit = leftDown ? INPUT_LEFT : INPUT_RIGHT;
    uint8_t out = 0;
    if (ticks >= kHorizontalDasTicks &&
        ((ticks - kHorizontalDasTicks) % kHorizontalArrTicks) == 0)
    {
        out = bit;
    }
    ticks++;
    return out;
}

static void AccumulateInput(bool suppress = false)
{
    if (suppress)
    {
        s_pendingInput = 0;
        s_leftHoldTicks = 0;
        s_rightHoldTicks = 0;
        return;
    }  // 채팅 입력 중 — 게임 키 흡수 중단
    if (platform_key_pressed(PKEY_LEFT))  s_pendingInput |= INPUT_LEFT;
    if (platform_key_pressed(PKEY_RIGHT)) s_pendingInput |= INPUT_RIGHT;
    if (platform_key_pressed(PKEY_UP))    s_pendingInput |= INPUT_ROTATE;
    if (platform_key_pressed(PKEY_SPACE)) s_pendingInput |= INPUT_DROP;
}

static uint8_t ConsumeInput(bool suppress = false)
{
    uint8_t mask = s_pendingInput;
    s_pendingInput = 0;
    if (suppress)
    {
        s_leftHoldTicks = 0;
        s_rightHoldTicks = 0;
        return 0;
    }

    const bool leftDown = platform_key_down(PKEY_LEFT);
    const bool rightDown = platform_key_down(PKEY_RIGHT);
    if (leftDown && rightDown)
    {
        mask &= static_cast<uint8_t>(~(INPUT_LEFT | INPUT_RIGHT));
    }
    mask |= HorizontalRepeatInput();
    if (platform_key_down(PKEY_DOWN)) mask |= INPUT_DOWN;
    return mask;
}

// `host:port` 형태 엔드포인트 파서. IPv6 브래킷 표기(`[::1]:7777`)도 허용.
//  - port 미지정 시 default_port 사용
//  - `:` 없거나 host 가 비었거나 port 가 0/범위초과 이면 false
//  - 예외 없이 std::from_chars 로 파싱 — 악의적 입력에도 크래시 안 함
static bool parse_endpoint(const std::string& s,
                           std::string& host_out,
                           uint16_t& port_out,
                           uint16_t default_port)
{
    if (s.empty()) return false;

    // IPv6 브래킷: `[addr]` 또는 `[addr]:port`
    if (s.front() == '[') {
        auto rb = s.find(']');
        if (rb == std::string::npos || rb == 1) return false;  // `[]` 또는 닫힘 없음
        host_out = s.substr(1, rb - 1);
        if (rb + 1 == s.size()) {                               // port 생략
            port_out = default_port; return true;
        }
        if (s[rb + 1] != ':') return false;                     // `]` 뒤에 `:` 만 허용
        std::string portStr = s.substr(rb + 2);
        if (portStr.empty()) return false;
        unsigned long p = 0;
        auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), p);
        if (ec != std::errc() || ptr != portStr.data() + portStr.size()) return false;
        if (p < 1 || p > 65535) return false;
        port_out = (uint16_t)p; return true;
    }

    // IPv4 / hostname: `host` 또는 `host:port`
    auto pos = s.find(':');
    if (pos == std::string::npos) {                             // port 생략
        host_out = s; port_out = default_port;
        return !host_out.empty();
    }
    if (pos == 0) return false;                                 // host 비어 있음
    host_out = s.substr(0, pos);
    std::string portStr = s.substr(pos + 1);
    if (portStr.empty()) return false;
    unsigned long p = 0;
    auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), p);
    if (ec != std::errc() || ptr != portStr.data() + portStr.size()) return false;
    if (p < 1 || p > 65535) return false;
    port_out = (uint16_t)p; return true;
}

// `--host` 용 순수 포트 파서. 1..65535 만 허용.
static bool parse_port(const std::string& s, uint16_t& port_out)
{
    if (s.empty()) return false;
    unsigned long p = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), p);
    if (ec != std::errc() || ptr != s.data() + s.size()) return false;
    if (p < 1 || p > 65535) return false;
    port_out = (uint16_t)p; return true;
}

enum class AppMode {
    Menu, ConnectInput, Single, BotSingle, Net,
    // Section D — 커스텀 룸 경로. 릴레이 주소는 CLI 기본값 사용이라
    // 별도 IP 입력 화면(이전의 MatchmakingAddr / RoomRelay)은 제거됨.
    RoomLobby,    // Create / Join 선택 (+ Join 시 코드 입력)
    RoomWaiting,  // 방 안에서 상대/Ready 대기
};

enum class RoomLobbyStage { Choose, EnterCode };

enum class GameOverState {
    None,
    ShowingGameOver,
    WaitingForRemote,
    ShowingDisagreement,
    SendingNewSeed,
    WaitingForNewSeed,
    RestartingGame,
    GoingToTitle,
};

int main(int argc, char** argv)
{
    if (!net::net_init()) {
        fprintf(stderr, "Failed to initialize networking\n");
        return 1;
    }

    // ── 플랫폼 + 렌더러 초기화 ─────────────────────────────────────────────
    platform_init(720, 640, "Tetris (Handmade)");
    renderer_init(720, 640);
    renderer_load_font("Font/monogram.ttf");

    // 아이콘 (선택) — 파일이 없으면 0 이 리턴되고 draw_image 는 no-op.
    //   YOU       : 로컬 플레이어 슬롯 (모든 2-보드 모드)
    //   OPPONENT  : 상대 슬롯 (Net / CustomRoom)
    //   BOT       : BotSingle 모드 상대 슬롯
    //   사용자가 assets/icons/ 에 드롭하면 자동 반영. 파일이 없어도 프로그램은
    //   정상 작동하며, 단지 아이콘 쿼드 자리만 비어 있게 됨.
    ImageHandle iconYou      = image_load("assets/icons/player.png");
    ImageHandle iconOpponent = image_load("assets/icons/opponent.png");
    ImageHandle iconBot      = image_load("assets/icons/bot.png");

    bool netMode = false, isHost = false, queueMode = false;
    std::string hostIp;
    uint16_t hostPort = 7777;
    std::string queueHost;
    uint16_t queuePort = 7777;

    // 메뉴에서 Matchmaking/Custom Room 을 고를 때 사용할 릴레이 주소.
    // 기본은 로컬 릴레이. 다른 공개 릴레이로 바꾸려면 `--relay host:port`.
    std::string relayHost = "127.0.0.1";
    uint16_t    relayPort = 7777;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--host") {
            isHost = true; netMode = true;
            if (i + 1 < argc) {
                std::string portStr = argv[++i];
                if (!parse_port(portStr, hostPort)) {
                    fprintf(stderr, "error: --host expects a port in 1..65535, got '%s'\n", portStr.c_str());
                    return 2;
                }
            }
        } else if (a == "--connect") {
            netMode = true;
            if (i + 1 < argc) {
                std::string ep = argv[++i];
                if (!parse_endpoint(ep, hostIp, hostPort, 7777)) {
                    fprintf(stderr, "error: --connect expects host[:port], got '%s'\n", ep.c_str());
                    return 2;
                }
            } else {
                fprintf(stderr, "error: --connect requires an argument (host[:port])\n");
                return 2;
            }
        } else if (a == "--queue") {
            netMode = true; queueMode = true;
            if (i + 1 < argc) {
                std::string ep = argv[++i];
                if (!parse_endpoint(ep, queueHost, queuePort, 7777)) {
                    fprintf(stderr, "error: --queue expects host[:port], got '%s'\n", ep.c_str());
                    return 2;
                }
            } else {
                fprintf(stderr, "error: --queue requires an argument (host[:port])\n");
                return 2;
            }
        } else if (a == "--relay") {
            if (i + 1 < argc) {
                std::string ep = argv[++i];
                if (!parse_endpoint(ep, relayHost, relayPort, 7777)) {
                    fprintf(stderr, "error: --relay expects host[:port], got '%s'\n", ep.c_str());
                    return 2;
                }
            } else {
                fprintf(stderr, "error: --relay requires an argument (host[:port])\n");
                return 2;
            }
        }
    }

    uint64_t sessionSeed = 0xDEADBEEFCAFEBABEull;
    net::Session session;
    uint32_t startDelay = 120;
    uint8_t  inputDelay = 2;
    uint32_t localTickNext = 0;
    uint32_t simTick = 0;
    std::unordered_map<uint32_t, uint8_t> localInputs;

    AppMode app = netMode ? AppMode::Net : AppMode::Menu;
    int menuIndex = 0;
    std::string connectText;
    std::string connectErrorMsg;
    bool connectError = false;

    // Section D — 커스텀 룸 클라이언트 UI 상태.
    std::string     roomRelayHost;              // RoomRelay 입력 후 저장
    uint16_t        roomRelayPort = 7777;
    RoomLobbyStage  roomStage = RoomLobbyStage::Choose;
    std::string     roomCodeInput;              // Join 시 유저가 타이핑하는 코드
    std::string     roomErrorMsg;               // Lobby 내 에러 표시
    bool            roomLocalReady = false;     // 내가 Ready 했는지 (RoomWaiting)

    // Section E — 채팅 상태. 게임 중 T 로 입력 모드 진입, Enter 송신.
    //   chatComposing : true 면 키 입력을 채팅 버퍼로 흡수 (게임 이동 입력 중단)
    //   chatInput     : 현재 타이핑 중인 문자열
    //   chatHistory   : 최근 수신/송신 메시지 (최대 kChatHistoryMax 줄)
    constexpr size_t kChatMaxChars     = 200;   // 한 메시지 최대 길이
    constexpr size_t kChatHistoryMax   = 4;     // 화면에 동시에 보이는 줄 수
    constexpr float  kChatFadeSeconds  = 10.0f; // 표시 후 사라지는 시간
    bool        chatComposing = false;
    std::string chatInput;
    struct ChatLine { std::string prefix; std::string text; float ageSec; };
    std::deque<ChatLine> chatHistory;

    if (app == AppMode::Net) {
        if (queueMode) {
            // 비동기 큐잉 — 즉시 리턴, 매칭 대기 중에도 렌더 루프는 계속 돈다.
            // isReady() 가 true 가 되면 seed/role 이 session.params() 에 채워져 있다.
            if (!session.QueueJoin(queueHost, queuePort, startDelay, inputDelay)) {
                fprintf(stderr, "Queue join failed to start\n"); return 1;
            }
        } else if (isHost) {
            sessionSeed = (uint64_t)(platform_get_time() * 1000000.0) + 0xC0FFEEULL;
            net::SeedParams sp{sessionSeed, startDelay, inputDelay, net::Role::Host};
            if (!session.Host(hostPort, sp)) {
                fprintf(stderr, "Host failed\n"); return 1;
            }
        } else {
            if (!session.Connect(hostIp, hostPort)) {
                fprintf(stderr, "Connect failed\n"); return 1;
            }
        }
    }

    std::unique_ptr<Game> gameSingle;
    std::unique_ptr<Game> gameLocal;
    std::unique_ptr<Game> gameRemote;

    // 인게임 "나가기" 모달 상태.
    //   Single/BotSingle: 모달 열리면 시뮬 일시정지 (accumulator 증가 중단).
    //   Net:              모달 열려도 게임은 계속 진행 (lockstep 동기 유지).
    // Yes 선택 시 메인메뉴로, Multi 는 session 을 close 해 상대에게 연결 단절
    //   로 전달 (= 상대 쪽에서 자동으로 승리 처리).
    bool quitDialogOpen = false;

    // ── Section C — Single vs Bot ───────────────────────────────────────────
    // BotSingle 모드에선 gameSingle 이 사람, gameBot 이 봇 보드. 둘 다 같은
    // seed 로 생성되지만 입력 스트림이 다르므로 자연스럽게 서로 다른 전개가 된다.
    // botInputQueue: BotOnnx::Infer → expand_placement 로 채운 틱 입력 마스크.
    //   매 틱 하나 pop, 비어 있으면 새 placement 계산을 시도.
    std::unique_ptr<Game> gameBot;
    bot::BotOnnx botOnnx;
    std::deque<uint8_t> botInputQueue;
    bool botAvailable = false;
    {
        std::string err;
        if (botOnnx.Load("model/policy.onnx", &err)) {
            botAvailable = true;
        } else {
            // 조용한 실패 — 메뉴에서 해당 항목이 회색으로 비활성화된다.
            fprintf(stderr, "[bot] policy.onnx not loaded: %s\n", err.c_str());
        }
    }
    int lastAttackHuman = 0, lastAttackBot = 0;

    static std::string cachedLocalIP;
    static std::string cachedPublicIP;
    static std::future<std::string> publicIpTask;
    static bool localIpDone = false;
    static bool publicIpLaunched = false;

    bool recording = false;
    ReplayData replay;
    replay.seed = sessionSeed;

    GameOverState gameOverState = GameOverState::None;
    net::GameOverChoice myGameOverChoice = net::GameOverChoice::None;
    float gameOverTimer = 0.0f;
    const float GAME_OVER_TIMEOUT      = 30.0f;
    const float DISAGREEMENT_COUNTDOWN = 3.0f;
    uint64_t lastReceivedSeed = 0;
    bool newSeedSent = false;

    // Section A — 링크 단절 grace. linkStatus() == Lost 를 처음 본 순간부터
    // 10초 카운트다운 후 타이틀로. 도중에 Stalled→OK 로 회복하면 리셋.
    bool linkLostActive = false;
    float linkLostCountdown = 0.0f;
    const float LINK_LOST_GRACE = 10.0f;

    // F.2 — 자동 HASH 검증. 매 600틱(~10s) 로컬 해시를 SendHash 하고 링으로
    // 기억. 상대의 HASH(tick, h) 가 들어오면 같은 틱의 로컬 해시와 비교 →
    // 불일치 시 DESYNC 오버레이 + stderr 로그.
    struct HashSnap { uint32_t tick = 0; uint64_t hash = 0; bool valid = false; };
    constexpr uint32_t HASH_PERIOD_TICKS = 600;
    constexpr size_t HASH_RING = 4;
    HashSnap localHashRing[HASH_RING]{};
    uint32_t lastHashSentTick = (uint32_t)-1;        // 중복 송신 방지
    uint32_t lastRemoteHashSeenTick = 0;
    uint32_t desyncTick = 0;
    bool desyncDetected = false;

    // Section I — 공격 라인 전달 상태. lockstep 을 통해 양쪽이 동일한 attack 값을
    // 산출하므로 네트워크 프레임은 필요 없다. 매 Tick 뒤 누적치의 델타만큼을
    // 반대편 pendingGarbage 에 주입한다. Game 객체 재생성 시 0 으로 리셋.
    int lastAttackLocal  = 0;
    int lastAttackRemote = 0;

    // Section I — 화면 흔들림. 보드별 독립 상태 (shake 대상: 가비지를 받는 쪽의
    // 보드만). 예: 내가 콤보로 상대에게 가비지를 보내면 → 상대 보드(오른쪽)만
    // 흔들림. 상대가 나에게 공격하면 → 내 보드(왼쪽)만 흔들림. 내 라인 클리어
    // 자체는 보드를 흔들지 않고 콜아웃 텍스트만 띄운다 (일반 테트리스 전투 관례).
    ShakeState shakeLeft{};
    ShakeState shakeRight{};

    // Section I — 콜아웃 텍스트 ("DOUBLE!" / "TRIPLE!" / "TETRIS!" / T-spin).
    struct Callout { const char* text = nullptr; float timeLeft = 0.0f; };
    Callout coLocal{};
    Callout coRemote{};
    auto trigger_callout = [](Callout& c, int lines) {
        const char* t = nullptr;
        switch (lines) {
            case 2: t = "DOUBLE!"; break;
            case 3: t = "TRIPLE!"; break;
            case 4: t = "TETRIS!"; break;
            default: break;
        }
        if (t) { c.text = t; c.timeLeft = 1.0f; }
    };
    auto trigger_tspin_callout = [](Callout& c, int lines) {
        const char* t = nullptr;
        switch (lines) {
            case 0: t = "T-SPIN!"; break;
            case 1: t = "T-SPIN SINGLE!"; break;
            case 2: t = "T-SPIN DOUBLE!"; break;
            case 3: t = "T-SPIN TRIPLE!"; break;
            default: break;
        }
        if (t) { c.text = t; c.timeLeft = 1.0f; }
    };

    // 보드별 이벤트 처리: callout + (가비지를 받을 때만) shake + 소비 플래그 리셋.
    //   shk 는 "이 보드 쪽" 의 shake 대상. 콜아웃은 이 보드 위에 뜬다.
    //   라인 클리어는 shake 를 트리거하지 않는다 — 공격(가비지) 가 반대편 보드로
    //   가면 그쪽 apply_fx 가 그 측 shake 를 걸어준다 (일반 테트리스 전투 관례).
    //   자기 / 상대 구분 없이 같은 로직 — 호출부가 올바른 shake 상태를 주입.
    auto apply_self_fx = [&](SimGame& sim, Callout& co) {
        if (sim.lastTSpinLines >= 0)
            trigger_tspin_callout(co, sim.lastTSpinLines);
        else if (sim.lastLinesCleared > 0)
            trigger_callout(co, sim.lastLinesCleared);
        if (sim.lastGarbageReceived > 0)
            shake_trigger(shakeLeft, 6.0f, 0.20f);
        if (sim.gameOverEvent)
            shake_trigger(shakeLeft, 16.0f, 0.50f);
        sim.lastLinesCleared = 0;
        sim.lastTSpinLines = -1;
        sim.lastGarbageReceived = 0;
        sim.gameOverEvent = false;
    };
    auto apply_peer_fx = [&](SimGame& sim, Callout& co) {
        if (sim.lastTSpinLines >= 0)
            trigger_tspin_callout(co, sim.lastTSpinLines);
        else if (sim.lastLinesCleared > 0)
            trigger_callout(co, sim.lastLinesCleared);
        if (sim.lastGarbageReceived > 0)
            shake_trigger(shakeRight, 6.0f, 0.20f);
        if (sim.gameOverEvent)
            shake_trigger(shakeRight, 16.0f, 0.50f);
        sim.lastLinesCleared = 0;
        sim.lastTSpinLines = -1;
        sim.lastGarbageReceived = 0;
        sim.gameOverEvent = false;
    };

    float accumulator = 0.0f;

    // ── 메인 루프 ───────────────────────────────────────────────────────────
    while (!platform_should_close())
    {
        // 1) 입력 처리 + 델타타임
        float deltaTime = platform_begin_frame();
        AccumulateInput(chatComposing);  // 엣지 트리거 입력을 매 프레임 누적

        // ── Section E: 채팅 (Net 모드 전용) ──────────────────────────────────
        //   - T : 입력 모드 진입 (게임 키 흡수됨)
        //   - Enter : 송신 + 히스토리 추가 + 닫기
        //   - Esc : 취소
        //   - Backspace : 한 글자 삭제
        //   - 그 외 printable : 버퍼에 누적 (WM_CHAR 경유)
        // 수신: session.PullChat 으로 상대 메시지 꺼내 히스토리에 추가.
        if (app == AppMode::Net) {
            // 수신 드레인 — 여러 줄이 쌓여도 한 프레임에 모두 흡수.
            std::string incoming;
            while (session.PullChat(incoming)) {
                chatHistory.push_back({"Opponent", std::move(incoming), 0.0f});
                while (chatHistory.size() > kChatHistoryMax) chatHistory.pop_front();
            }

            // 입력 모드 토글 — chatComposing 이 false 일 때만 T 로 진입.
            if (!chatComposing && platform_key_pressed(PKEY_T)) {
                chatComposing = true;
                chatInput.clear();
            } else if (chatComposing) {
                // printable 문자 드레인
                char ch = platform_get_char_pressed();
                while (ch != 0) {
                    if (ch >= 0x20 && ch <= 0x7E && chatInput.size() < kChatMaxChars)
                        chatInput.push_back(ch);
                    ch = platform_get_char_pressed();
                }
                if (platform_key_pressed(PKEY_BACK) && !chatInput.empty())
                    chatInput.pop_back();
                if (platform_key_pressed(PKEY_ENTER)) {
                    if (!chatInput.empty() && session.isConnected()) {
                        session.SendChat(chatInput);
                        chatHistory.push_back({"You", chatInput, 0.0f});
                        while (chatHistory.size() > kChatHistoryMax) chatHistory.pop_front();
                    }
                    chatInput.clear();
                    chatComposing = false;
                } else if (platform_key_pressed(PKEY_ESCAPE)) {
                    chatInput.clear();
                    chatComposing = false;
                }
            }

            // 히스토리 노화 — kChatFadeSeconds 경과분은 제거.
            for (auto& line : chatHistory) line.ageSec += deltaTime;
            while (!chatHistory.empty() && chatHistory.front().ageSec > kChatFadeSeconds)
                chatHistory.pop_front();
        } else {
            // Net 아닌 모드로 돌아가면 채팅 상태 리셋.
            chatComposing = false;
            chatInput.clear();
            chatHistory.clear();
        }

        // 2) 고정 틱 시뮬레이션 (60Hz)
        //   "나가기" 모달이 열려 있으면 Single/BotSingle 은 시간 진행 멈춤.
        //   Net 은 lockstep 동기 유지 필요 — 계속 진행 (모달은 오버레이 UI 일 뿐).
        const bool tickPauseForDialog = quitDialogOpen &&
            (app == AppMode::Single || app == AppMode::BotSingle);
        if (!tickPauseForDialog) accumulator += deltaTime;
        while (accumulator >= SECONDS_PER_TICK)
        {
            uint8_t inputMask = ConsumeInput(chatComposing);

            if (app == AppMode::Net && session.isConnected())
            {
                // 중요: INPUT 을 보낼 수 있는 조건은 엄격히.
                //   1) 게임 객체(gameLocal/gameRemote) 가 존재 — 매치메이킹 대기
                //      기간에는 sendQ 에 stale 프레임이 쌓여 ioThread 기동 시 상대
                //      remoteInputs 의 tick 0..N 을 stale 로 점유(= emplace 가 진짜
                //      입력을 덮어쓰지 않음) → 비대칭 DESYNC 를 유발.
                //   2) startDelay == 0 — 카운트다운(120틱) 동안에도 보내면 tick
                //      0..119 가 쌓여 시작 직후 fast-forward 구간 발생.
                //   3) gameOverState == None — 게임 오버 화면 / 재시작 협상 / 시드
                //      교환 대기 동안에도 보내면 "끝난 라운드의 INPUT" 이 새 라운드의
                //      같은 tick 번호 공간과 섞임. 프로토콜에 round-id 가 없으므로
                //      수신 측 emplace 가 stale 로 선점할 위험.
                if (gameLocal && gameRemote && startDelay == 0 &&
                    gameOverState == GameOverState::None)
                {
                    localInputs[localTickNext] = inputMask;
                    session.SendInput(localTickNext, inputMask);
                    localTickNext++;
                }

                if (session.isReady() && (!gameLocal || !gameRemote))
                {
                    sessionSeed = session.params().seed;
                    inputDelay  = session.params().input_delay;
                    gameLocal   = std::make_unique<Game>(sessionSeed);
                    gameRemote  = std::make_unique<Game>(sessionSeed);
                    localInputs.clear();
                    localTickNext = 0; simTick = 0;
                    startDelay = session.params().start_tick;
                    lastAttackLocal = 0; lastAttackRemote = 0;
                    // DESYNC 디버깅: 양쪽 창 로그를 비교해 초기 seed + 초기 hash 가
                    // 같은지 먼저 확인. 여기가 다르면 lockstep 출발점부터 갈림.
                    fprintf(stderr, "[INIT] seed=0x%016llx inputDelay=%u startDelay=%u\n",
                            (unsigned long long)sessionSeed,
                            (unsigned)inputDelay, (unsigned)startDelay);
                    fprintf(stderr, "[INIT] gameLocal  hash=0x%016llx\n",
                            (unsigned long long)gameLocal->ComputeStateHash());
                    fprintf(stderr, "[INIT] gameRemote hash=0x%016llx\n",
                            (unsigned long long)gameRemote->ComputeStateHash());
                    accumulator -= SECONDS_PER_TICK; continue;
                }

                if (session.isReady())
                {
                    if (startDelay > 0) { startDelay--; accumulator -= SECONDS_PER_TICK; continue; }

                    int64_t lastLocalSent = (localTickNext == 0) ? -1 : (int64_t)localTickNext - 1;
                    int64_t lastRemote    = (int64_t)session.maxRemoteTick();
                    int64_t safeTick      = std::min(lastLocalSent, lastRemote) - (int64_t)inputDelay;

                    if ((int64_t)simTick <= safeTick && gameLocal && gameRemote)
                    {
                        while ((int64_t)simTick <= safeTick)
                        {
                            uint8_t li = 0, ri = 0;
                            auto it = localInputs.find(simTick);
                            if (it != localInputs.end()) li = it->second;
                            if (!session.GetRemoteInput(simTick, ri)) break;
                            gameLocal->SubmitInput(li);
                            gameRemote->SubmitInput(ri);
                            gameLocal->Tick();
                            gameRemote->Tick();

                            // Section I: 양쪽 SimGame 이 lockstep 으로 동일한 attack 값을
                            // 산출하므로 로컬에서 델타를 계산해 반대편 pendingGarbage 로 전달.
                            // 주의: 이 코드는 "로컬 뷰에서 보이는 가비지 큐"를 다룬다 —
                            //   gameRemote (상대 화면 미러) 에는 local 의 공격이 들어가고
                            //   gameLocal  (내 화면)        에는 remote 의 공격이 들어간다.
                            {
                                int attL = gameLocal->sim.AttackLinesSent()  - lastAttackLocal;
                                int attR = gameRemote->sim.AttackLinesSent() - lastAttackRemote;
                                if (attL > 0) gameRemote->sim.AddPendingGarbage(attL);
                                if (attR > 0) gameLocal->sim.AddPendingGarbage(attR);
                                lastAttackLocal  = gameLocal->sim.AttackLinesSent();
                                lastAttackRemote = gameRemote->sim.AttackLinesSent();

                                apply_self_fx(gameLocal->sim,  coLocal);
                                apply_peer_fx(gameRemote->sim, coRemote);
                            }
                            simTick++;

                            // F.2: 600틱마다 양쪽 경기판 해시를 결합해 송신 + 링 기록.
                            // gameLocal 만 해싱하면 "호스트의 gameLocal" vs "게스트의
                            // gameLocal" 을 비교하게 되는데, 이 둘은 서로 다른 경기라
                            // 항상 다를 수밖에 없다(DESYNC 오탐). lockstep 이 정상이면
                            // 양쪽 모두 gameLocal+gameRemote 를 (같은 관점에서) 갖고
                            // 있으므로 XOR 로 결합하면 동일 해시가 나온다.
                            if (simTick > 0 && simTick % HASH_PERIOD_TICKS == 0 &&
                                simTick != lastHashSentTick) {
                                uint64_t hL = gameLocal->ComputeStateHash();
                                uint64_t hR = gameRemote->ComputeStateHash();
                                uint64_t h  = hL ^ hR;
                                session.SendHash(simTick, h);
                                auto& slot = localHashRing[(simTick / HASH_PERIOD_TICKS) % HASH_RING];
                                slot.tick = simTick; slot.hash = h; slot.valid = true;
                                lastHashSentTick = simTick;
                            }
                        }
                    }
                }
            }
            else if (app == AppMode::Single && gameSingle)
            {
                gameSingle->SubmitInput(inputMask);
                gameSingle->Tick();
                apply_self_fx(gameSingle->sim, coLocal);
            }
            // ── Section C — BotSingle 틱 ───────────────────────────────────────
            else if (app == AppMode::BotSingle && gameSingle && gameBot)
            {
                // 1) 봇 입력 큐가 비었으면 새 placement 계산.
                //    Infer 실패 또는 합법 수 없음 → INPUT_NONE 로 대기 (게임오버면 자연스럽게
                //    gameBot 가 멈춰 있음).
                if (botInputQueue.empty() && !gameBot->sim.IsGameOver()) {
                    int tgtCol = -1, tgtRot = -1;
                    bool ok = botOnnx.IsLoaded() && botOnnx.Infer(gameBot->sim, tgtCol, tgtRot);
                    if (!ok) ok = bot::fallback_placement(gameBot->sim, tgtCol, tgtRot);
                    if (ok) {
                        int curCol = gameBot->sim.CurrentCol();
                        int curRot = gameBot->sim.CurrentRotation();
                        auto seq = bot::expand_placement(curCol, curRot, tgtCol, tgtRot);
                        for (uint8_t m : seq) botInputQueue.push_back(m);
                    }
                }
                uint8_t botMask = INPUT_NONE;
                if (!botInputQueue.empty()) {
                    botMask = botInputQueue.front();
                    botInputQueue.pop_front();
                }

                gameSingle->SubmitInput(inputMask);
                gameBot->SubmitInput(botMask);
                gameSingle->Tick();
                gameBot->Tick();

                // Section I — 두 보드 간 가비지 교환 (Net 모드와 동일 구조).
                {
                    int attH = gameSingle->sim.AttackLinesSent() - lastAttackHuman;
                    int attB = gameBot->sim.AttackLinesSent()    - lastAttackBot;
                    if (attH > 0) gameBot->sim.AddPendingGarbage(attH);
                    if (attB > 0) gameSingle->sim.AddPendingGarbage(attB);
                    lastAttackHuman = gameSingle->sim.AttackLinesSent();
                    lastAttackBot   = gameBot->sim.AttackLinesSent();
                }

                apply_self_fx(gameSingle->sim, coLocal);
                apply_peer_fx(gameBot->sim,    coRemote);

                // 상대(봇) 피스가 락되면 큐를 비우고 다음 피스에서 다시 Infer.
                // expand_placement 는 마지막에 INPUT_DROP 을 넣으므로 시퀀스 끝에서
                // 자연스럽게 큐가 비워진다 — 별도 처리 불필요.
            }

            if (recording)
            {
                FrameInputs fr{}; fr.p1 = inputMask; fr.p2 = 0;
                replay.frames.push_back(fr);
            }
            accumulator -= SECONDS_PER_TICK;
        }

        // Section A — 링크 Lost 감지 + grace 카운트다운 (Net 모드 한정).
        // 게임 중이든 GameOverState 대기 중이든 10초 후 타이틀로 강제 복귀.
        if (app == AppMode::Net) {
            net::LinkStatus ls = session.linkStatus();
            if (ls == net::LinkStatus::Lost) {
                if (!linkLostActive) {
                    linkLostActive = true;
                    linkLostCountdown = LINK_LOST_GRACE;
                    std::cout << "[NET] Peer lost — returning to title in "
                              << LINK_LOST_GRACE << "s\n";
                } else {
                    linkLostCountdown -= deltaTime;
                    if (linkLostCountdown <= 0.0f) {
                        std::cout << "[NET] Grace elapsed — returning to menu\n";
                        session.Close();
                        gameLocal.reset(); gameRemote.reset();
                        gameOverState = GameOverState::None;
                        queueMode = false; netMode = false;
                        linkLostActive = false; linkLostCountdown = 0.0f;
                        app = AppMode::Menu;
                    }
                }
            } else if (linkLostActive) {
                // Stalled 는 유지하되 Lost 에서 회복되면 카운트다운 취소.
                linkLostActive = false;
                linkLostCountdown = 0.0f;
                std::cout << "[NET] Peer recovered — cancelling grace\n";
            }
        } else if (linkLostActive) {
            linkLostActive = false;
            linkLostCountdown = 0.0f;
        }

        // F.2 — 원격 HASH 수신 감지 + 링 비교. 같은 틱의 로컬 해시가 링에
        // 있어야 비교 가능 (링 크기 4 → 과거 40초 이력 커버).
        if (app == AppMode::Net && gameLocal) {
            uint32_t rt = 0; uint64_t rh = 0;
            if (session.GetLastRemoteHash(rt, rh) && rt != 0 && rt != lastRemoteHashSeenTick) {
                lastRemoteHashSeenTick = rt;
                auto& slot = localHashRing[(rt / HASH_PERIOD_TICKS) % HASH_RING];
                if (slot.valid && slot.tick == rt) {
                    if (slot.hash != rh) {
                        // DESYNC 시 어느 섹션이 달라졌는지 즉시 판별할 수 있도록
                        // 현재 시점의 gameLocal/gameRemote 섹션별 해시를 출력.
                        // (원격 hash 는 이미 XOR 결합이라 섹션 분리 불가 — 자기 쪽만 출력.
                        //  상대 쪽도 같은 시점에 DESYNC 를 찍으니 양쪽 콘솔을 대조하면
                        //  어느 필드가 먼저 달라졌는지 좁힐 수 있다.)
                        fprintf(stderr, "[DESYNC] tick=%u local=0x%016llx remote=0x%016llx\n",
                                rt, (unsigned long long)slot.hash, (unsigned long long)rh);
                        if (gameLocal && gameRemote) {
                            auto bL = gameLocal->sim.StateHashBreakdown();
                            auto bR = gameRemote->sim.StateHashBreakdown();
                            fprintf(stderr, "  gameLocal : grid=%016llx cur=%016llx nxt=%016llx rng=%016llx sf=%016llx co=%016llx\n",
                                    (unsigned long long)bL.grid, (unsigned long long)bL.currentBlock,
                                    (unsigned long long)bL.nextBlock, (unsigned long long)bL.rng,
                                    (unsigned long long)bL.scoreFlags, (unsigned long long)bL.combat);
                            fprintf(stderr, "  gameRemote: grid=%016llx cur=%016llx nxt=%016llx rng=%016llx sf=%016llx co=%016llx\n",
                                    (unsigned long long)bR.grid, (unsigned long long)bR.currentBlock,
                                    (unsigned long long)bR.nextBlock, (unsigned long long)bR.rng,
                                    (unsigned long long)bR.scoreFlags, (unsigned long long)bR.combat);
                        }
                        desyncDetected = true;
                        desyncTick = rt;
                    }
                }
                // 같은 틱이 링에 없을 수도 있음(시작 직후 등) — 이 경우 무시.
            }
        }

        // 3) 렌더링
        // Section I: shake 업데이트 (2개 독립 상태). 뷰 오프셋은 보드별 드로우
        // 직전에 개별 적용하고, UI/오버레이 그릴 땐 0 으로 리셋.
        shake_update(shakeLeft,  deltaTime);
        shake_update(shakeRight, deltaTime);
        if (coLocal.timeLeft  > 0.0f) coLocal.timeLeft  -= deltaTime;
        if (coRemote.timeLeft > 0.0f) coRemote.timeLeft -= deltaTime;
        renderer_set_view_offset(0, 0);

        renderer_begin({8, 10, 20, 255});

        // ── 메뉴 ────────────────────────────────────────────────────────────
        //   키보드와 마우스 모두 지원. 키보드 위/아래 로 menuIndex 이동, Enter 선택.
        //   마우스 클릭 = 해당 항목 즉시 활성화 (menuIndex 와 무관).
        if (app == AppMode::Menu)
        {
            // 메뉴 배경 — 중앙에 미세한 그라데이션 느낌을 위해 두 레이어 오버레이.
            draw_rect(200, 60, 320, 5, {55, 62, 110, 180});   // 타이틀 위 강조선
            {
                int tw = measure_text("TETRIS", 64);
                draw_text("TETRIS", (720 - tw) / 2, 80, 64, WHITE);
            }
            draw_rect(200, 152, 320, 2, {45, 52, 90, 140});   // 타이틀 아래 구분선

            constexpr Color DISABLED = {70, 70, 70, 255};
            const char* items[] = {
                "Single Play",
                "Single vs Bot",
                "Matchmaking Multi",
                "Custom Room Multi",
                "Quit",
            };
            constexpr int kMenuCount = 5;

            // 버튼 레이아웃 — 중앙 정렬. 너비 300, 높이 46, 간격 10.
            const int bw = 300;
            const int bh = 46;
            const int bx = (720 - bw) / 2;
            const int byStart = 210;

            // 키보드 네비게이션 (기존 동작 유지).
            if (platform_key_pressed(PKEY_DOWN)) menuIndex = (menuIndex + 1) % kMenuCount;
            if (platform_key_pressed(PKEY_UP))   menuIndex = (menuIndex + kMenuCount - 1) % kMenuCount;

            int activated = -1;  // 이번 프레임 활성화된 항목(-1 = 없음)

            for (int i = 0; i < kMenuCount; ++i)
            {
                const int by = byStart + i * (bh + 10);
                const bool disabled = (i == 1 && !botAvailable);
                // Disabled 항목은 버튼 그리되 클릭 반환 무시(+ 라벨 회색).
                if (disabled) {
                    draw_rect_rounded(bx, by, bw, bh, 0.25f, {25, 30, 45, 255});
                    const int tw = measure_text(items[i], 24);
                    draw_text(items[i], bx + (bw - tw) / 2, by + (bh - 24) / 2,
                              24, DISABLED);
                    continue;
                }
                bool clicked = gui_button_highlighted(bx, by, bw, bh, items[i],
                                                      (i == menuIndex), 24);
                if (clicked) activated = i;
            }

            // 키보드 Enter/Space 로도 현재 강조된 항목 활성화.
            if (platform_key_pressed(PKEY_ENTER) || platform_key_pressed(PKEY_SPACE)) {
                if (!(menuIndex == 1 && !botAvailable)) activated = menuIndex;
            }

            if (!botAvailable) {
                // Bot 모드가 disabled 임을 안내.
                draw_text("(model/policy.onnx not found)", 250,
                          byStart + 1 * (bh + 10) + bh + 2, 12, DISABLED);
            }
            draw_text("(direct Host/Connect: use --host / --connect CLI)",
                      180, 570, 12, DISABLED);

            if (activated >= 0) {
                if (activated == 0) {
                    app = AppMode::Single;
                    gameSingle = std::make_unique<Game>(sessionSeed);
                } else if (activated == 1) {
                    app = AppMode::BotSingle;
                    gameSingle = std::make_unique<Game>(sessionSeed);
                    gameBot    = std::make_unique<Game>(sessionSeed);
                    botInputQueue.clear();
                    lastAttackHuman = 0; lastAttackBot = 0;
                } else if (activated == 2) {
                    queueHost = relayHost; queuePort = relayPort;
                    if (session.QueueJoin(queueHost, queuePort, startDelay, inputDelay)) {
                        netMode = true; queueMode = true; isHost = false;
                        app = AppMode::Net;
                    }
                } else if (activated == 3) {
                    roomRelayHost = relayHost; roomRelayPort = relayPort;
                    app = AppMode::RoomLobby;
                    roomStage = RoomLobbyStage::Choose;
                    roomCodeInput.clear();
                    roomErrorMsg.clear();
                } else {
                    platform_shutdown(); return 0;
                }
            }
        }

        // ── IP 입력 화면 ─────────────────────────────────────────────────────
        if (app == AppMode::ConnectInput)
        {
            draw_text("Enter IP:port", 180, 200, 40, WHITE);
            draw_rect(180, 250, 360, 44, lightBlue);
            draw_text(connectText.c_str(), 190, 256, 32, WHITE);
            if (connectError)
                draw_text(connectErrorMsg.c_str(), 180, 310, 22, RED);
            draw_text("[Q] Back to Menu", 220, 420, 20, GRAY);

            char ch = platform_get_char_pressed();
            while (ch)
            {
                if ((ch >= '0' && ch <= '9') || ch == '.' || ch == ':')
                    connectText.push_back(ch);
                ch = platform_get_char_pressed();
            }
            if (platform_key_pressed(PKEY_BACK) && !connectText.empty())
                connectText.pop_back();
            if (platform_key_pressed(PKEY_ENTER) || platform_key_pressed(PKEY_SPACE))
            {
                auto pos = connectText.find(':');
                if (pos != std::string::npos && pos > 0 && pos < connectText.length() - 1)
                {
                    hostIp = connectText.substr(0, pos);
                    std::string portStr = connectText.substr(pos + 1);
                    connectError = false;
                    try {
                        hostPort = (uint16_t)std::stoi(portStr);
                        if (hostPort == 0) throw std::exception();
                    } catch (...) {
                        connectError = true; connectErrorMsg = "Invalid port number!";
                    }
                    if (!connectError && (hostIp.empty() ||
                        hostIp.find_first_not_of("0123456789.") != std::string::npos))
                    {
                        connectError = true; connectErrorMsg = "Invalid IP address format!";
                    }
                    if (!connectError)
                    {
                        netMode = true; isHost = false; app = AppMode::Net;
                        std::cout << "[NET] Connecting to " << hostIp << ":" << hostPort << "\n";
                        if (!session.Connect(hostIp, hostPort)) {
                            connectError = true;
                            connectErrorMsg = "Connection failed! Check IP/port/firewall";
                            app = AppMode::ConnectInput;
                        }
                    }
                } else {
                    connectError = true;
                    connectErrorMsg = "Format: IP:PORT (e.g. 192.168.1.100:7777)";
                }
            }
            if (platform_key_pressed(PKEY_Q)) app = AppMode::Menu;
        }

        // (이전에 있던 "Matchmaking Relay IP 입력" / "Custom Room Relay IP 입력"
        //  화면은 제거되었다. 릴레이 주소는 CLI 플래그 `--relay host[:port]` 로
        //  지정하거나 기본값 127.0.0.1:7777 을 사용. 메뉴에서 Matchmaking Multi /
        //  Custom Room Multi 를 고르면 즉시 QueueJoin 또는 RoomLobby 로 진입.)

        // ── [Custom Room] 로비 — Create or Join ──────────────────────────────
        if (app == AppMode::RoomLobby)
        {
            draw_text("Custom Room", 180, 100, 40, WHITE);
            char relayLine[128];
            snprintf(relayLine, sizeof(relayLine), "Relay: %s:%u",
                     roomRelayHost.c_str(), (unsigned)roomRelayPort);
            draw_text(relayLine, 180, 150, 18, GRAY);

            if (roomStage == RoomLobbyStage::Choose) {
                draw_text("[C] Create Room",    180, 230, 30, WHITE);
                draw_text("[J] Join by Code",   180, 280, 30, WHITE);
                draw_text("[Q] Back",           180, 340, 22, GRAY);
                if (!roomErrorMsg.empty())
                    draw_text(roomErrorMsg.c_str(), 180, 390, 18, RED);

                if (platform_key_pressed(PKEY_C)) {
                    if (session.RoomCreate(roomRelayHost, roomRelayPort, startDelay, inputDelay)) {
                        app = AppMode::RoomWaiting;
                        roomLocalReady = false;
                    } else {
                        roomErrorMsg = "Session busy — try again";
                    }
                } else if (platform_key_pressed(PKEY_J)) {
                    roomStage = RoomLobbyStage::EnterCode;
                    roomCodeInput.clear();
                    roomErrorMsg.clear();
                } else if (platform_key_pressed(PKEY_Q)) {
                    app = AppMode::Menu;
                }
            } else {  // EnterCode
                draw_text("Enter code (5 chars):", 180, 220, 24, WHITE);
                draw_rect(180, 260, 260, 44, lightBlue);
                draw_text(roomCodeInput.c_str(), 190, 266, 32, WHITE);
                draw_text("[Enter] Join  [Q] Back", 180, 330, 20, GRAY);
                if (!roomErrorMsg.empty())
                    draw_text(roomErrorMsg.c_str(), 180, 380, 18, RED);

                char ch = platform_get_char_pressed();
                while (ch) {
                    // 대문자 영숫자만 허용, 최대 5자 (서버 알파벳과 맞춤).
                    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
                    bool alnum = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
                    if (alnum && roomCodeInput.size() < 5)
                        roomCodeInput.push_back(ch);
                    ch = platform_get_char_pressed();
                }
                if (platform_key_pressed(PKEY_BACK) && !roomCodeInput.empty())
                    roomCodeInput.pop_back();
                if (platform_key_pressed(PKEY_Q)) {
                    roomStage = RoomLobbyStage::Choose;
                    roomErrorMsg.clear();
                }
                if (platform_key_pressed(PKEY_ENTER) || platform_key_pressed(PKEY_SPACE)) {
                    if (roomCodeInput.size() != 5) {
                        roomErrorMsg = "Code must be exactly 5 characters";
                    } else if (session.RoomJoin(roomRelayHost, roomRelayPort,
                                                roomCodeInput, startDelay, inputDelay)) {
                        app = AppMode::RoomWaiting;
                        roomLocalReady = false;
                    } else {
                        roomErrorMsg = "Session busy — try again";
                    }
                }
            }
        }

        // ── [Custom Room] 대기 — Ready / MATCH_FOUND ───────────────────────
        if (app == AppMode::RoomWaiting)
        {
            net::RoomState rs = session.roomState();
            std::string code = session.roomCode();
            int peers = session.roomPeerCount();

            draw_text("Custom Room", 180, 80, 36, WHITE);

            char codeLine[128];
            snprintf(codeLine, sizeof(codeLine), "Code: %s", code.empty() ? "-----" : code.c_str());
            draw_text(codeLine, 180, 160, 36, WHITE);

            const char* stateLabel = "…";
            Color stateColor = GRAY;
            switch (rs) {
                case net::RoomState::Connecting:      stateLabel = "Connecting…";        break;
                case net::RoomState::Waiting:         stateLabel = "Waiting for peer…";  break;
                case net::RoomState::WaitingWithPeer: stateLabel = "Peer joined. Press R when ready."; stateColor = WHITE; break;
                case net::RoomState::NotFound:        stateLabel = "Room not found"; stateColor = RED; break;
                case net::RoomState::Full:            stateLabel = "Room is full"; stateColor = RED; break;
                case net::RoomState::GoneFull:        stateLabel = "Peer left. Waiting…"; stateColor = YELLOW; break;
                case net::RoomState::Failed:          stateLabel = "Connection failed"; stateColor = RED; break;
                case net::RoomState::Starting:        stateLabel = "Starting!"; stateColor = GREEN; break;
                default: break;
            }
            draw_text(stateLabel, 180, 220, 22, stateColor);

            char peerLine[64];
            snprintf(peerLine, sizeof(peerLine), "Peers: %d / 2", peers);
            draw_text(peerLine, 180, 260, 20, GRAY);

            draw_text(roomLocalReady ? "You: READY" : "You: Not Ready",
                      180, 300, 24, roomLocalReady ? GREEN : WHITE);

            // 양쪽 슬롯 아이콘 — 로컬은 항상 있음, 상대는 peers==2 일 때만.
            if (iconYou)      draw_image(iconYou,      140, 340, 64, 64);
            if (iconOpponent && peers >= 2) draw_image(iconOpponent, 516, 340, 64, 64);
            draw_text("You", 150, 410, 18, WHITE);
            if (peers >= 2) draw_text("Peer", 520, 410, 18, WHITE);

            draw_text("[R] Toggle Ready   [Esc] Leave", 180, 450, 20, GRAY);

            if (rs == net::RoomState::WaitingWithPeer ||
                rs == net::RoomState::Waiting) {
                if (platform_key_pressed(PKEY_R)) {
                    roomLocalReady = !roomLocalReady;
                    session.RoomSendReady(roomLocalReady);
                }
            }
            if (platform_key_pressed(PKEY_ESCAPE) || platform_key_pressed(PKEY_Q)) {
                session.RoomLeave();
                session.Close();
                roomLocalReady = false;
                app = AppMode::Menu;
            }
            if (rs == net::RoomState::Failed ||
                rs == net::RoomState::NotFound ||
                rs == net::RoomState::Full) {
                // 잠시 보여준 뒤 Esc 눌러 나가도록. 자동 복귀는 없음.
            }

            // MATCH_FOUND 수신됨 → 게임 세션으로 전환.
            if (rs == net::RoomState::Starting && session.isReady()) {
                app = AppMode::Net;
                netMode = true; isHost = false;
                roomLocalReady = false;
            }
        }

        // ── 1인 모드 ─────────────────────────────────────────────────────────
        if (app == AppMode::Single && gameSingle)
        {
            // 우측 정보 패널 (보드 오른쪽 x=316 ~ 720)
            constexpr int pX = 320, pW = 175;
            constexpr Color panelBg   = {18, 22, 42, 255};
            constexpr Color panelLine = {45, 52, 85, 255};
            constexpr Color labelClr  = {120, 130, 170, 255};

            // SCORE
            draw_rect_rounded(pX, 12, pW, 80, 0.3f, panelBg);
            draw_text("SCORE", pX + 12, 18, 16, labelClr);
            {
                char buf[16]; snprintf(buf, sizeof(buf), "%d", gameSingle->score);
                int tw = measure_text(buf, 34);
                draw_text(buf, pX + (pW - tw) / 2, 40, 34, WHITE);
            }

            // LEVEL + LINES 나란히
            draw_rect_rounded(pX, 104, pW, 70, 0.3f, panelBg);
            draw_rect(pX + 12, 130, pW - 24, 1, panelLine);
            draw_text("LEVEL", pX + 12, 110, 14, labelClr);
            draw_text("LINES", pX + pW/2 + 4, 110, 14, labelClr);
            draw_text(fmt_buf("%d",  gameSingle->sim.level),
                      pX + 22, 126, 28, WHITE);
            draw_text(fmt_buf("%d",  gameSingle->sim.totalLinesCleared),
                      pX + pW/2 + 14, 126, 28, WHITE);

            // NEXT
            draw_rect_rounded(pX, 186, pW, 130, 0.3f, panelBg);
            draw_text("NEXT", pX + 12, 192, 14, labelClr);
            // 3칸 기준 60px → (175-60)/2 = 57 ≈ 55 offset 으로 대략 중앙정렬
            gameSingle->DrawNextMini(pX + 50, 215, 20);

            // 보드 shake
            {
                float sdx = 0.f, sdy = 0.f;
                shake_offset(shakeLeft, sdx, sdy);
                renderer_set_view_offset((int)sdx, (int)sdy);
                gameSingle->DrawBoardAt(11, 11);
            }
            renderer_set_view_offset(0, 0);

            // 콜아웃
            if (coLocal.text && coLocal.timeLeft > 0.0f) {
                int tw = measure_text(coLocal.text, 48);
                draw_text(coLocal.text, 155 - tw / 2, 290, 48, YELLOW);
            }
        }

        // F5/F6 리플레이
        if (platform_key_pressed(PKEY_F5)) { recording = true; replay.frames.clear(); }
        if (platform_key_pressed(PKEY_F6) && recording)
        {
            std::error_code ec;
            std::filesystem::create_directories("out", ec);
            ReplayIO::Save("out/replay.txt", replay);
            recording = false;
        }

        // H 키: 해시 출력
        if (platform_key_pressed(PKEY_H))
        {
            unsigned long long h1 = gameSingle ? gameSingle->ComputeStateHash() : 0;
            unsigned long long hL = gameLocal  ? gameLocal->ComputeStateHash()  : 0;
            unsigned long long hR = gameRemote ? gameRemote->ComputeStateHash() : 0;
            std::cout << "Hash single=0x" << std::hex << h1
                      << " local=0x" << hL << " remote=0x" << hR << std::dec << "\n";
        }

        // ── Section C — BotSingle 렌더 + 게임오버 ─────────────────────────
        if (app == AppMode::BotSingle && gameSingle && gameBot)
        {
            int leftX = 11, rightX = 11 + 300 + 60;
            if (iconYou) draw_image(iconYou, leftX,  6, 32, 32);
            if (iconBot) draw_image(iconBot, rightX, 6, 32, 32);
            draw_text("You", leftX  + 38, 8, 22, WHITE);
            draw_text("Bot", rightX + 38, 8, 22, WHITE);
            // 보드별 shake (Net 과 같은 구조).
            {
                float sdx = 0.f, sdy = 0.f;
                shake_offset(shakeLeft, sdx, sdy);
                renderer_set_view_offset((int)sdx, (int)sdy);
                gameSingle->DrawBoardAt(leftX, 11);
                Game::DrawGarbageBar(leftX, 11, gameSingle->sim.PendingGarbage());
            }
            {
                float sdx = 0.f, sdy = 0.f;
                shake_offset(shakeRight, sdx, sdy);
                renderer_set_view_offset((int)sdx, (int)sdy);
                gameBot->DrawBoardAt(rightX, 11);
                Game::DrawGarbageBar(rightX, 11, gameBot->sim.PendingGarbage());
            }
            renderer_set_view_offset(0, 0);

            // Next 프리뷰 — Multi 와 동일 레이아웃.
            {
                int midX = leftX + 300 + 5;
                draw_text("Next", midX,  20, 16, GRAY);
                gameSingle->DrawNextMini(midX, 44, 14);
                draw_text("Bot",  midX, 160, 16, GRAY);
                gameBot->DrawNextMini(midX, 184, 14);
            }

            // 스코어/레벨 하단 패널
            {
                constexpr Color sb = {18, 22, 40, 200};
                draw_rect_rounded(leftX,  614, 120, 22, 0.4f, sb);
                draw_rect_rounded(rightX, 614, 120, 22, 0.4f, sb);
                draw_text(fmt_buf("Score: %d", gameSingle->score), leftX  + 6, 616, 16, WHITE);
                draw_text(fmt_buf("Score: %d", gameBot->score),    rightX + 6, 616, 16, WHITE);
                draw_text(fmt_buf("Lv.%d", gameSingle->sim.level), leftX  + 6, 633, 14, {120,130,170,255});
                draw_text(fmt_buf("Lv.%d", gameBot->sim.level),    rightX + 6, 633, 14, {120,130,170,255});
            }

            if (coLocal.text && coLocal.timeLeft > 0.0f) {
                int tw = measure_text(coLocal.text, 40);
                draw_text(coLocal.text, leftX + (300 - tw) / 2, 280, 40, YELLOW);
            }
            if (coRemote.text && coRemote.timeLeft > 0.0f) {
                int tw = measure_text(coRemote.text, 40);
                draw_text(coRemote.text, rightX + (300 - tw) / 2, 280, 40, YELLOW);
            }

            // 한 쪽이 끝나면 승패 표시 + [R]/[Q].
            if (gameSingle->gameOver || gameBot->gameOver)
            {
                const char* label;
                Color labelC;
                if (gameSingle->gameOver && !gameBot->gameOver) {
                    label = "LOSE"; labelC = RED;
                } else if (gameBot->gameOver && !gameSingle->gameOver) {
                    label = "WIN";  labelC = GREEN;
                } else {
                    label = "DRAW"; labelC = YELLOW;
                }
                int tw = measure_text(label, 60);
                draw_text(label, 360 - tw / 2, 280, 60, labelC);
                draw_text("[R] Restart",     280, 350, 28, GREEN);
                draw_text("[Q] Go to Title", 260, 385, 28, YELLOW);
                if (platform_key_pressed(PKEY_R)) {
                    gameSingle = std::make_unique<Game>(sessionSeed);
                    gameBot    = std::make_unique<Game>(sessionSeed);
                    botInputQueue.clear();
                    lastAttackHuman = 0; lastAttackBot = 0;
                } else if (platform_key_pressed(PKEY_Q)) {
                    gameSingle.reset();
                    gameBot.reset();
                    botInputQueue.clear();
                    app = AppMode::Menu;
                }
            }
        }

        // 1인 게임 오버
        if (app == AppMode::Single && gameSingle && gameSingle->gameOver)
        {
            draw_text("GAME OVER",    120, 300, 60, WHITE);
            draw_text("[R] Restart",  200, 350, 28, GREEN);
            draw_text("[Q] Go to Title", 180, 385, 28, YELLOW);
            if (platform_key_pressed(PKEY_R))
            {
                gameSingle = std::make_unique<Game>(sessionSeed);
                if (recording) replay.frames.clear();
            }
            else if (platform_key_pressed(PKEY_Q))
            {
                gameSingle.reset(); app = AppMode::Menu;
            }
        }

        // ── 멀티플레이 ────────────────────────────────────────────────────────
        if (app == AppMode::Net && gameLocal && gameRemote)
        {
            int leftX = 11, rightX = 11 + 300 + 60;
            // 아이콘 (32x32) 을 라벨 왼쪽에 배치. 파일 없으면 no-op.
            if (iconYou)      draw_image(iconYou,      leftX,  6, 32, 32);
            if (iconOpponent) draw_image(iconOpponent, rightX, 6, 32, 32);
            draw_text("You",      leftX  + 38,  8, 22, WHITE);
            draw_text("Opponent", rightX + 38,  8, 22, WHITE);
            // 보드별 shake — 각 보드 드로우 직전에 그 측 offset 을 적용.
            //   왼쪽(내 보드): shakeLeft. 오른쪽(상대 미러): shakeRight.
            //   apply_self_fx 가 shakeLeft 를, apply_peer_fx 가 shakeRight 를 트리거.
            {
                float sdx = 0.f, sdy = 0.f;
                shake_offset(shakeLeft, sdx, sdy);
                renderer_set_view_offset((int)sdx, (int)sdy);
                gameLocal->DrawBoardAt(leftX, 11);
                Game::DrawGarbageBar(leftX, 11, gameLocal->sim.PendingGarbage());
            }
            {
                float sdx = 0.f, sdy = 0.f;
                shake_offset(shakeRight, sdx, sdy);
                renderer_set_view_offset((int)sdx, (int)sdy);
                gameRemote->DrawBoardAt(rightX, 11);
                Game::DrawGarbageBar(rightX, 11, gameRemote->sim.PendingGarbage());
            }
            renderer_set_view_offset(0, 0);  // UI/오버레이는 정적

            // Next 프리뷰 — 두 보드 사이 60px 갭을 활용 (cellSize=14).
            //   Local 용: 상단, Remote 용: 그 아래. 라벨 포함.
            {
                int midX = leftX + 300 + 5;               // 보드 사이 갭 시작 (316)
                draw_text("Next",  midX, 20, 16, GRAY);
                gameLocal->DrawNextMini(midX, 44, 14);    // ~56x56 영역
                draw_text("Opp",   midX, 160, 16, GRAY);
                gameRemote->DrawNextMini(midX, 184, 14);
            }

            // 스코어/레벨 하단 패널
            {
                constexpr Color sb = {18, 22, 40, 200};
                draw_rect_rounded(leftX,  614, 120, 22, 0.4f, sb);
                draw_rect_rounded(rightX, 614, 120, 22, 0.4f, sb);
                draw_text(fmt_buf("Score: %d", gameLocal->score),  leftX  + 6, 616, 16, WHITE);
                draw_text(fmt_buf("Score: %d", gameRemote->score), rightX + 6, 616, 16, WHITE);
                draw_text(fmt_buf("Lv.%d", gameLocal->sim.level),  leftX  + 6, 633, 14, {120,130,170,255});
                draw_text(fmt_buf("Lv.%d", gameRemote->sim.level), rightX + 6, 633, 14, {120,130,170,255});
            }

            // Section I — 양측 보드 중앙에 콜아웃. 보드 폭 300px 가정.
            if (coLocal.text && coLocal.timeLeft > 0.0f) {
                int tw = measure_text(coLocal.text, 40);
                draw_text(coLocal.text, leftX + (300 - tw) / 2, 280, 40, YELLOW);
            }
            if (coRemote.text && coRemote.timeLeft > 0.0f) {
                int tw = measure_text(coRemote.text, 40);
                draw_text(coRemote.text, rightX + (300 - tw) / 2, 280, 40, YELLOW);
            }

            if ((gameLocal->gameOver || gameRemote->gameOver) &&
                gameOverState == GameOverState::None)
            {
                gameOverState = GameOverState::ShowingGameOver;
                myGameOverChoice = net::GameOverChoice::None;
                gameOverTimer = 0.0f;
            }

            if (gameOverState == GameOverState::ShowingGameOver)
            {
                draw_text("GAME OVER",                  220, 280, 60, WHITE);
                draw_text("[R] Restart",                240, 350, 28, GREEN);
                draw_text("[Q] Go to Title (immediate)",170, 385, 28, YELLOW);
                if (platform_key_pressed(PKEY_R)) {
                    myGameOverChoice = net::GameOverChoice::Restart;
                    session.SendGameOverChoice(myGameOverChoice);
                    gameOverState = GameOverState::WaitingForRemote;
                    gameOverTimer = 0.0f;
                } else if (platform_key_pressed(PKEY_Q)) {
                    gameOverState = GameOverState::GoingToTitle;
                }
            }
            else if (gameOverState == GameOverState::WaitingForRemote)
            {
                draw_text("GAME OVER",                220, 280, 60, WHITE);
                draw_text("Waiting for opponent...", 180, 450, 24, GRAY);
                gameOverTimer += deltaTime;
                int remaining = std::max(0, (int)(GAME_OVER_TIMEOUT - gameOverTimer));
                draw_text(fmt_buf("Timeout in %ds", remaining), 250, 480, 20, GRAY);

                net::GameOverChoice remoteChoice;
                if (session.GetRemoteGameOverChoice(remoteChoice))
                {
                    if (myGameOverChoice == remoteChoice)
                    {
                        if (myGameOverChoice == net::GameOverChoice::Restart)
                        {
                            if (session.params().role == net::Role::Host)
                                gameOverState = GameOverState::SendingNewSeed;
                            else {
                                lastReceivedSeed = sessionSeed;
                                gameOverState = GameOverState::WaitingForNewSeed;
                            }
                            gameOverTimer = 0.0f;
                        } else {
                            gameOverState = GameOverState::GoingToTitle;
                        }
                    } else {
                        gameOverState = GameOverState::ShowingDisagreement;
                        gameOverTimer = 0.0f;
                    }
                }
                if (gameOverTimer >= GAME_OVER_TIMEOUT)
                    gameOverState = GameOverState::GoingToTitle;
            }
            else if (gameOverState == GameOverState::ShowingDisagreement)
            {
                draw_text("CHOICES DIFFER",        220, 300, 40, RED);
                draw_text("Returning to title...", 180, 360, 28, YELLOW);
                gameOverTimer += deltaTime;
                int countdown = std::max(0, (int)(DISAGREEMENT_COUNTDOWN - gameOverTimer) + 1);
                draw_text(fmt_buf("%d", countdown), 350, 410, 60, WHITE);
                if (gameOverTimer >= DISAGREEMENT_COUNTDOWN)
                    gameOverState = GameOverState::GoingToTitle;
            }
            else if (gameOverState == GameOverState::SendingNewSeed)
            {
                draw_text("GAME OVER",          220, 280, 60, WHITE);
                draw_text("Sending new seed...",180, 450, 24, GRAY);
                if (!newSeedSent) {
                    sessionSeed = (uint64_t)(platform_get_time() * 1000000.0) + rand();
                    session.SendNewSeed(sessionSeed);
                    newSeedSent = true;
                }
                gameOverTimer += deltaTime;
                if (gameOverTimer >= 1.5f) { newSeedSent = false; gameOverState = GameOverState::RestartingGame; }
            }
            else if (gameOverState == GameOverState::WaitingForNewSeed)
            {
                draw_text("GAME OVER",             220, 280, 60, WHITE);
                draw_text("Waiting for new seed...",180, 450, 24, GRAY);
                uint64_t newSeed = session.params().seed;
                if (newSeed != lastReceivedSeed) {
                    lastReceivedSeed = newSeed;
                    gameOverState = GameOverState::RestartingGame;
                }
                gameOverTimer += deltaTime;
                if (gameOverTimer >= 10.0f) gameOverState = GameOverState::GoingToTitle;
            }
            else if (gameOverState == GameOverState::RestartingGame)
            {
                sessionSeed = session.params().seed;
                session.ClearInputs();
                localInputs.clear(); localTickNext = 0; simTick = 0;
                startDelay = session.params().start_tick;
                gameLocal  = std::make_unique<Game>(sessionSeed);
                gameRemote = std::make_unique<Game>(sessionSeed);
                lastAttackLocal = 0; lastAttackRemote = 0;
                session.ClearGameOverChoices();
                // HASH 관련 상태 초기화 — 이전 라운드의 slot 이 남아있으면
                // 새 라운드 tick 600 snapshot 을 이전 라운드 스냅샷과 비교하거나,
                // lastHashSentTick 때문에 새 라운드의 첫 HASH 송신이 스킵될 수 있음.
                // 결과: 실제 게임은 정상인데 DESYNC 배너만 뜨는 오탐.
                for (auto& slot : localHashRing) slot = HashSnap{};
                lastHashSentTick       = (uint32_t)-1;
                lastRemoteHashSeenTick = 0;
                desyncDetected         = false;
                desyncTick             = 0;
                if (recording) { replay.frames.clear(); replay.seed = sessionSeed; }
                gameOverState = GameOverState::None;
            }
            else if (gameOverState == GameOverState::GoingToTitle)
            {
                gameLocal.reset(); gameRemote.reset();
                localInputs.clear(); localTickNext = 0; simTick = 0;
                session.Close();
                localIpDone = false; publicIpLaunched = false;
                cachedLocalIP.clear(); cachedPublicIP.clear();
                session.ClearGameOverChoices();
                gameOverState = GameOverState::None;
                app = AppMode::Menu;
            }

            if (!session.isConnected())
            {
                if (session.isListening())
                    draw_text(fmt_buf("Hosting on port %u...", (unsigned)hostPort), 180, 530, 22, WHITE);
                draw_text("Waiting for connection...", 180, 560, 22, WHITE);
            }
            else if (!session.isReady())
            {
                draw_text("Waiting for session ready...", 170, 560, 22, WHITE);
            }
        }

        // ── 멀티 대기 화면 ─────────────────────────────────────────────────────
        if (app == AppMode::Net && (!gameLocal || !gameRemote))
        {
            constexpr Color cardBg  = {18, 22, 42, 255};
            constexpr Color accentL = {55, 62, 100, 255};
            // 중앙 카드 패널
            draw_rect_rounded(160, 140, 400, 340, 0.15f, cardBg);
            draw_rect(160, 140, 400, 3, accentL);  // 상단 강조선

            if (queueMode)
            {
                if (session.hasFailed()) {
                    gui_text_center(360, 190, "Matchmaking Failed", 28, RED);
                    gui_text_center(360, 232, "relay unreachable or timeout", 16, GRAY);
                    gui_text_center(360, 400, "[Q] Back to Menu", 20, YELLOW);
                } else if (!session.isConnected()) {
                    gui_text_center(360, 185, "Connecting to Relay...", 24, WHITE);
                    draw_text(fmt_buf("%s:%u", queueHost.c_str(), (unsigned)queuePort),
                              220, 218, 16, {120,130,170,255});
                    gui_text_center(360, 400, "[Q] Cancel", 18, GRAY);
                } else {
                    gui_text_center(360, 185, "Searching for Opponent", 26, WHITE);
                    gui_text_center(360, 224, "Connected — waiting for match...", 15, {120,130,170,255});
                    gui_text_center(360, 400, "[Q] Cancel", 18, GRAY);
                }
            }
            else if (isHost)
            {
                if (!localIpDone) {
                    cachedLocalIP = net::get_local_ip(); localIpDone = true;
                }
                if (!publicIpLaunched) {
                    publicIpTask = std::async(std::launch::async, net::get_public_ip);
                    publicIpLaunched = true;
                }
                if (publicIpTask.valid() &&
                    publicIpTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    cachedPublicIP = publicIpTask.get();

                gui_text_center(360, 165, fmt_buf("Hosting on port %u", (unsigned)hostPort), 24, WHITE);
                draw_rect(200, 200, 320, 1, accentL);
                draw_text("Same WiFi", 200, 214, 14, {120,130,170,255});
                draw_text(fmt_buf("%s:%u", cachedLocalIP.c_str(), (unsigned)hostPort), 200, 232, 18, YELLOW);
                if (!cachedPublicIP.empty()) {
                    draw_text("Internet",  200, 262, 14, {120,130,170,255});
                    draw_text(fmt_buf("%s:%u", cachedPublicIP.c_str(), (unsigned)hostPort), 200, 280, 18, GREEN);
                    draw_text("(port forwarding required)", 200, 302, 13, GRAY);
                } else {
                    draw_text("Internet: resolving...", 200, 262, 14, GRAY);
                }
                draw_rect(200, 330, 320, 1, accentL);
                gui_text_center(360, 348, "Waiting for connection...", 20, WHITE);
                gui_text_center(360, 400, "[Q] Back to Menu", 18, GRAY);
            }
            else
            {
                if (session.hasFailed()) {
                    gui_text_center(360, 195, "Connection Failed", 28, RED);
                    gui_text_center(360, 240, "Check IP address and port", 18, GRAY);
                    gui_text_center(360, 400, "[Q] Back to Menu", 20, YELLOW);
                } else {
                    gui_text_center(360, 200, "Connecting...", 28, WHITE);
                    gui_text_center(360, 400, "[Q] Cancel", 18, GRAY);
                }
            }
            if (platform_key_pressed(PKEY_Q))
            {
                if (queueMode) session.QueueCancel();
                session.Close();
                queueMode = false; netMode = false;
                localIpDone = false; publicIpLaunched = false;
                cachedLocalIP.clear(); cachedPublicIP.clear();
                app = AppMode::Menu;
            }
        }

        // ── NET HUD (최소) ────────────────────────────────────────────────────
        if (app == AppMode::Net)
        {
            draw_text(fmt_buf("NET: %s", session.isConnected() ? "CONNECTED" : "DISCONNECTED"),
                      10, 580, 10, RAYWHITE);
            if (session.isReady())
            {
                draw_text(fmt_buf("SEED: 0x%08x", (unsigned)(sessionSeed & 0xFFFFFFFFu)),
                          10, 594, 10, RAYWHITE);
                draw_text(fmt_buf("TICKS localSent=%u remoteMax=%u sim=%u delay=%u",
                                  (unsigned)((localTickNext == 0) ? 0 : localTickNext - 1),
                                  (unsigned)session.maxRemoteTick(),
                                  (unsigned)simTick, (unsigned)inputDelay),
                          10, 606, 10, RAYWHITE);
            }

            // 링크 상태 오버레이 — Stalled 는 은은하게, Lost 는 grace 카운트다운.
            net::LinkStatus ls = session.linkStatus();
            if (linkLostActive) {
                int remain = (int)(linkLostCountdown + 0.999f);
                draw_rect(60, 240, 600, 120, {0, 0, 0, 200});
                draw_text("Opponent disconnected", 130, 260, 32, RED);
                draw_text(fmt_buf("Returning to title in %d...", remain),
                          130, 308, 24, WHITE);
            } else if (ls == net::LinkStatus::Stalled && gameLocal && gameRemote) {
                draw_text("Opponent frozen - waiting...", 60, 560, 14, YELLOW);
            }

            // F.2 DESYNC 배너 — 한 번 감지되면 세션 종료까지 유지.
            if (desyncDetected) {
                draw_rect(60, 180, 600, 52, {40, 0, 0, 220});
                draw_text(fmt_buf("DESYNC DETECTED at tick %u", desyncTick),
                          120, 192, 20, RED);
                draw_text("(state hashes diverged - lockstep broken)",
                          90, 214, 14, YELLOW);
            }

            // ── Section E: 채팅 오버레이 ─────────────────────────────────────
            // 좌측 하단에 최근 수신/송신 메시지 (최대 kChatHistoryMax 줄).
            // 작은 폰트(14px) + 반투명 배경. 10초 경과 시 자동 제거(노화).
            // 아래쪽이 가장 최신 — 위에서부터 오래된 순서.
            {
                int base_y = 500;
                int i = 0;
                for (auto& line : chatHistory) {
                    // 페이드아웃: 남은 시간 < 2초이면 알파 점진 감소.
                    float remaining = kChatFadeSeconds - line.ageSec;
                    uint8_t a = 255;
                    if (remaining < 2.0f) a = (uint8_t)(255.0f * (remaining / 2.0f));
                    Color c = (line.prefix == "You")
                        ? Color{180, 220, 255, a}
                        : Color{255, 220, 180, a};
                    std::string s = line.prefix + ": " + line.text;
                    draw_text(s.c_str(), 14, base_y + i * 16, 14, c);
                    i++;
                }
            }

            // 채팅 입력창 — composing 중에만 표시. 커서는 끝에 '_' 로.
            if (chatComposing) {
                draw_rect(6, 624, 708, 28, {0, 0, 0, 220});
                std::string shown = "Chat> " + chatInput + "_";
                draw_text(shown.c_str(), 14, 630, 16, WHITE);
                // 힌트 — 우측 끝.
                draw_text("[Enter send  Esc cancel]", 480, 632, 12, GRAY);
            } else if (session.isReady()) {
                // 가벼운 힌트 — 우측 상단 근처.
                draw_text("[T] chat", 670, 594, 10, GRAY);
            }
        }

        // ── 인게임 나가기 버튼 + 확인 모달 ─────────────────────────────────
        // 모든 인게임 모드(Single / BotSingle / Net) 에서 우상단 X 버튼을 렌더.
        // Net 모드에서 채팅 중일 땐 마우스 클릭이 X 를 건드리지 않도록 숨김.
        const bool inGame =
            (app == AppMode::Single    && gameSingle) ||
            (app == AppMode::BotSingle && gameSingle && gameBot) ||
            (app == AppMode::Net       && gameLocal && gameRemote);
        if (inGame && !quitDialogOpen) {
            // 화면은 720x640. X 버튼은 우상단 2px 마진.
            if (gui_close_button(720 - 32 - 2, 2, 32)) {
                quitDialogOpen = true;
            }
        }

        if (quitDialogOpen) {
            // 반투명 배경으로 뒤 게임 렌더를 어둡게.
            gui_modal_dim(720, 640);

            // 모달 박스 (중앙, 420x220)
            const int mw = 420, mh = 220;
            const int mx = (720 - mw) / 2;
            const int my = (640 - mh) / 2;
            draw_rect_rounded(mx, my, mw, mh, 0.15f, {28, 32, 48, 255});

            // 타이틀 + 설명 (모드별 문구)
            gui_text_center(360, my + 24, "정말 나가시겠습니까?", 28, WHITE);
            const char* line1 = nullptr;
            const char* line2 = nullptr;
            if (app == AppMode::Net) {
                line1 = "나가면 패배로 기록됩니다.";
                line2 = "게임은 상대방이 계속 진행합니다.";
            } else {
                line1 = "현재 게임이 중지됩니다.";
                line2 = "";
            }
            gui_text_center(360, my + 70,  line1, 16, GRAY);
            if (line2 && *line2) gui_text_center(360, my + 92, line2, 16, GRAY);

            // Yes/No 버튼 (각 140x44, 중앙에서 좌우 분리).
            const int bw = 140, bh = 44;
            const int gap = 30;
            const int byPos = my + mh - bh - 24;
            const int bxYes = 360 - bw - gap / 2;
            const int bxNo  = 360 + gap / 2;

            bool clickYes = gui_button(bxYes, byPos, bw, bh, "예 (Y)", 22);
            bool clickNo  = gui_button(bxNo,  byPos, bw, bh, "아니오 (N)", 22);

            // 키보드: Y = Yes, N = No, Enter = Yes (관성), Escape 는 창 닫기라 피함.
            if (platform_key_pressed(PKEY_Y) || platform_key_pressed(PKEY_ENTER))
                clickYes = true;
            if (platform_key_pressed(PKEY_N))
                clickNo = true;

            if (clickNo) {
                quitDialogOpen = false;
            } else if (clickYes) {
                quitDialogOpen = false;
                // Net: 세션 종료 → 상대에게 단절 전달(= 패배 기록) → 메뉴로.
                if (app == AppMode::Net) {
                    session.Close();
                    netMode = false; isHost = false; queueMode = false;
                    gameLocal.reset();
                    gameRemote.reset();
                }
                // Single/BotSingle: 게임 객체 파기 → 메뉴로.
                if (app == AppMode::Single) {
                    gameSingle.reset();
                }
                if (app == AppMode::BotSingle) {
                    gameSingle.reset();
                    gameBot.reset();
                    botInputQueue.clear();
                }
                app = AppMode::Menu;
            }
        }

        renderer_end();
        platform_end_frame();
    }

    image_unload(iconYou);
    image_unload(iconOpponent);
    image_unload(iconBot);
    renderer_shutdown();
    platform_shutdown();
    net::net_shutdown();
    return 0;
}
