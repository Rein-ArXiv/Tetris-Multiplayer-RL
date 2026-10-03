#pragma once
#include <vector>
#include "sim_game.h"
#include "../core/rng.h"
#include "../core/input.h"
#include "../core/constants.h"
#include "../platform/platform.h"  // Color
#include "../audio/audio.h"

// 고스트 피스 표시 on/off (렌더 전용 — 설정 화면이 구동). 기본 on.
// main.cpp 의 GameSettings.ghostOn 변경 시 호출해 Draw 의 고스트 그리기를 게이트.
void game_set_ghost_enabled(bool on);

// [NET] Handmade 렌더러 래퍼 — SimGame 위에 draw_rect() 기반 렌더링과 선택된 오디오 백엔드.
// 렌더링은 renderer/renderer.h 의 draw_rect() 를 사용.
// 오디오는 audio/audio.h의 공통 API를 사용하며 빌드가 SDL/XAudio2 구현을 선택한다.
class Game
{
public:
    Game(uint64_t seed = 0);
    ~Game();

    // Owns audio handles and reference aliases into sim. Copying would alias
    // another object's state and release its handles; moving requires rebinding.
    // Keep object identity stable; transfer ownership through unique_ptr<Game>.
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    // ── 렌더링 ──────────────────────────────────────────────────────────────
    void Draw();
    void DrawBoardAt(int offsetX, int offsetY, int cellSize = 30);
    void DrawNextAt(int offsetX, int offsetY);
    // 축소 프리뷰 — 멀티/봇 모드용 (cellSize 작게). 보드 사이 좁은 갭에 들어감.
    void DrawNextMini(int offsetX, int offsetY, int cellSize);
    void DrawNextQueueMini(int offsetX, int offsetY, int cellSize,
                           int maxCount = SimGame::kNextPreviewCount,
                           int ySpacing = 48);
    // 가비지 큐 미리보기 바 — 보드 왼쪽(offsetX-8 위치)에 빨간 바 세로 그리기.
    // pending: 주입 대기 중인 행 수. 최대 표시 12행.
    static void DrawGarbageBar(int boardX, int boardY, int pending, int cellSize = 30);

    // ── 시뮬레이션 위임 ─────────────────────────────────────────────────────
    void SubmitInput(uint8_t inputMask);
    void Tick();
    void MoveBlockDown();

    // ── 해시 (결정론 검증) ──────────────────────────────────────────────────
    unsigned long long ComputeStateHash() const;

    // main.cpp 가 직접 읽는 SimGame 핸들
    SimGame sim;

    // SimGame 상태를 직접 참조하는 별칭 (하위 호환)
    bool& gameOver;
    int&  score;

private:
    void ConsumeSoundEvents();
    void DrawGrid(int offsetX, int offsetY, int cellSize = 30) const;
    void DrawBlock(const SimBlock& block, int offsetX, int offsetY, int cellSize = 30) const;
    void DrawBlockMini(const SimBlock& block, int offsetX, int offsetY, int cellSize) const;

    std::vector<Color> cellColors;

    // ── 오디오 핸들 (선택된 백엔드) ───────────────────────────────────────────────
    AudioHandle sndRotate  = 0;
    AudioHandle sndClear   = 0;
    AudioHandle sndDrop    = 0;
    AudioHandle sndGarbage = 0;
    bool audioInitCalled = false;
    bool musicUser = false;
};
