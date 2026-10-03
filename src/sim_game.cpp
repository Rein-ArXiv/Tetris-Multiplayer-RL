#include "sim_game.h"
#include "../core/hash.h"
#include <algorithm>
#include <cstdint>
#include <limits>

// [NET/RL] This file is the single source of truth for game logic.
// Ported line-for-line from src/game.cpp to preserve deterministic state hashes.
// Do NOT add rendering/audio/platform deps here — those belong in the Game wrapper.

SimGame::SimGame(uint64_t seed)
    : gameOver(false),
      score(0),
      rng(seed ? seed : 0xC0FFEE123456789ull),
      // Separate owned stream: this XOR fork does not guarantee statistical independence.
      garbageRng((seed ? seed : 0xC0FFEE123456789ull) ^ 0x9E3779B97F4A7C15ull),
      gravityCounterTicks(0),
      dropIntervalTicks(TICKS_PER_SECOND / 2), // default: drop every 0.5s
      // 이 생성자의 초기 상태를 명시한다. 같은 멤버의 기본 초기화 식은
      // 여기서 다시 실행되지 않는다. 두 방식 중 하나로 값을 정의하면 된다.
      softDropCounterTicks(0),
      lastMoveWasRotate(false),
      attackLinesSent(0),
      pendingGarbage(0)
{
    blocks = GetAllBlocks();
    currentBlock = GetRandomBlock();
    nextBlocks.reserve(kNextPreviewCount);
    for (int i = 0; i < kNextPreviewCount; ++i)
    {
        nextBlocks.push_back(GetRandomBlock());
    }
    ghostBlock = MakeGhostBlock(currentBlock);
    // sim_grid is zero-initialized by its default constructor.
    DropExpectation();
}

SimBlock SimGame::GetRandomBlock()
{
    // Refill the piece bag on demand; each draw consumes this piece RNG once,
    // including a one-item bag. Stable erase order is part of seeded replay.
    // Garbage uses its separately owned garbageRng stream.
    if (blocks.empty())
    {
        blocks = GetAllBlocks();
    }
    int randomIndex = rng.nextUInt(static_cast<uint32_t>(blocks.size()));
    SimBlock block = blocks[randomIndex];
    blocks.erase(blocks.begin() + randomIndex);
    return block;
}

std::vector<SimBlock> SimGame::GetAllBlocks() const
{
    // Order MUST match original Game::GetAllBlocks exactly: I,J,L,O,S,T,Z.
    // The order determines which id is at which vector index, and the RNG
    // selects by index — changing order breaks state hash parity.
    return {SimIBlock(), SimJBlock(), SimLBlock(), SimOBlock(), SimSBlock(), SimTBlock(), SimZBlock()};
}

SimBlock SimGame::MakeGhostBlock(const SimBlock& block) const
{
    SimBlock ghost = block;
    ghost.id = 8;
    return ghost;
}

void SimGame::SubmitInput(uint8_t inputMask)
{
    if (gameOver) return;

    if (hasInput(inputMask, INPUT_LEFT))   MoveBlockLeft();
    if (hasInput(inputMask, INPUT_RIGHT))  MoveBlockRight();

    // DOWN이 처음 관찰된 호출은 즉시 시도하고, 이후 3호출을 건너뛴다.
    // 한 틱당 SubmitInput 1회라면 4틱 주기: 60Hz에서 초당 15회 시도.
    // 자연 중력은 Tick에서 별도로 더해진다. false가 관찰되면 다음 눌림을 재준비한다.
    // 이 카운터는 미래 전이에 영향을 주므로 상태 해시에 포함한다.
    constexpr int kSoftDropCooldownTicks = 3;
    if (hasInput(inputMask, INPUT_DOWN)) {
        if (softDropCounterTicks <= 0) {
            MoveBlockDown();
            softDropCounterTicks = kSoftDropCooldownTicks;
        } else {
            softDropCounterTicks--;
        }
    } else {
        softDropCounterTicks = 0;
    }

    if (hasInput(inputMask, INPUT_ROTATE)) RotateBlockImpl();
    if (hasInput(inputMask, INPUT_DROP))   MoveBlockDrop();

    DropExpectation();
}

