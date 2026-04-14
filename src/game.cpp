// src/game.cpp — raylib 래퍼 → Handmade 렌더러 래퍼
//
// 학습 포인트:
//   이 파일이 하는 일 = 원래 raylib::DrawRectangle 을 직접 구현한 것.
//   draw_rect(x, y, w, h, color) → renderer.cpp → OpenGL VBO → GPU

#include "game.h"
#include "colors.h"
#include "../renderer/renderer.h"

Game::Game(uint64_t seed)
    : sim(seed),
      gameOver(sim.gameOver),
      score(sim.score)
{
    cellColors = GetCellColors();

    // 오디오 초기화 (참조 카운팅 -- 멀티플레이에서 두 번 호출해도 안전)
    if (audio_init())
    {
        sndRotate = audio_load_sound("Sounds/rotate.mp3");
        sndClear  = audio_load_sound("Sounds/clear.mp3");
        sndMusic  = audio_load_sound("Sounds/music.mp3");
        audio_play_music(sndMusic);
    }
}

Game::~Game()
{
    audio_stop_music();
    audio_unload_sound(sndRotate);
    audio_unload_sound(sndClear);
    audio_unload_sound(sndMusic);
    audio_shutdown();  // 참조 카운팅: 마지막 Game 소멸 시만 실제 해제
}

void Game::SubmitInput(uint8_t inputMask)
{
    sim.SubmitInput(inputMask);
    if (sim.rotateSoundEvent)
    {
        audio_play_sound(sndRotate);
        sim.rotateSoundEvent = false;
    }
}

void Game::Tick()
{
    sim.Tick();
    if (sim.clearSoundEvent)
    {
        audio_play_sound(sndClear);
        sim.clearSoundEvent = false;
    }
}

void Game::MoveBlockDown()
{
    sim.MoveBlockDown();
}

unsigned long long Game::ComputeStateHash() const
{
    return static_cast<unsigned long long>(sim.StateHash());
}

// ─── 내부 렌더링 ──────────────────────────────────────────────────────────────

void Game::DrawGrid(int offsetX, int offsetY) const
{
    constexpr int cellSize = 30;
    const auto& g = sim.Grid();
    for (int row = 0; row < SimGrid::kRows; row++)
    {
        for (int col = 0; col < SimGrid::kCols; col++)
        {
            int v = g[row][col];
            draw_rect(
                col * cellSize + offsetX,
                row * cellSize + offsetY,
                cellSize - 1, cellSize - 1,
                cellColors[v]);
        }
    }
}

void Game::DrawBlock(const SimBlock& block, int offsetX, int offsetY) const
{
    constexpr int cellSize = 30;
    std::vector<Position> tiles = block.GetCellPositions();
    for (const Position& p : tiles)
    {
        draw_rect(
            p.column * cellSize + offsetX,
            p.row    * cellSize + offsetY,
            cellSize - 1, cellSize - 1,
            cellColors[block.id]);
    }
}

void Game::Draw()
{
    DrawGrid(11, 11);
    DrawBlock(sim.GhostBlock(),   11, 11);
    DrawBlock(sim.CurrentBlock(), 11, 11);

    const SimBlock& next = sim.NextBlock();
    switch (next.id)
    {
    case 3: DrawBlock(next, 255, 260); break;  // I
    case 4: DrawBlock(next, 255, 280); break;  // O
    default: DrawBlock(next, 270, 270); break;
    }
}

void Game::DrawBoardAt(int offsetX, int offsetY)
{
    DrawGrid(offsetX, offsetY);
    DrawBlock(sim.GhostBlock(),   offsetX, offsetY);
    DrawBlock(sim.CurrentBlock(), offsetX, offsetY);
}

void Game::DrawNextAt(int offsetX, int offsetY)
{
    const SimBlock& next = sim.NextBlock();
    switch (next.id)
    {
    case 3: DrawBlock(next, offsetX - 15, offsetY - 10); break;
    case 4: DrawBlock(next, offsetX - 15, offsetY + 10); break;
    default: DrawBlock(next, offsetX, offsetY); break;
    }
}
