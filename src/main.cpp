#include <raylib.h>
#include <iostream>
#include <cstdio>
#include "../core/constants.h"
#include "../core/input.h"
#include "../core/replay.h"
#include "../core/hash.h"
#include "../net/session.h"
#include "../net/socket.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <filesystem>
#include "game.h"
#include "colors.h"

// 키보드 입력을 비트마스크로 변환 (core/input.h 참조)
// Lockstep 전체 흐름: docs/QUICK_START.md
static uint8_t SampleInput()
{
    uint8_t mask = INPUT_NONE;
    if (IsKeyPressed(KEY_LEFT))  mask |= INPUT_LEFT;
    if (IsKeyPressed(KEY_RIGHT)) mask |= INPUT_RIGHT;
    if (IsKeyDown(KEY_DOWN))     mask |= INPUT_DOWN;
    if (IsKeyPressed(KEY_UP))    mask |= INPUT_ROTATE;
    if (IsKeyPressed(KEY_SPACE)) mask |= INPUT_DROP;
    return mask;
}

enum class AppMode { Menu, ConnectInput, Single, Net };

// 게임 오버 상태 머신
enum class GameOverState {
    None,                    // 게임 중
    ShowingGameOver,         // "GAME OVER" 표시, 선택 대기
    WaitingForRemote,        // 내 선택 완료, 상대 선택 대기
    ShowingDisagreement,     // 불일치 메시지 표시 (3초)
    RestartingGame,          // 재시작 준비
    GoingToTitle,            // 타이틀 복귀
};