void SimGame::Tick()
{
    if (gameOver) return;
    gravityCounterTicks++;
    if (gravityCounterTicks >= dropIntervalTicks)
    {
        gravityCounterTicks = 0;
        MoveBlockDown();
    }
}

void SimGame::MoveBlockLeft()
{
    if (gameOver) return;
    currentBlock.Move(0, -1);
    if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
    {
        currentBlock.Move(0, 1);
    }
    else
    {
        lastMoveWasRotate = false;
        ghostBlock = MakeGhostBlock(currentBlock);
    }
}

void SimGame::MoveBlockRight()
{
    if (gameOver) return;
    currentBlock.Move(0, 1);
    if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
    {
        currentBlock.Move(0, -1);
    }
    else
    {
        lastMoveWasRotate = false;
        ghostBlock = MakeGhostBlock(currentBlock);
    }
}

void SimGame::MoveBlockDown()
{
    if (gameOver) return;
    currentBlock.Move(1, 0);
    if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
    {
        currentBlock.Move(-1, 0);
        LockBlock();
    }
    else
    {
        lastMoveWasRotate = false;
    }
}

void SimGame::MoveBlockDrop()
{
    if (gameOver) return;
    while (IsBlockOutside(currentBlock) == false && BlockFits(currentBlock) == true)
    {
        currentBlock.Move(1, 0);
    }
    currentBlock.Move(-1, 0);
    dropSoundEvent = true;
    hardDropEvent  = true;   // 흔들림용 (렌더 전용, 해시 무관)
    LockBlock();
}

void SimGame::DropExpectation()
{
    if (gameOver) return;
    // Refresh from the current pose; a cached hint may describe an older board.
    ghostBlock = MakeGhostBlock(currentBlock);
    if (IsBlockOutside(ghostBlock) || !BlockFits(ghostBlock)) return;
    while (IsBlockOutside(ghostBlock) == false && BlockFits(ghostBlock) == true)
    {
        ghostBlock.Move(1, 0);
    }
    ghostBlock.Move(-1, 0);
}

bool SimGame::IsBlockOutside(const SimBlock& block) const
{
    std::vector<Position> tiles = block.GetCellPositions();
    for (const Position& item : tiles)
    {
        if (sim_grid.IsCellOutside(item.row, item.column))
        {
            return true;
        }
    }
    return false;
}

void SimGame::RotateBlockImpl()
{
    if (gameOver) return;
    currentBlock.Rotate();
    if (IsBlockOutside(currentBlock) == true || BlockFits(currentBlock) == false)
    {
        currentBlock.UndoRotation();
    }
    else
    {
        lastMoveWasRotate = true;
        rotateSoundEvent = true;
        ghostBlock = MakeGhostBlock(currentBlock);
    }
}

static int attack_lines_for(int rowsCleared, bool tSpin)
{
    if (tSpin)
    {
        switch (rowsCleared) {
            case 1: return 2;   // T-spin Single
            case 2: return 4;   // T-spin Double
            case 3: return 6;   // T-spin Triple
            default: return 0;  // T-spin no-line
        }
    }
    switch (rowsCleared) {
        case 2: return 1;   // Double → 1 가비지
        case 3: return 2;   // Triple → 2 가비지
        case 4: return 4;   // Tetris → 4 가비지
        default: return 0;  // Single or none
    }
}

bool SimGame::IsTSpinLock() const
{
    if (currentBlock.id != 6 || !lastMoveWasRotate) return false;

    const int pivotRow = currentBlock.rowOffset + 1;
    const int pivotCol = currentBlock.columnOffset + 1;
    const int corners[4][2] = {
        {pivotRow - 1, pivotCol - 1},
        {pivotRow - 1, pivotCol + 1},
        {pivotRow + 1, pivotCol - 1},
        {pivotRow + 1, pivotCol + 1},
    };

    int blocked = 0;
    for (const auto& corner : corners)
    {
        const int row = corner[0];
        const int col = corner[1];
        if (sim_grid.IsCellOutside(row, col) || !sim_grid.IsCellEmpty(row, col))
        {
            blocked++;
        }
    }
    return blocked >= 3;
}

