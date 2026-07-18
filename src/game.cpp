// src/game.cpp — raylib 래퍼 → Handmade 렌더러 래퍼
//
// 학습 포인트:
//   이 파일이 하는 일 = 원래 raylib::DrawRectangle 을 직접 구현한 것.
//   draw_rect(x, y, w, h, color) → renderer.cpp → OpenGL VBO → GPU

#include "game.h"
#include "colors.h"
#include "../renderer/renderer.h"
#include <climits>   // INT_MAX

namespace {
AudioHandle sharedMusic = 0;
int sharedMusicUsers = 0;
bool g_ghostEnabled = true;   // 고스트 피스 표시 (설정 화면이 구동)
}

void game_set_ghost_enabled(bool on) { g_ghostEnabled = on; }

Game::Game(uint64_t seed)
    : sim(seed),
      gameOver(sim.gameOver),
      score(sim.score)
{
    cellColors = GetCellColors();

    // 오디오 초기화 (참조 카운팅 -- 멀티플레이에서 두 번 호출해도 안전)
    audioInitCalled = true;
    if (audio_init())
    {
        sndRotate  = audio_load_sound("Sounds/rotate.mp3");
        sndClear   = audio_load_sound("Sounds/clear.mp3");
        sndDrop    = audio_load_sound("Sounds/drop.mp3");
        sndGarbage = audio_load_sound("Sounds/garbage.mp3");
        if (sharedMusic == 0) {
            sharedMusic = audio_load_sound("Sounds/music.mp3");
        }
        if (sharedMusic != 0) {
            ++sharedMusicUsers;
            musicUser = true;
            audio_play_music(sharedMusic);
        }
    }
}

Game::~Game()
{
    if (musicUser) {
        if (sharedMusicUsers > 0) --sharedMusicUsers;
        if (sharedMusicUsers == 0 && sharedMusic != 0) {
            audio_stop_music();
            audio_unload_sound(sharedMusic);
            sharedMusic = 0;
        }
        musicUser = false;
    }
    audio_unload_sound(sndRotate);
    audio_unload_sound(sndClear);
    audio_unload_sound(sndDrop);
    audio_unload_sound(sndGarbage);
    if (audioInitCalled) {
        audio_shutdown();  // 참조 카운팅: 마지막 Game 소멸 시만 실제 해제
        audioInitCalled = false;
    }
}

void Game::SubmitInput(uint8_t inputMask)
{
    sim.SubmitInput(inputMask);
    if (sim.rotateSoundEvent)  { audio_play_sound(sndRotate);  sim.rotateSoundEvent  = false; }
    // drop 전용 에셋(Sounds/drop.mp3)이 없으면 무음 대신 rotate 로 대체해
    // 피드백을 유지한다 (audio_play_sound(0) 은 no-op). 핸들 alias 가 아니라
    // 재생 시점 fallback 이므로 소멸자의 이중 unload 가 없다.
    if (sim.dropSoundEvent)    { audio_play_sound(sndDrop ? sndDrop : sndRotate); sim.dropSoundEvent = false; }
}

void Game::Tick()
{
    sim.Tick();
    if (sim.clearSoundEvent)   { audio_play_sound(sndClear);   sim.clearSoundEvent   = false; }
    // garbage 전용 에셋이 없으면 clear 로 대체 (위 drop 과 동일 이유).
    if (sim.garbageSoundEvent) { audio_play_sound(sndGarbage ? sndGarbage : sndClear); sim.garbageSoundEvent = false; }
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
    if (g_ghostEnabled) DrawBlock(sim.GhostBlock(), 11, 11);
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
    constexpr int cellSize = 30;
    constexpr int bw = SimGrid::kCols * cellSize;
    constexpr int bh = SimGrid::kRows * cellSize;
    // 보드 테두리 → 배경 순으로 그려서 1px 테두리 효과
    draw_rect(offsetX - 2, offsetY - 2, bw + 4, bh + 4, {55, 62, 100, 255});
    draw_rect(offsetX,     offsetY,     bw,     bh,     {14, 16, 30, 255});
    DrawGrid(offsetX, offsetY);
    if (g_ghostEnabled) DrawBlock(sim.GhostBlock(), offsetX, offsetY);
    DrawBlock(sim.CurrentBlock(), offsetX, offsetY);
}

void Game::DrawGarbageBar(int boardX, int boardY, int pending)
{
    if (pending <= 0) return;
    constexpr int cellSize = 30;
    constexpr int barW = 5;
    constexpr int boardH = SimGrid::kRows * cellSize;  // 600px
    constexpr int maxRows = 12;

    int rows = (pending > maxRows) ? maxRows : pending;
    int barH = rows * cellSize;

    // 배경 트랙 (어두운 바)
    draw_rect(boardX - barW - 2, boardY, barW, boardH, {40, 10, 10, 180});
    // 채워진 부분 (아래서 위로 — 가비지는 하단에서 올라옴)
    draw_rect(boardX - barW - 2, boardY + boardH - barH, barW, barH, {220, 40, 40, 220});
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

void Game::DrawNextMini(int offsetX, int offsetY, int cellSize)
{
    DrawBlockMini(sim.NextBlock(), offsetX, offsetY, cellSize);
}

void Game::DrawNextQueueMini(int offsetX, int offsetY, int cellSize,
                             int maxCount, int ySpacing)
{
    const auto& queue = sim.NextBlocks();
    int count = static_cast<int>(queue.size());
    if (count > maxCount) count = maxCount;
    for (int i = 0; i < count; ++i)
    {
        DrawBlockMini(queue[i], offsetX, offsetY + i * ySpacing, cellSize);
    }
}

void Game::DrawBlockMini(const SimBlock& block, int offsetX, int offsetY, int cellSize) const
{
    // SimBlock::GetCellPositions 는 블록의 로컬 좌표(0-based bounding box)를 반환.
    // cellSize 를 파라미터로 받아 축소 그리기. DrawBlock 과 달리 색상 팔레트를
    // 직접 인덱싱하고 전체 크기를 조절한다.
    std::vector<Position> tiles = block.GetCellPositions();
    // 블록을 bounding box 기준으로 정규화 — next 의 row/column 가 스폰 위치 기준이라
    // 그대로 그리면 오른쪽 하단으로 치우침. min row/col 를 빼서 (0,0) 에서 시작하게.
    int minRow = INT_MAX, minCol = INT_MAX;
    for (const auto& p : tiles) {
        if (p.row    < minRow) minRow = p.row;
        if (p.column < minCol) minCol = p.column;
    }
    for (const auto& p : tiles)
    {
        draw_rect(
            (p.column - minCol) * cellSize + offsetX,
            (p.row    - minRow) * cellSize + offsetY,
            cellSize - 1, cellSize - 1,
            cellColors[block.id]);
    }
}