int main(int argc, char** argv)
{
    // [NET] 네트워킹 초기화
    if (!net::net_init()) {
        std::cerr << "Failed to initialize networking\n";
        return 1;
    }

    // [NET] 멀티보드 뷰를 고려하여 기본 폭을 늘립니다.
    InitWindow(720, 640, "Tetris");
    SetTargetFPS(60);                       // Set FPS

    Font font = LoadFontEx("Font/monogram.ttf", 64, 0, 0);


    // [NET] 이 시드는 실제로는 세션 핸드셰이크(HELLO/SEED)에서 합의된 값을 사용해야 합니다.
    // CLI 인자 처리(학습용): --host [port], --connect ip:port
    bool netMode = false; bool isHost = false; std::string hostIp; uint16_t hostPort=7777;
    for (int i=1;i<argc;++i) {
        std::string a = argv[i];
        if (a == "--host") { isHost = true; netMode = true; if (i+1<argc) { hostPort = (uint16_t)std::stoi(argv[i+1]); i++; } }
        else if (a == "--connect") {
            netMode = true; if (i+1<argc) { std::string ep = argv[i+1]; i++; auto pos=ep.find(":"); hostIp = ep.substr(0,pos); hostPort = (uint16_t)std::stoi(ep.substr(pos+1)); }
        }
    }

    uint64_t sessionSeed = 0xDEADBEEFCAFEBABEull;
    net::Session session;
    uint32_t startDelay = 120; uint8_t inputDelay = 2;  // 입력 지연 4틱 → 2틱으로 감소
    // [NET] 틱 카운터: 입력 수집/전송된 틱(localTickNext), 시뮬레이션 완료된 틱(simTick)
    uint32_t localTickNext = 0;
    uint32_t simTick = 0;
    std::unordered_map<uint32_t, uint8_t> localInputs;
    
    // App mode: if no CLI, show menu; else jump directly
    AppMode app = netMode ? AppMode::Net : AppMode::Menu;
    int menuIndex = 0; // 0:Single, 1:Host, 2:Connect, 3:Exit
    std::string connectText; bool connectError = false;
    
    // If CLI selected netMode, start network immediately
    if (app == AppMode::Net) {
        if (isHost) {
            sessionSeed = ((uint64_t)GetTime()*1000000.0) + 0xC0FFEEULL;
            net::SeedParams sp{sessionSeed, startDelay, inputDelay, net::Role::Host};
            if (!session.Host(hostPort, sp)) { TraceLog(LOG_ERROR, "Host failed"); }
        } else {
            if (!session.Connect(hostIp, hostPort)) { TraceLog(LOG_ERROR, "Connect failed"); }
        }
    }

    std::unique_ptr<Game> gameSingle;
    std::unique_ptr<Game> gameLocal;
    std::unique_ptr<Game> gameRemote;

    // [FIX] IP 주소를 한 번만 조회하여 캐시
    static std::string cachedLocalIP = "";
    static std::string cachedPublicIP = "";
    static bool ipCached = false;

    // [NET] 게임 보드는 세션 ready 후에 올바른 시드로 생성합니다

    // [NET] 리플레이 기록(간단). F5로 시작, F6로 저장 종료.
    bool recording = false;
    ReplayData replay;
    replay.seed = sessionSeed;
    replay.frames.clear();

    // 게임 오버 상태 관리
    GameOverState gameOverState = GameOverState::None;
    net::GameOverChoice myGameOverChoice = net::GameOverChoice::None;
    float gameOverTimer = 0.0f;
    const float GAME_OVER_TIMEOUT = 30.0f;
    const float DISAGREEMENT_COUNTDOWN = 3.0f;

    float accumulator = 0.0f;
    while(WindowShouldClose() == false)     // Press ESC or click X button
    {
        if (gameSingle) UpdateMusicStream(gameSingle->music);
        if (gameLocal) UpdateMusicStream(gameLocal->music);

        // Fixed-timestep simulation
        // [NET] Lockstep: 입력을 해당 틱에 모두 수신했다면 그 틱을 한 번만 진행합니다.
        // 본 로컬 버전에선 네트워크 큐 대신 로컬 키 입력을 즉시 적용합니다.
        accumulator += GetFrameTime();
        while (accumulator >= SECONDS_PER_TICK)
        {
            uint8_t inputMask = SampleInput();
            if (inputMask == INPUT_NONE && IsKeyPressed(KEY_ENTER) && gameSingle && gameSingle->gameOver)
            {
                // Restart on any key when game over
                // Handled in render branch below (legacy behavior)
            }
            else
            {
                if (app == AppMode::Net && session.isConnected()) {

                    // 준비 이전에도 입력 전송(원격 버퍼링)
                    localInputs[localTickNext] = inputMask;
                    session.SendInput(localTickNext, inputMask);
                    localTickNext++;

                    // 핸드셰이크 완료 → 양쪽 동일 시드로 보드 초기화 (결정론 보장)
                    if (session.isReady() && (!gameLocal || !gameRemote)) {
                        sessionSeed = session.params().seed;
                        inputDelay = session.params().input_delay;
                        gameLocal = std::make_unique<Game>(sessionSeed);
                        gameRemote = std::make_unique<Game>(sessionSeed);
                        localInputs.clear();
                        localTickNext = 0; simTick = 0; startDelay = session.params().start_tick;
                        accumulator -= SECONDS_PER_TICK; continue;
                    }

                    if (session.isReady()) {
                        // 시작 전 카운트다운
                        if (startDelay > 0) { startDelay--; accumulator -= SECONDS_PER_TICK; continue; }

                        // Lockstep 안전 틱 계산 (상세: docs/QUICK_START.md#q1)
                        int64_t lastLocalSent = (localTickNext == 0) ? -1 : (int64_t)localTickNext - 1;
                        int64_t lastRemote = (int64_t)session.maxRemoteTick();
                        int64_t safeTickInclusive = std::min(lastLocalSent, lastRemote) - (int64_t)inputDelay;

                        // 안전 틱까지 시뮬레이션 진행
                        if ((int64_t)simTick <= safeTickInclusive && gameLocal && gameRemote) {
                            while ((int64_t)simTick <= safeTickInclusive) {
                                uint8_t li = 0, ri = 0;
                                auto it = localInputs.find(simTick);
                                if (it != localInputs.end()) li = it->second;
                                if (!session.GetRemoteInput(simTick, ri)) break;

                                gameLocal->SubmitInput(li);
                                gameRemote->SubmitInput(ri);
                                gameLocal->Tick();
                                gameRemote->Tick();
                                simTick++;
                            }
                        }
                    }
                } else if (app == AppMode::Single && gameSingle) {
                    gameSingle->SubmitInput(inputMask);
                    gameSingle->Tick();
                } else {
                }
            }
            if (recording)
            {
                FrameInputs fr{}; fr.p1 = inputMask; fr.p2 = 0;
                replay.frames.push_back(fr);
            }
            accumulator -= SECONDS_PER_TICK;
        }

        BeginDrawing();         // Draw empty canvas
        ClearBackground(darkBlue);
        
        // Menu/UI state machine
        if (app == AppMode::Menu) {
            DrawTextEx(font, "TETRIS", {220, 100}, 60, 2, WHITE);
            const char* items[] = { "Single Player", "Host (port 7777)", "Connect (enter IP:port)", "Exit" };
            for (int i=0;i<4;++i) {
                Color c = (i==menuIndex)? WHITE : GRAY;
                DrawTextEx(font, items[i], {180, (float)(220 + i*40)}, 32, 2, c);
            }
            // Navigate
            if (IsKeyPressed(KEY_DOWN)) menuIndex = (menuIndex+1) % 4;
            if (IsKeyPressed(KEY_UP)) menuIndex = (menuIndex+3) % 4;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (menuIndex == 0) {
                    app = AppMode::Single;
                    gameSingle = std::make_unique<Game>(sessionSeed);
                } else if (menuIndex == 1) {
                    isHost = true; netMode = true; app = AppMode::Net;
                    sessionSeed = ((uint64_t)GetTime()*1000000.0) + 0xC0FFEEULL;
                    net::SeedParams sp{sessionSeed, startDelay, inputDelay, net::Role::Host};
                    if (!session.Host(hostPort, sp)) { TraceLog(LOG_ERROR, "Host failed"); }
                } else if (menuIndex == 2) {
                    app = AppMode::ConnectInput; connectText.clear(); connectError = false;
                } else if (menuIndex == 3) {
                    CloseWindow(); return 0;
                }
            }
        } else if (app == AppMode::ConnectInput) {
            DrawTextEx(font, "Enter IP:port", {180, 200}, 40, 2, WHITE);
            DrawRectangle(180, 250, 360, 44, lightBlue);
            DrawTextEx(font, connectText.c_str(), {190, 256}, 32, 2, WHITE);
            if (connectError) DrawTextEx(font, "Invalid endpoint", {180, 310}, 24, 2, red);
            // Text input
            int ch = GetCharPressed();
            while (ch > 0) {
                if ((ch >= '0' && ch <= '9') || ch == '.' || ch == ':') connectText.push_back((char)ch);
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !connectText.empty()) connectText.pop_back();
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                auto pos = connectText.find(":");
                if (pos != std::string::npos && pos > 0 && pos < connectText.length() - 1) {
                    hostIp = connectText.substr(0,pos);
                    std::string portStr = connectText.substr(pos+1);

                    // IP 주소와 포트 유효성 검사
                    try {
                        hostPort = (uint16_t)std::stoi(portStr);
                        if (hostPort == 0) throw std::exception();
                    } catch(...) {
                        connectError = true;
                        connectText = "Invalid port number!";
                    }

                    // IP 주소 기본 형식 검사 - 오류가 없을 때만 진행
                    if (!connectError && (hostIp.empty() || hostIp.find_first_not_of("0123456789.") != std::string::npos)) {
                        connectError = true;
                        connectText = "Invalid IP address format!";
                    }

                    // 오류가 없을 때만 연결 시도
                    if (!connectError) {
                        netMode = true; isHost = false; app = AppMode::Net;
                        std::cout << "[NET] Attempting to connect to " << hostIp << ":" << hostPort << std::endl;

                        if (!session.Connect(hostIp, hostPort)) {
                            TraceLog(LOG_ERROR, "Connect failed to %s:%d", hostIp.c_str(), hostPort);
                            connectError = true;
                            connectText = "Connection failed! Check IP/port/firewall";
                            app = AppMode::ConnectInput;
                        }
                        else {
                            std::cout << "[NET] Connection initiated successfully" << std::endl;
                            // 보드는 ready 후 세션 시드로 생성
                        }
                    }
                } else {
                    connectError = true;
                    connectText = "Format: IP:PORT (e.g. 192.168.1.100:7777)";
                }
            }
            if (IsKeyPressed(KEY_ESCAPE)) { app = AppMode::Menu; }
        }

        // Single board rendering
        if (app == AppMode::Single && gameSingle) {
            DrawTextEx(font, "Score", {365, 15}, 38, 2, WHITE);
            DrawTextEx(font, "Next", {370, 175}, 38, 2, WHITE);
            DrawRectangleRounded({320, 55, 170, 60}, 0.3, 6, lightBlue);
            char scoreText[10]; sprintf(scoreText, "%d", gameSingle->score);
            Vector2 textSize = MeasureTextEx(font, scoreText, 38, 2);
            DrawTextEx(font, scoreText, {320 + (170 - textSize.x)/2, 65}, 38, 2, WHITE);
            DrawRectangleRounded({320, 215, 170, 180}, 0.3, 6, lightBlue);
            gameSingle->Draw();
        }
        if (IsKeyPressed(KEY_F5)) { recording = true; replay.frames.clear(); }
        if (IsKeyPressed(KEY_F6)) {
            if (recording) {
                std::error_code ec; std::filesystem::create_directories("out", ec);
                ReplayIO::Save("out/replay.txt", replay);
                recording = false;
            }
        }

        if (IsKeyPressed(KEY_H)) {
            unsigned long long h1 = 0;
            if (gameSingle) h1 = gameSingle->ComputeStateHash();
            unsigned long long hL = 0, hR = 0;
            if (gameLocal) hL = gameLocal->ComputeStateHash();
            if (gameRemote) hR = gameRemote->ComputeStateHash();
            std::cout << "Hash(single): 0x" << std::hex << h1
                      << " local: 0x" << hL << " remote: 0x" << hR << std::dec << std::endl;
        }

        if (app == AppMode::Single && gameSingle && gameSingle->gameOver)
        {
            DrawTextEx(font, "GAME OVER", {120, 300}, 60, 2, WHITE);
            DrawTextEx(font, "Press Any key To Restart", {85, 350}, 25, 2, WHITE);
            // Restart if any key pressed
            if (GetKeyPressed() != 0)
            {
                gameSingle->gameOver = false;
                // Reinitialize the game state deterministically (same seed)
                // Alternatively, could reseed for variation
                // [NET] 온라인에선 서버/호스트가 정한 시드/시작틱을 다시 적용해야 합니다.
                gameSingle = std::make_unique<Game>(sessionSeed);
                if (recording) replay.frames.clear();
            }
        }
        // [NET] 멀티 모드 그리기: 좌우 보드 (연결/준비 이전에도 레이아웃 표시)
        if (app == AppMode::Net && gameLocal && gameRemote) {
            ClearBackground(darkBlue);
            // 보드 원점
            Vector2 leftOrigin { 11, 11 };
            Vector2 rightOrigin { 11 + 300 + 60, 11 };
            // 타이틀
            DrawTextEx(font, "Local", {leftOrigin.x, 8}, 22, 2, WHITE);
            DrawTextEx(font, "Remote", {rightOrigin.x, 8}, 22, 2, WHITE);
            // 보드
            gameLocal->DrawBoardAt((int)leftOrigin.x, (int)leftOrigin.y);
            gameRemote->DrawBoardAt((int)rightOrigin.x, (int)rightOrigin.y);
            // 점수 표기
            DrawTextEx(font, TextFormat("Score: %d", gameLocal->score), {leftOrigin.x, 620-28}, 20, 1, WHITE);
            DrawTextEx(font, TextFormat("Score: %d", gameRemote->score), {rightOrigin.x, 620-28}, 20, 1, WHITE);

            // 게임 오버 감지 및 상태 전환
            if ((gameLocal->gameOver || gameRemote->gameOver) && gameOverState == GameOverState::None) {
                gameOverState = GameOverState::ShowingGameOver;
                myGameOverChoice = net::GameOverChoice::None;
                gameOverTimer = 0.0f;
                std::cout << "[GAME] Game over detected, entering choice screen" << std::endl;
            }

            // 게임 오버 상태별 처리
            if (gameOverState == GameOverState::ShowingGameOver) {
                DrawTextEx(font, "GAME OVER", {220, 280}, 60, 2, WHITE);
                DrawTextEx(font, "[R] Restart", {240, 350}, 28, 2, GREEN);
                DrawTextEx(font, "[ESC] Go to Title", {200, 385}, 28, 2, YELLOW);

                if (IsKeyPressed(KEY_R)) {
                    myGameOverChoice = net::GameOverChoice::Restart;
                    session.SendGameOverChoice(myGameOverChoice);
                    gameOverState = GameOverState::WaitingForRemote;
                    gameOverTimer = 0.0f;
                    std::cout << "[GAME] Selected: Restart, waiting for opponent..." << std::endl;
                } else if (IsKeyPressed(KEY_ESCAPE)) {
                    myGameOverChoice = net::GameOverChoice::GoToTitle;
                    session.SendGameOverChoice(myGameOverChoice);
                    gameOverState = GameOverState::WaitingForRemote;
                    gameOverTimer = 0.0f;
                    std::cout << "[GAME] Selected: GoToTitle, waiting for opponent..." << std::endl;
                }
            }
            else if (gameOverState == GameOverState::WaitingForRemote) {
                DrawTextEx(font, "GAME OVER", {220, 280}, 60, 2, WHITE);
                DrawTextEx(font, "Waiting for opponent...", {180, 450}, 24, 2, GRAY);

                // 타임아웃 시간 표시
                gameOverTimer += GetFrameTime();
                int remaining = (int)(GAME_OVER_TIMEOUT - gameOverTimer);
                if (remaining < 0) remaining = 0;
                DrawTextEx(font, TextFormat("Timeout in %ds", remaining), {250, 480}, 20, 2, GRAY);

                net::GameOverChoice remoteChoice;
                if (session.GetRemoteGameOverChoice(remoteChoice)) {
                    std::cout << "[GAME] Received opponent choice: " << (int)remoteChoice << std::endl;

                    if (myGameOverChoice == remoteChoice) {
                        // 양쪽 선택 일치
                        if (myGameOverChoice == net::GameOverChoice::Restart) {
                            std::cout << "[GAME] Both chose Restart, restarting game..." << std::endl;
                            gameOverState = GameOverState::RestartingGame;
                        } else {
                            std::cout << "[GAME] Both chose GoToTitle, returning to menu..." << std::endl;
                            gameOverState = GameOverState::GoingToTitle;
                        }
                    } else {
                        // 불일치
                        std::cout << "[GAME] Choices differ! My:" << (int)myGameOverChoice
                                  << " Remote:" << (int)remoteChoice << std::endl;
                        gameOverState = GameOverState::ShowingDisagreement;
                        gameOverTimer = 0.0f;
                    }
                }

                // 타임아웃 처리
                if (gameOverTimer >= GAME_OVER_TIMEOUT) {
                    std::cout << "[GAME] Timeout waiting for opponent, disconnecting..." << std::endl;
                    gameOverState = GameOverState::GoingToTitle;
                }
            }
            else if (gameOverState == GameOverState::ShowingDisagreement) {
                DrawTextEx(font, "CHOICES DIFFER", {220, 300}, 40, 2, RED);
                DrawTextEx(font, "Returning to title in...", {180, 360}, 28, 2, YELLOW);

                gameOverTimer += GetFrameTime();
                int countdown = (int)(DISAGREEMENT_COUNTDOWN - gameOverTimer) + 1;
                if (countdown < 0) countdown = 0;
                DrawTextEx(font, TextFormat("%d", countdown), {350, 410}, 60, 2, WHITE);

                if (gameOverTimer >= DISAGREEMENT_COUNTDOWN) {
                    std::cout << "[GAME] Countdown finished, going to title" << std::endl;
                    gameOverState = GameOverState::GoingToTitle;
                }
            }
            else if (gameOverState == GameOverState::RestartingGame) {
                // 새 게임 시작 - 호스트가 새 시드 생성 및 전송
                if (session.params().role == net::Role::Host) {
                    sessionSeed = ((uint64_t)GetTime() * 1000000.0) + rand();
                    session.SendNewSeed(sessionSeed);
                    std::cout << "[GAME] Host sending new seed: 0x" << std::hex << sessionSeed << std::dec << std::endl;
                } else {
                    // 클라이언트는 호스트로부터 새 SEED 메시지를 받을 때까지 대기
                    // SEED 메시지 수신 시 seedParams가 자동으로 업데이트됨 (session.cpp handleFrame에서 처리)
                    sessionSeed = session.params().seed;
                    std::cout << "[GAME] Client using received seed: 0x" << std::hex << sessionSeed << std::dec << std::endl;
                }

                // 게임 재시작
                gameLocal = std::make_unique<Game>(sessionSeed);
                gameRemote = std::make_unique<Game>(sessionSeed);
                localInputs.clear();
                localTickNext = 0;
                simTick = 0;
                startDelay = session.params().start_tick;

                session.ClearGameOverChoices();
                gameOverState = GameOverState::None;
                std::cout << "[GAME] Game restarted successfully" << std::endl;
            }
            else if (gameOverState == GameOverState::GoingToTitle) {
                // 타이틀로 복귀 (연결 유지 또는 종료)
                std::cout << "[GAME] Returning to title menu" << std::endl;

                // 게임 정리
                gameLocal.reset();
                gameRemote.reset();
                localInputs.clear();
                localTickNext = 0;
                simTick = 0;

                // 연결 유지 여부 결정 (현재는 유지)
                // session.Close();  // 연결 종료하려면 주석 해제

                session.ClearGameOverChoices();
                gameOverState = GameOverState::None;

                app = AppMode::Menu;
            }
            if (!session.isConnected()) {
                if (session.isListening()) DrawTextEx(font, TextFormat("Hosting on port %u...", (unsigned)hostPort), {180, 530}, 22, 2, WHITE);
                DrawTextEx(font, "Waiting for connection...", {180, 560}, 22, 2, WHITE);
            } else if (!session.isReady()) {
                DrawTextEx(font, "Waiting for session ready...", {170, 560}, 22, 2, WHITE);
            }
        }
        // [NET] 보드 초기화 전 대기 화면
        if (app == AppMode::Net && (!gameLocal || !gameRemote)) {
            ClearBackground(darkBlue);
            if (isHost) {
                // [FIX] IP 주소를 한 번만 조회 (성능 개선)
                if (!ipCached) {
                    cachedLocalIP = net::get_local_ip();
                    cachedPublicIP = net::get_public_ip();
                    ipCached = true;
                }

                if (session.isListening()) {
                    DrawTextEx(font, TextFormat("Hosting on port %u", (unsigned)hostPort), {180, 120}, 28, 2, WHITE);

                    // 로컬 IP (같은 WiFi용)
                    DrawTextEx(font, "Same WiFi:", {50, 160}, 20, 2, GRAY);
                    DrawTextEx(font, TextFormat("%s:%u", cachedLocalIP.c_str(), (unsigned)hostPort), {50, 180}, 18, 2, YELLOW);

                    // 공인 IP (인터넷용)
                    if (!cachedPublicIP.empty()) {
                        DrawTextEx(font, "Internet:", {350, 160}, 20, 2, GRAY);
                        DrawTextEx(font, TextFormat("%s:%u", cachedPublicIP.c_str(), (unsigned)hostPort), {350, 180}, 18, 2, GREEN);
                        DrawTextEx(font, "(Need port forwarding)", {300, 200}, 16, 2, GRAY);
                    } else {
                        DrawTextEx(font, "Internet: Getting IP...", {350, 160}, 16, 2, GRAY);
                        DrawTextEx(font, "This may take a few seconds", {300, 180}, 14, 2, GRAY);
                    }
                }
                DrawTextEx(font, "Waiting for connection...", {250, 250}, 24, 2, WHITE);
            } else {
                if (session.hasFailed()) {
                    DrawTextEx(font, "Connection Failed!", {200, 200}, 28, 2, RED);
                    DrawTextEx(font, "Check IP address and port", {160, 240}, 20, 2, GRAY);
                    DrawTextEx(font, "Press ESC to go back", {200, 280}, 20, 2, WHITE);
                } else {
                    DrawTextEx(font, "Connecting...", {240, 220}, 28, 2, WHITE);
                }
            }
            DrawTextEx(font, "Press ESC to cancel", {220, 350}, 20, 2, GRAY);
            if (IsKeyPressed(KEY_ESCAPE)) { session.Close(); app = AppMode::Menu; }
        }

        // [NET] 상태 표시(HUD 최소): 연결 여부/시드/틱 등
        if (app == AppMode::Net) {
            DrawText(TextFormat("NET: %s", session.isConnected()?"CONNECTED":"DISCONNECTED"), 10, 580, 10, RAYWHITE);
            if (session.isReady()) {
                DrawText(TextFormat("SEED: 0x%08x", (unsigned)(sessionSeed & 0xFFFFFFFFu)), 10, 594, 10, RAYWHITE);
                DrawText(TextFormat("TICKS localSent=%u remoteMax=%u sim=%u delay=%u", (unsigned)((localTickNext==0)?0:localTickNext-1), (unsigned)session.maxRemoteTick(), (unsigned)simTick, (unsigned)inputDelay), 10, 606, 10, RAYWHITE);
            }
        }
        EndDrawing();           // End drawing canvas
    }

    CloseWindow();
    net::net_shutdown();
    return 0;
}