void SimGame::LockBlock()
{
    const bool tSpin = IsTSpinLock();
    std::vector<Position> tiles = currentBlock.GetCellPositions();
    for (const Position& item : tiles)
    {
        sim_grid.grid[item.row][item.column] = currentBlock.id;
    }
    currentBlock = NextBlock();
    ghostBlock = MakeGhostBlock(currentBlock);
    bool wasGameOver = gameOver;
    if (BlockFits(currentBlock) == false)
    {
        gameOver = true;
    }

    nextBlocks.erase(nextBlocks.begin());
    nextBlocks.push_back(GetRandomBlock());
    int rowsCleared = sim_grid.ClearFullRows();
    lastLinesCleared = rowsCleared;
    lastTSpinLines = tSpin ? rowsCleared : -1;
    if (rowsCleared > 0 || tSpin)
    {
        if (rowsCleared > 0) clearSoundEvent = true;
        UpdateScore(rowsCleared, 0, tSpin);
        attackLinesSent = saturating_add_count(attackLinesSent, attack_lines_for(rowsCleared, tSpin));
    }
    lastMoveWasRotate = false;

    // 가비지 주입 — 라인 클리어 적용 후, 다음 피스가 확정된 이 시점에서 하단으로 올라온다.
    // 주의: 클리어 없이 그냥 놓은 경우에도 pendingGarbage 가 있으면 받는다.
    int inserted = 0;
    if (pendingGarbage > 0 && !gameOver)
    {
        inserted = std::min(pendingGarbage, SimGrid::kRows);
        InsertGarbage(inserted);
        pendingGarbage = 0;
        // 가비지가 올라와 currentBlock 스폰 위치를 막았으면 topout.
        if (!BlockFits(currentBlock)) gameOver = true;
    }
    lastGarbageReceived = inserted;
    if (inserted > 0) garbageSoundEvent = true;

    if (gameOver && !wasGameOver) gameOverEvent = true;
    // Also covers natural gravity and placement-level callers, without input.
    // Recompute after row removal and garbage insertion have settled the board.
    DropExpectation();
}

void SimGame::InsertGarbage(int rows)
{
    if (rows <= 0) return;
    if (rows > SimGrid::kRows) rows = SimGrid::kRows;

    // Detect occupied cells leaving the top before overwriting them. An empty
    // discarded row is not a defeat. Still publish the shifted/final board.
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < SimGrid::kCols; ++c)
            if (!sim_grid.IsCellEmpty(r, c)) gameOver = true;

    // Read higher-index source rows before a later write can replace them.
    for (int r = 0; r + rows < SimGrid::kRows; r++)
    {
        for (int c = 0; c < SimGrid::kCols; c++)
        {
            sim_grid.grid[r][c] = sim_grid.grid[r + rows][c];
        }
    }
    // 하단 rows 행은 가비지 (id=9, 홀 1개). 한 공격 묶음은 동일 홀 컬럼 공유.
    int hole = static_cast<int>(garbageRng.nextUInt(SimGrid::kCols));
    for (int i = 0; i < rows; i++)
    {
        int gr = SimGrid::kRows - 1 - i;
        for (int c = 0; c < SimGrid::kCols; c++)
        {
            sim_grid.grid[gr][c] = (c == hole) ? 0 : 9;
        }
    }
}

bool SimGame::BlockFits(const SimBlock& block) const
{
    std::vector<Position> tiles = block.GetCellPositions();
    for (const Position& item : tiles)
    {
        if (sim_grid.IsCellEmpty(item.row, item.column) == false)
        {
            return false;
        }
    }
    return true;
}

