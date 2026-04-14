#pragma once
#include <vector>
#include "sim_game.h"
#include "../core/rng.h"
#include "../core/input.h"
#include "../core/constants.h"
#include "../platform/platform.h"  // Color (raylib 대체)
#include "../audio/audio.h"

// [NET] Handmade 렌더러 래퍼 — SimGame 위에 draw_rect() 기반 렌더링 + XAudio2 오디오.
// 렌더링은 renderer/renderer.h 의 draw_rect() 를 사용.
// 오디오는 audio/audio.h 의 XAudio2 래퍼를 사용.
class Game
{
public:
    Game(uint64_t seed = 0);
    ~Game();

    // ── 렌더링 ──────────────────────────────────────────────────────────────
    void Draw();
    void DrawBoardAt(int offsetX, int offsetY);
    void DrawNextAt(int offsetX, int offsetY);

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
    void DrawGrid(int offsetX, int offsetY) const;
    void DrawBlock(const SimBlock& block, int offsetX, int offsetY) const;

    std::vector<Color> cellColors;

    // ── 오디오 핸들 (XAudio2) ───────────────────────────────────────────────
    AudioHandle sndRotate = 0;
    AudioHandle sndClear  = 0;
    AudioHandle sndMusic  = 0;
};
