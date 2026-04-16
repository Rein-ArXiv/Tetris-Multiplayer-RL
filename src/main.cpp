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
#include "colors.h"
#include "../platform/platform.h"
#include "../renderer/renderer.h"
#include "../renderer/shake.h"

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
//   틱에서 ConsumeInput()으로 소비 + held 키(DOWN)를 합산한다.
static uint8_t s_pendingInput = 0;

static void AccumulateInput()
{
    if (platform_key_pressed(PKEY_LEFT))  s_pendingInput |= INPUT_LEFT;
    if (platform_key_pressed(PKEY_RIGHT)) s_pendingInput |= INPUT_RIGHT;
    if (platform_key_pressed(PKEY_UP))    s_pendingInput |= INPUT_ROTATE;
    if (platform_key_pressed(PKEY_SPACE)) s_pendingInput |= INPUT_DROP;
}

static uint8_t ConsumeInput()
{
    uint8_t mask = s_pendingInput;
    s_pendingInput = 0;
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

enum class AppMode { Menu, ConnectInput, Single, BotSingle, Net };

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

    bool netMode = false, isHost = false, queueMode = false;
    std::string hostIp;
    uint16_t hostPort = 7777;
    std::string queueHost;
    uint16_t queuePort = 7777;

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

    // Section I — 화면 흔들림. 이벤트성 트리거 → 감쇠. 결정론 영향 없음.
    ShakeState shake{};

    // Section I — 콜아웃 텍스트 ("DOUBLE!" / "TRIPLE!" / "TETRIS!").
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

    float accumulator = 0.0f;

    // ── 메인 루프 ───────────────────────────────────────────────────────────
    while (!platform_should_close())
    {
        // 1) 입력 처리 + 델타타임
        float deltaTime = platform_begin_frame();
        AccumulateInput();  // 엣지 트리거 입력을 매 프레임 누적

        // 2) 고정 틱 시뮬레이션 (60Hz)
        accumulator += deltaTime;
        while (accumulator >= SECONDS_PER_TICK)
        {
            uint8_t inputMask = ConsumeInput();

            if (app == AppMode::Net && session.isConnected())
            {
                localInputs[localTickNext] = inputMask;
                session.SendInput(localTickNext, inputMask);
                localTickNext++;

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

                                // Section I: Shake 트리거. 라인 수·받은 가비지·topout 별 세기.
                                auto trigger_for_lines = [&](int lines) {
                                    switch (lines) {
                                        case 1: shake_trigger(shake, 2.0f, 0.10f); break;
                                        case 2: shake_trigger(shake, 4.0f, 0.15f); break;
                                        case 3: shake_trigger(shake, 6.0f, 0.20f); break;
                                        case 4: shake_trigger(shake, 8.0f, 0.25f); break;
                                    }
                                };
                                // 내 보드에서 클리어 / 가비지 받음 → 내 화면이 흔들린다.
                                if (gameLocal->sim.lastLinesCleared > 0) {
                                    trigger_for_lines(gameLocal->sim.lastLinesCleared);
                                    trigger_callout(coLocal, gameLocal->sim.lastLinesCleared);
                                }
                                if (gameLocal->sim.lastGarbageReceived > 0)
                                    shake_trigger(shake, 6.0f, 0.20f);
                                if (gameLocal->sim.gameOverEvent)
                                    shake_trigger(shake, 16.0f, 0.50f);
                                gameLocal->sim.lastLinesCleared = 0;
                                gameLocal->sim.lastGarbageReceived = 0;
                                gameLocal->sim.gameOverEvent = false;
                                // 상대 측 클리어도 콜아웃으로 알림.
                                if (gameRemote->sim.lastLinesCleared > 0)
                                    trigger_callout(coRemote, gameRemote->sim.lastLinesCleared);
                                gameRemote->sim.lastLinesCleared = 0;
                                gameRemote->sim.lastGarbageReceived = 0;
                                gameRemote->sim.gameOverEvent = false;
                            }
                            simTick++;

                            // F.2: 600틱마다 로컬 해시 송신 + 링 기록.
                            if (simTick > 0 && simTick % HASH_PERIOD_TICKS == 0 &&
                                simTick != lastHashSentTick) {
                                uint64_t h = gameLocal->ComputeStateHash();
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
                // Section I: 싱글 모드에서도 자기 클리어/게임오버 shake.
                if (gameSingle->sim.lastLinesCleared > 0) {
                    switch (gameSingle->sim.lastLinesCleared) {
                        case 1: shake_trigger(shake, 2.0f, 0.10f); break;
                        case 2: shake_trigger(shake, 4.0f, 0.15f); break;
                        case 3: shake_trigger(shake, 6.0f, 0.20f); break;
                        case 4: shake_trigger(shake, 8.0f, 0.25f); break;
                    }
                    trigger_callout(coLocal, gameSingle->sim.lastLinesCleared);
                }
                if (gameSingle->sim.gameOverEvent)
                    shake_trigger(shake, 16.0f, 0.50f);
                gameSingle->sim.lastLinesCleared = 0;
                gameSingle->sim.lastGarbageReceived = 0;
                gameSingle->sim.gameOverEvent = false;
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
                        fprintf(stderr, "[DESYNC] tick=%u local=0x%016llx remote=0x%016llx\n",
                                rt, (unsigned long long)slot.hash, (unsigned long long)rh);
                        desyncDetected = true;
                        desyncTick = rt;
                    }
                }
                // 같은 틱이 링에 없을 수도 있음(시작 직후 등) — 이 경우 무시.
            }
        }

        // 3) 렌더링
        // Section I: shake 업데이트 + 뷰 오프셋 적용. 메뉴/네트 대기 등도 통과하지만
        // trigger 가 걸리지 않으면 dx=dy=0 이라 영향 없음.
        shake_update(shake, deltaTime);
        if (coLocal.timeLeft  > 0.0f) coLocal.timeLeft  -= deltaTime;
        if (coRemote.timeLeft > 0.0f) coRemote.timeLeft -= deltaTime;
        {
            float sdx = 0.0f, sdy = 0.0f;
            shake_offset(shake, sdx, sdy);
            renderer_set_view_offset((int)sdx, (int)sdy);
        }

        renderer_begin(darkBlue);

        // ── 메뉴 ────────────────────────────────────────────────────────────
        if (app == AppMode::Menu)
        {
            draw_text("TETRIS", 220, 100, 60, WHITE);
            // 봇 추론(ONNX) 경로는 아직 연결 전 — Plan Section C 에서 `model/policy.onnx`
            // 가 함께 올라올 때까지는 "Single vs Bot" 을 회색 비활성 표시로 둔다.
            const bool botAvailable = false;
            constexpr Color DISABLED = {70, 70, 70, 255};
            const char* items[] = {
                "Single Player",
                "Single vs Bot",
                "Host (port 7777)",
                "Connect (enter IP:port)",
                "Exit",
            };
            constexpr int kMenuCount = 5;
            for (int i = 0; i < kMenuCount; ++i)
            {
                Color c = (i == menuIndex) ? WHITE : GRAY;
                if (i == 1 && !botAvailable) c = DISABLED;
                draw_text(items[i], 180, 220 + i * 40, 32, c);
            }
            if (!botAvailable && menuIndex == 1) {
                draw_text("(model/policy.onnx not found)", 180, 220 + 1 * 40 + 28, 14, DISABLED);
            }
            if (platform_key_pressed(PKEY_DOWN)) menuIndex = (menuIndex + 1) % kMenuCount;
            if (platform_key_pressed(PKEY_UP))   menuIndex = (menuIndex + kMenuCount - 1) % kMenuCount;
            if (platform_key_pressed(PKEY_ENTER) || platform_key_pressed(PKEY_SPACE))
            {
                if (menuIndex == 0) {
                    app = AppMode::Single;
                    gameSingle = std::make_unique<Game>(sessionSeed);
                } else if (menuIndex == 1) {
                    if (botAvailable) {
                        app = AppMode::BotSingle;
                        gameSingle = std::make_unique<Game>(sessionSeed);
                        // 우측 보드(gameLocal/gameRemote 재사용 예정)는 Section C 에서 합류.
                    }
                    // 비활성 상태에선 Enter 를 흡수(이동/사운드 없음).
                } else if (menuIndex == 2) {
                    isHost = true; netMode = true;
                    sessionSeed = (uint64_t)(platform_get_time() * 1000000.0) + 0xC0FFEEULL;
                    net::SeedParams sp{sessionSeed, startDelay, inputDelay, net::Role::Host};
                    if (session.Host(hostPort, sp)) app = AppMode::Net;
                    else fprintf(stderr, "Host failed - port already in use?\n");
                } else if (menuIndex == 3) {
                    app = AppMode::ConnectInput;
                    connectText.clear(); connectError = false; connectErrorMsg.clear();
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

        // ── 1인 모드 ─────────────────────────────────────────────────────────
        if (app == AppMode::Single && gameSingle)
        {
            draw_text("Score", 365, 15, 38, WHITE);
            draw_text("Next",  370, 175, 38, WHITE);
            draw_rect_rounded(320, 55, 170, 60, 0.3f, lightBlue);
            char scoreText[16]; snprintf(scoreText, sizeof(scoreText), "%d", gameSingle->score);
            int tw = measure_text(scoreText, 38);
            draw_text(scoreText, 320 + (170 - tw) / 2, 65, 38, WHITE);
            draw_rect_rounded(320, 215, 170, 180, 0.3f, lightBlue);
            gameSingle->Draw();
            // Section I — 싱글 모드 콜아웃.
            if (coLocal.text && coLocal.timeLeft > 0.0f) {
                int tw = measure_text(coLocal.text, 56);
                draw_text(coLocal.text, 150 - tw / 2, 280, 56, YELLOW);
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
            draw_text("Local",  leftX,  8, 22, WHITE);
            draw_text("Remote", rightX, 8, 22, WHITE);
            gameLocal->DrawBoardAt(leftX,  11);
            gameRemote->DrawBoardAt(rightX, 11);
            draw_text(fmt_buf("Score: %d", gameLocal->score),  leftX,  640 - 28, 20, WHITE);
            draw_text(fmt_buf("Score: %d", gameRemote->score), rightX, 640 - 28, 20, WHITE);

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
            if (queueMode)
            {
                if (session.hasFailed()) {
                    draw_text("Matchmaking failed",            190, 200, 28, RED);
                    draw_text("(relay unreachable or timeout)", 140, 240, 18, GRAY);
                    draw_text("[Q] Back to Menu",              220, 320, 20, YELLOW);
                } else if (!session.isConnected()) {
                    draw_text(fmt_buf("Connecting to relay %s:%u...",
                                       queueHost.c_str(), (unsigned)queuePort),
                              80, 220, 22, WHITE);
                    draw_text("[Q] Cancel", 260, 320, 20, GRAY);
                } else {
                    draw_text("Searching for opponent...", 160, 220, 26, WHITE);
                    draw_text("(connected, waiting for match)", 150, 250, 16, GRAY);
                    draw_text("[Q] Cancel", 260, 320, 20, GRAY);
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

                if (session.isListening())
                {
                    draw_text(fmt_buf("Hosting on port %u", (unsigned)hostPort), 180, 120, 28, WHITE);
                    draw_text("Same WiFi:", 50, 160, 20, GRAY);
                    draw_text(fmt_buf("%s:%u", cachedLocalIP.c_str(), (unsigned)hostPort), 50, 180, 18, YELLOW);
                    if (!cachedPublicIP.empty()) {
                        draw_text("Internet:", 350, 160, 20, GRAY);
                        draw_text(fmt_buf("%s:%u", cachedPublicIP.c_str(), (unsigned)hostPort), 350, 180, 18, GREEN);
                        draw_text("(Need port forwarding)", 300, 200, 16, GRAY);
                    } else {
                        draw_text("Internet: Getting IP...", 350, 160, 16, GRAY);
                    }
                }
                draw_text("Waiting for connection...", 250, 250, 24, WHITE);
                draw_text("[Q] Back to Menu",          240, 320, 20, GRAY);
            }
            else
            {
                if (session.hasFailed()) {
                    draw_text("Connection Failed!",         200, 200, 28, RED);
                    draw_text("Check IP address and port", 160, 240, 20, GRAY);
                    draw_text("[Q] Back to Menu",           220, 320, 20, YELLOW);
                } else {
                    draw_text("Connecting...", 240, 220, 28, WHITE);
                    draw_text("[Q] Cancel",    260, 320, 20, GRAY);
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
        }

        renderer_end();
        platform_end_frame();
    }

    renderer_shutdown();
    platform_shutdown();
    net::net_shutdown();
    return 0;
}