void SimGame::UpdateScore(int linesCleared, int levelUp, bool tSpin)
{
    // Award using the level before this clear; the table is project-specific.
    int basePoints = 0;
    if (tSpin)
    {
        switch (linesCleared)
        {
        case 0: basePoints = 400; break;
        case 1: basePoints = 800; break;
        case 2: basePoints = 1200; break;
        case 3: basePoints = 1600; break;
        default: break;
        }
    }
    else
    {
        switch (linesCleared)
        {
        case 1: basePoints = 100; break;
        case 2: basePoints = 300; break;
        case 3: basePoints = 600; break;
        case 4: basePoints = 1000; break;
        default: break;
        }
    }
    // Widen BEFORE arithmetic. Preserve the int API/hash representation and
    // saturate at its limit rather than invoking signed-overflow UB.
    const std::int64_t gained = std::int64_t{basePoints} * level +
                                std::int64_t{levelUp} * 1000;
    const auto limit = std::int64_t{std::numeric_limits<int>::max()};
    score = static_cast<int>(std::clamp(std::int64_t{score} + gained,
                                        std::int64_t{0}, limit));

    // 레벨 시스템: 10라인마다 레벨업 + 중력 증가.
    totalLinesCleared = static_cast<int>(std::clamp(
        std::int64_t{totalLinesCleared} + linesCleared, std::int64_t{0}, limit));
    int newLevel = totalLinesCleared / 10 + 1;
    if (newLevel > level) {
        level = (newLevel > 20) ? 20 : newLevel;
        // 레벨별 중력: 1→30틱, 5→25틱, 10→18틱, 15→11틱, 20→3틱.
        // TICKS_PER_SECOND=60 기준으로 30에서 3까지 정수 선형 보간한다.
        int newInterval = 30 - (level - 1) * 27 / 19;  // 레벨1=30, 레벨20=3
        if (newInterval < 3) newInterval = 3;
        dropIntervalTicks = newInterval;
    }
}

SimGame::HashBreakdown SimGame::StateHashBreakdown() const
{
    HashBreakdown b{};
    constexpr uint64_t BASE = 14695981039346656037ull;

    // Grid
    b.grid = BASE;
    for (const auto& row : sim_grid.grid)
        for (int cell : row) b.grid = fnv1a64_value(int32_t{cell}, b.grid);

    // Current block
    uint64_t cb = BASE;
    cb = fnv1a64_value(currentBlock.id, cb);
    cb = fnv1a64_value(currentBlock.GetRotationState(), cb);
    cb = fnv1a64_value(currentBlock.GetRowOffset(), cb);
    cb = fnv1a64_value(currentBlock.GetColumnOffset(), cb);
    b.currentBlock = cb;

    // Next preview queue
    uint64_t nb = BASE;
    nb = fnv1a64_value(static_cast<int>(nextBlocks.size()), nb);
    for (const SimBlock& next : nextBlocks)
    {
        nb = fnv1a64_value(next.id, nb);
        nb = fnv1a64_value(next.GetRotationState(), nb);
        nb = fnv1a64_value(next.GetRowOffset(), nb);
        nb = fnv1a64_value(next.GetColumnOffset(), nb);
    }
    b.nextBlock = nb;

    // RNG
    b.rng = fnv1a64_value(rng.getState(), BASE);

    // Score / flags / gravity / level
    uint64_t sf = BASE;
    sf = fnv1a64_value(score, sf);
    sf = fnv1a64_value(gameOver ? 1 : 0, sf);
    sf = fnv1a64_value(gravityCounterTicks, sf);
    sf = fnv1a64_value(dropIntervalTicks, sf);
    sf = fnv1a64_value(softDropCounterTicks, sf);
    sf = fnv1a64_value(totalLinesCleared, sf);
    sf = fnv1a64_value(level, sf);
    sf = fnv1a64_value(lastMoveWasRotate ? 1 : 0, sf);
    b.scoreFlags = sf;

    // Combat
    uint64_t co = BASE;
    co = fnv1a64_value(garbageRng.getState(), co);
    co = fnv1a64_value(attackLinesSent, co);
    co = fnv1a64_value(pendingGarbage, co);
    b.combat = co;

    return b;
}

uint64_t SimGame::StateHash() const { return ComputeStateHash(false); }

uint64_t SimGame::DiagnosticStateHashV2() const { return ComputeStateHash(true); }

