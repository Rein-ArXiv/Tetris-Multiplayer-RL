#include "sim_game.h"
#include "../core/hash.h"

// [NET/RL] This file is the single source of truth for game logic.
// Ported line-for-line from src/game.cpp to preserve deterministic state hashes.
// Do NOT add raylib/audio/rendering here — those belong in the Game wrapper.

SimGame::SimGame(uint64_t seed)
    : gameOver(false),
      score(0),
      rng(seed ? seed : 0xC0FFEE123456789ull),
      // splitmix-style fork: 시드와 상호 상관관계가 약한 별도 스트림.
      garbageRng((seed ? seed : 0xC0FFEE123456789ull) ^ 0x9E3779B97F4A7C15ull),
      gravityCounterTicks(0),
      dropIntervalTicks(TICKS_PER_SECOND / 2), // default: drop every 0.5s
      // in-class initializer 에 의존하지 않고 명시 — StateHash 포함 필드들의
      // 결정론 보장을 위해 생성자 시점에 확정.
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
}

SimBlock SimGame::GetRandomBlock()
{
    // [NET] '가방'이 비면 새 가방을 채웁니다. RNG 호출 횟수가 틱/입력 흐름에 따라
    // 달라지지 않도록 주의 — 이 함수가 RNG의 유일한 호출 지점입니다.
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

    // 소프트 드롭: 매 틱 호출되면 60셀/초(너무 빠름). N틱마다 1회로 제한.
    //   최초 눌림(카운터=0) 은 즉시 반응, 그 다음부터 kSoftDropIntervalTicks
    //   (=3, 60Hz → 약 15셀/초) 간격. 뗐다가 다시 눌러도 즉시.
    //   결정론: 이 카운터는 상태 해시에 포함되므로 양쪽 클라이언트 동일 전개.
    constexpr int kSoftDropIntervalTicks = 3;
    if (hasInput(inputMask, INPUT_DOWN)) {
        if (softDropCounterTicks <= 0) {
            MoveBlockDown();
            softDropCounterTicks = kSoftDropIntervalTicks;
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
    LockBlock();
}

void SimGame::DropExpectation()
{
    if (gameOver) return;
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
        attackLinesSent += attack_lines_for(rowsCleared, tSpin);
    }
    lastMoveWasRotate = false;

    // 가비지 주입 — 라인 클리어 적용 후, 다음 피스가 확정된 이 시점에서 하단으로 올라온다.
    // 주의: 클리어 없이 그냥 놓은 경우에도 pendingGarbage 가 있으면 받는다.
    int inserted = 0;
    if (pendingGarbage > 0 && !gameOver)
    {
        inserted = pendingGarbage;
        InsertGarbage(pendingGarbage);
        pendingGarbage = 0;
        // 가비지가 올라와 currentBlock 스폰 위치를 막았으면 topout.
        if (!BlockFits(currentBlock)) gameOver = true;
    }
    lastGarbageReceived = inserted;
    if (inserted > 0) garbageSoundEvent = true;

    if (gameOver && !wasGameOver) gameOverEvent = true;
}

void SimGame::InsertGarbage(int rows)
{
    if (rows <= 0) return;
    if (rows > SimGrid::kRows) rows = SimGrid::kRows;

    // 기존 행을 위로 밀어올린다 — 상단 rows 만큼은 소실 (오버플로우는 게임오버 처리).
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
    // 점수: 레벨 배율 적용 (표준 NES 테트리스 방식).
    if (tSpin)
    {
        switch (linesCleared)
        {
        case 0: score += 400  * level; break;
        case 1: score += 800  * level; break;
        case 2: score += 1200 * level; break;
        case 3: score += 1600 * level; break;
        default: break;
        }
    }
    else
    {
        switch (linesCleared)
        {
        case 1: score += 100  * level; break;
        case 2: score += 300  * level; break;
        case 3: score += 600  * level; break;
        case 4: score += 1000 * level; break;
        default: break;
        }
    }
    score += levelUp * 1000;

    // 레벨 시스템: 10라인마다 레벨업 + 중력 증가.
    totalLinesCleared += linesCleared;
    int newLevel = totalLinesCleared / 10 + 1;
    if (newLevel > level) {
        level = (newLevel > 20) ? 20 : newLevel;
        // 레벨별 중력: 1→30틱, 5→20틱, 10→12틱, 15→7틱, 20→3틱
        // TICKS_PER_SECOND=60 기준. max(3, 30 - (level-1)*1.5)
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
    b.grid = fnv1a64(&sim_grid.grid[0][0], sizeof(sim_grid.grid), BASE);

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

uint64_t SimGame::StateHash() const
{
    uint64_t h = 14695981039346656037ull;
    // Grid bytes — layout must match old Grid::grid exactly.
    h = fnv1a64(&sim_grid.grid[0][0], sizeof(sim_grid.grid), h);
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
    // Combat state — 양쪽이 동일한 입력에서 동일한 값을 도출하므로 해시에 포함하면
    // 가비지 로직 버그가 HASH 자동 검증(F.2)에서 즉시 DESYNC 로 잡힌다.
    uint64_t gRng = garbageRng.getState();
    h = fnv1a64_value(gRng, h);
    h = fnv1a64_value(attackLinesSent, h);
    h = fnv1a64_value(pendingGarbage, h);
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
    int delta = col - target.columnOffset;
    target.columnOffset += delta;
    if (IsBlockOutside(target) || !BlockFits(target)) return -1;
    // Hard drop
    while (IsBlockOutside(target) == false && BlockFits(target) == true)
    {
        target.rowOffset++;
    }
    target.rowOffset--;
    if (IsBlockOutside(target) || !BlockFits(target)) return -1;

    // Snapshot cleared-line count before lock. Score is level-scaled, so it
    // cannot be inverted back to a line count after level 1.
    int linesBefore = totalLinesCleared;

    // Commit: overwrite currentBlock with the landed configuration and lock.
    currentBlock = target;
    lastMoveWasRotate = false;
    LockBlock();

    return totalLinesCleared - linesBefore;
}
