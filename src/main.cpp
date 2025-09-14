#include <raylib.h>
#include <iostream>
#include <cstdio>
#include "../core/constants.h"
#include "../core/input.h"
#include "../core/replay.h"
#include "../net/session.h"
#include <string>
#include <vector>
#include <filesystem>
#include "game.h"
#include "colors.h"

// [NET] 현재는 로컬 키보드 입력을 샘플링합니다.
// Lockstep/서버 권위로 확장 시, 네트워크 스레드가 수신한 '틱별 입력'을 큐에 넣고
// 여기서는 해당 틱의 입력을 큐에서 꺼내 Game::SubmitInput에 전달합니다.
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

int main(int argc, char** argv)
{
    InitWindow(500, 620, "Tetris");
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
    uint32_t startDelay = 120; uint8_t inputDelay = 4;
    if (netMode) {
        if (isHost) {
            // 임의 시드 생성(학습용): std::random_device는 환경 의존 가능
            sessionSeed = ((uint64_t)GetTime()*1000000.0) + 0xC0FFEEULL;
            net::SeedParams sp{sessionSeed, startDelay, inputDelay, net::Role::Host};
            if (!session.Host(hostPort, sp)) { TraceLog(LOG_ERROR, "Host failed"); }
        } else {
            if (!session.Connect(hostIp, hostPort)) { TraceLog(LOG_ERROR, "Connect failed"); }
        }
    }

    Game game = Game(sessionSeed);

    // [NET] 리플레이 기록(간단). F5로 시작, F6로 저장 종료.
    bool recording = false;
    ReplayData replay;
    replay.seed = sessionSeed;
    replay.frames.clear();

    float accumulator = 0.0f;
    while(WindowShouldClose() == false)     // Press ESC or click X button
    {
        UpdateMusicStream(game.music);

        // Fixed-timestep simulation
        // [NET] Lockstep: 입력을 해당 틱에 모두 수신했다면 그 틱을 한 번만 진행합니다.
        // 본 로컬 버전에선 네트워크 큐 대신 로컬 키 입력을 즉시 적용합니다.
        accumulator += GetFrameTime();
        while (accumulator >= SECONDS_PER_TICK)
        {
            uint8_t inputMask = SampleInput();
            if (inputMask == INPUT_NONE && IsKeyPressed(KEY_ENTER) && game.gameOver)
            {
                // Restart on any key when game over
                // Handled in render branch below (legacy behavior)
            }
            else
            {
                if (netMode && session.isConnected() && session.isReady()) {
                    static uint32_t netTick = 0;
                    // 시작 전 대기(startDelay)
                    if (startDelay > 0) { startDelay--; accumulator -= SECONDS_PER_TICK; continue; }

                    // 로컬 입력 전송
                    session.SendInput(netTick, inputMask);

                    // 원격 입력 수신 여부 확인; 입력 지연을 고려해 충분히 뒤의 틱은 기다립니다.
                    uint8_t remoteMask = 0;
                    if (!session.GetRemoteInput(netTick, remoteMask)) {
                        // 아직 입력 미도착 → 틱 진행 보류
                        break;
                    }
                    // 병합 입력 적용
                    game.SubmitInput((uint8_t)(inputMask | remoteMask));
                    game.Tick();
                    netTick++;
                } else {
                    game.SubmitInput(inputMask);
                    game.Tick();
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
        DrawTextEx(font, "Score", {365, 15}, 38, 2, WHITE);
        DrawTextEx(font, "Next", {370, 175}, 38, 2, WHITE);
        
        DrawRectangleRounded({320, 55, 170, 60}, 0.3, 6, lightBlue);
        
        char scoreText[10];
        sprintf(scoreText, "%d", game.score);
        Vector2 textSize = MeasureTextEx(font, scoreText, 38, 2);

        DrawTextEx(font, scoreText, {320 + (170 - textSize.x)/2, 65}, 38, 2, WHITE);
        DrawRectangleRounded({320, 215, 170, 180}, 0.3, 6, lightBlue);
        game.Draw();
        if (IsKeyPressed(KEY_F5)) { recording = true; replay.frames.clear(); }
        if (IsKeyPressed(KEY_F6)) {
            if (recording) {
                std::error_code ec; std::filesystem::create_directories("out", ec);
                ReplayIO::Save("out/replay.txt", replay);
                recording = false;
            }
        }

        if (IsKeyPressed(KEY_H)) {
            auto h = game.ComputeStateHash();
            std::cout << "Tick hash: 0x" << std::hex << h << std::dec << std::endl;
        }

        if (game.gameOver)
        {
            DrawTextEx(font, "GAME OVER", {120, 300}, 60, 2, WHITE);
            DrawTextEx(font, "Press Any key To Restart", {85, 350}, 25, 2, WHITE);
            // Restart if any key pressed
            if (GetKeyPressed() != 0)
            {
                game.gameOver = false;
                // Reinitialize the game state deterministically (same seed)
                // Alternatively, could reseed for variation
                // [NET] 온라인에선 서버/호스트가 정한 시드/시작틱을 다시 적용해야 합니다.
                game = Game(sessionSeed);
                if (recording) replay.frames.clear();
            }
        }

        // [NET] 상태 표시(HUD 최소): 연결 여부/시드/틱 등
        if (netMode) {
            DrawText(TextFormat("NET: %s", session.isConnected()?"CONNECTED":"DISCONNECTED"), 10, 580, 10, RAYWHITE);
            if (session.isReady()) {
                DrawText(TextFormat("SEED: 0x%08x", (unsigned)(sessionSeed & 0xFFFFFFFFu)), 10, 594, 10, RAYWHITE);
            }
        }
        EndDrawing();           // End drawing canvas
    }

    CloseWindow();
    return 0;
}