uint64_t SimGame::ComputeStateHash(bool includeBag) const
{
    uint64_t h = 14695981039346656037ull;
    if (includeBag) {
        h = fnv1a64("SIMH", 4, h);
        h = fnv1a64_value(uint32_t{2}, h);
    }
    // Row-major signed 32-bit values encoded LE, independent of host endian.
    for (const auto& row : sim_grid.grid)
        for (int cell : row) h = fnv1a64_value(int32_t{cell}, h);
    // Current block state
    h = fnv1a64_value(currentBlock.id, h);
    int curRot = currentBlock.GetRotationState();
    int curRow = currentBlock.GetRowOffset();
    int curCol = currentBlock.GetColumnOffset();
    h = fnv1a64_value(curRot, h);
    h = fnv1a64_value(curRow, h);
    h = fnv1a64_value(curCol, h);
    // Next preview queue state
    h = fnv1a64_value(static_cast<int>(nextBlocks.size()), h);
    for (const SimBlock& next : nextBlocks)
    {
        h = fnv1a64_value(next.id, h);
        h = fnv1a64_value(next.GetRotationState(), h);
        h = fnv1a64_value(next.GetRowOffset(), h);
        h = fnv1a64_value(next.GetColumnOffset(), h);
    }
    // RNG / score / flags / gravity
    uint64_t rngState = rng.getState();
    h = fnv1a64_value(rngState, h);
    h = fnv1a64_value(score, h);
    int over = gameOver ? 1 : 0;
    h = fnv1a64_value(over, h);
    h = fnv1a64_value(gravityCounterTicks, h);
    h = fnv1a64_value(dropIntervalTicks, h);
    h = fnv1a64_value(softDropCounterTicks, h);
    h = fnv1a64_value(totalLinesCleared, h);
    h = fnv1a64_value(level, h);
    h = fnv1a64_value(lastMoveWasRotate ? 1 : 0, h);
    // Compare at matching simulation boundaries; mismatches are diagnostic evidence.
    uint64_t gRng = garbageRng.getState();
    h = fnv1a64_value(gRng, h);
    h = fnv1a64_value(attackLinesSent, h);
    h = fnv1a64_value(pendingGarbage, h);
    if (includeBag) {
        h = fnv1a64_value(static_cast<uint32_t>(blocks.size()), h);
        for (const SimBlock& block : blocks) {
            h = fnv1a64_value(block.id, h);
            h = fnv1a64_value(block.GetRotationState(), h);
            h = fnv1a64_value(block.GetRowOffset(), h);
            h = fnv1a64_value(block.GetColumnOffset(), h);
        }
    }
    return h;
}

// ============================================================================
// Placement-level API (for RL training — not exercised by the lockstep game).
// ============================================================================

std::vector<SimGame::Placement> SimGame::LegalPlacements() const
{
    std::vector<Placement> out;
    if (gameOver) return out;

    const int numRotations = static_cast<int>(currentBlock.cells.size());
    for (int rot = 0; rot < numRotations; rot++)
    {
        for (int col = 0; col < SimGrid::kCols; col++)
        {
            // Start from a fresh copy of the live piece.
            SimBlock test = currentBlock;
            // Rotate in place to the target rotation.
            while (test.rotationState != rot)
            {
                test.Rotate();
            }
            // Slide horizontally to the target column offset.
            int delta = col - test.columnOffset;
            test.columnOffset += delta;
            // Reject if the rotated & translated piece is invalid at spawn height.
            if (IsBlockOutside(test) || !BlockFits(test)) continue;
            // Hard drop simulation.
            while (IsBlockOutside(test) == false && BlockFits(test) == true)
            {
                test.rowOffset++;
            }
            test.rowOffset--;
            if (IsBlockOutside(test) || !BlockFits(test)) continue;
            out.push_back({col, rot});
        }
    }
    return out;
}

int SimGame::ApplyPlacement(int col, int rot)
{
    if (gameOver) return -1;

    // Build target configuration from the live currentBlock.
    SimBlock target = currentBlock;
    const int numRotations = static_cast<int>(target.cells.size());
    if (rot < 0 || rot >= numRotations) return -1;
    while (target.rotationState != rot)
    {
        target.Rotate();
    }
    // Validate the external origin before GetCellPositions adds int offsets.
    // Avoid both col-oldOrigin subtraction overflow and local+col overflow.
    for (const Position& cell : target.cells.at(target.rotationState))
    {
        const std::int64_t column = std::int64_t{cell.column} + col;
        if (column < 0 || column >= SimGrid::kCols) return -1;
    }
    target.columnOffset = col;
    if (IsBlockOutside(target) || !BlockFits(target)) return -1;
    // Hard drop
    while (IsBlockOutside(target) == false && BlockFits(target) == true)
    {
        target.rowOffset++;
    }
    target.rowOffset--;
    if (IsBlockOutside(target) || !BlockFits(target)) return -1;

    // Commit: overwrite currentBlock with the landed configuration and lock.
    currentBlock = target;
    lastMoveWasRotate = false;
    LockBlock();

    // Use the actual lock result: a saturated total cannot recover this delta.
    return lastLinesCleared;
}
