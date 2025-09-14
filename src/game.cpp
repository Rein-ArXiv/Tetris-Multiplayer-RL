#include "game.h"

// [NET] seed는 모든 참가자가 동일해야 동일한 블록 순서를 보장합니다.
Game::Game(uint64_t seed)
    : rng(seed ? seed : 0xC0FFEE123456789ull),
      gravityCounterTicks(0),
      dropIntervalTicks(TICKS_PER_SECOND / 2) // default: drop every 0.5s
{
    grid = Grid();
    blocks = GetAllBlocks();
    currentBlock = GetRandomBlock();
    nextBlock = GetRandomBlock();
    ghostBlock = MakeGhostBlock(currentBlock);
    gameOver = false;
    score = 0;
    InitAudioDevice();
    music = LoadMusicStream("Sounds/music.mp3");
    PlayMusicStream(music);
    rotateSound = LoadSound("Sounds/rotate.mp3");
    clearSound = LoadSound("Sounds/clear.mp3");
    //dropSound
    //levelUpSound
}

Game::~Game()
{
    UnloadMusicStream(music);
    UnloadSound(rotateSound);
    UnloadSound(clearSound);
    CloseAudioDevice();
}

Block Game::GetRandomBlock()
{
    // [NET] '가방'이 비면 새 가방을 채웁니다. RNG 호출 횟수가 틱/입력 흐름에 따라 달라지지 않도록 주의합니다.
    if (blocks.empty())
    {
        blocks = GetAllBlocks();
    }
    int randomIndex = rng.nextUInt(static_cast<uint32_t>(blocks.size())); // [NET] RNG 상태는 동기화 대상
    Block block = blocks[randomIndex];
    blocks.erase(blocks.begin() + randomIndex);
    return block;
}

std::vector<Block> Game::GetAllBlocks()
{
    return {IBlock(), JBlock(), LBlock(), OBlock(), SBlock(), TBlock(), ZBlock()};
}

void Game::Draw()
{
    grid.Draw();
    ghostBlock.Draw(11, 11);
    currentBlock.Draw(11, 11);
    
    switch(nextBlock.id)
    {
        case 3:
            nextBlock.Draw(255, 260);
            break;
        case 4:
            nextBlock.Draw(255, 280);
            break;
        default:
            nextBlock.Draw(270, 270);
            break;
    }
}

// [NET] 수집된 틱 입력을 적용합니다. 원격 입력도 같은 틱에 합쳐 적용해야 Lockstep이 유지됩니다.
void Game::SubmitInput(uint8_t inputMask)
{
    if (gameOver) return;

    if (hasInput(inputMask, INPUT_LEFT))  MoveBlockLeft();
    if (hasInput(inputMask, INPUT_RIGHT)) MoveBlockRight();
    if (hasInput(inputMask, INPUT_DOWN))  MoveBlockDown();
    if (hasInput(inputMask, INPUT_ROTATE)) RotateBlock();
    if (hasInput(inputMask, INPUT_DROP))  MoveBlockDrop();

    DropExpectation();
}

// [NET] 입력과 분리된 시간 진행. Lockstep에서는 '모든 참가자의 해당 틱 입력 수신' 이후에만 Tick을 진행합니다.
void Game::Tick()
{
    if (gameOver) return;
    gravityCounterTicks++;
    if (gravityCounterTicks >= dropIntervalTicks)
    {
        gravityCounterTicks = 0;
        MoveBlockDown();
    }
}

void Game::MoveBlockLeft()
{
    if (!gameOver) {
        currentBlock.Move(0, -1);
        if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
        {
            currentBlock.Move(0, 1);
        }
        else
        {
            ghostBlock = MakeGhostBlock(currentBlock);
        }
    }
}

void Game::MoveBlockRight()
{
    if (!gameOver) {
        currentBlock.Move(0, 1);
        if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
        {
            currentBlock.Move(0, -1);
        }
        else
        {
            ghostBlock = MakeGhostBlock(currentBlock);
        }
    }
}

void Game::MoveBlockDown()
{
    if (!gameOver) {
        currentBlock.Move(1, 0);
        if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
        {
            currentBlock.Move(-1, 0);
            LockBlock();
        }
    }
}

void Game::MoveBlockDrop()
{
    if (!gameOver) {
        while (IsBlockOutside(currentBlock) == false && BlockFits(currentBlock) == true)
        {
            currentBlock.Move(1, 0);
        }
        currentBlock.Move(-1, 0);
        LockBlock();
    }
}

void Game::DropExpectation()
{
    if (!gameOver) {
        while(IsBlockOutside(ghostBlock) == false && BlockFits(ghostBlock) == true)
        {
            ghostBlock.Move(1, 0);
        }

        ghostBlock.Move(-1, 0);
    }
}

bool Game::IsBlockOutside(const Block& block)
{
    std::vector<Position> tiles = block.GetCellPositions();
    for (Position item : tiles)
    {
        if (grid.IsCellOutside(item.row, item.column))
        {
            return true;
        }
    }
    return false;
}

void Game::RotateBlock()
{
    if (!gameOver) {
        currentBlock.Rotate();
        if (IsBlockOutside(currentBlock) == true || BlockFits(currentBlock) == false)
        {
            currentBlock.UndoRotation();
        }
        else
        {
            PlaySound(rotateSound);
            ghostBlock = MakeGhostBlock(currentBlock);
        }
    }
}

void Game::LockBlock()
{
    std::vector<Position> tiles = currentBlock.GetCellPositions();
    for(Position item : tiles)
    {
        grid.grid[item.row][item.column] = currentBlock.id;
    }
    currentBlock = nextBlock;
    ghostBlock = MakeGhostBlock(currentBlock);
    if (BlockFits(currentBlock) == false)
    {
        gameOver = true;
    }
    
    nextBlock = GetRandomBlock();
    int rowsCleared = grid.ClearFullRows();
    if (rowsCleared > 0)
    {
        PlaySound(clearSound);
        UpdateScore(rowsCleared, 0);        
        // [NET] 대전 규칙: rowsCleared>0이면 상대에게 Garbage 전송 이벤트를 큐에 넣는 훅이 들어갑니다.
        // ex) OnLinesCleared(rowsCleared) → NetSession.Enqueue(GARBAGE, rowsCleared)
    }
}

bool Game::BlockFits(const Block& block)
{
    std::vector<Position> tiles = block.GetCellPositions();
    for(Position item : tiles)
    {
        if (grid.IsCellEmpty(item.row, item.column) == false)
        {
            return false;
        }
    }
    return true;
}

void Game::Reset()
{
    grid.Initialize();
    blocks = GetAllBlocks();
    currentBlock = GetRandomBlock();
    nextBlock = GetRandomBlock();
    ghostBlock = MakeGhostBlock(currentBlock);
    score = 0;
    gravityCounterTicks = 0;
    // [NET] 리셋은 시드/틱을 세션에서 재합의한 뒤에만 수행해야 합니다.
}

void Game::UpdateScore(int linesCleared, int levelUp)
{
    switch(linesCleared)
    {
    case 1:
        score += 100;
        break;
    case 2:
        score += 300;
        break;
    case 3:
        score += 600;
        break;
    case 4:
        score += 1000;
        break;
    default:
        break;
    }
    score += levelUp * 1000;
}

Block Game::MakeGhostBlock(const Block& block)
{
    ghostBlock = block;
    ghostBlock.id = 8;

    return ghostBlock;
}

// [NET] 참고: 상태 해시/스냅샷 지점
// - 상태 해시: grid 내용, current/next 블록 상태, RNG 내부 상태, score, gameOver, gravity 카운터 등을
//   해시(FNV-1a 등)하여 틱별로 비교하면 desync 탐지가 가능합니다.
// - 스냅샷: 위 구성요소 직렬화/역직렬화 코드를 추가하면 네트워크 재동기/리플레이에 활용할 수 있습니다.

unsigned long long Game::ComputeStateHash() const
{
    uint64_t h = 14695981039346656037ull;
    // Grid
    h = fnv1a64(&grid.grid[0][0], sizeof(grid.grid), h);
    // Current block
    h = fnv1a64_value(currentBlock.id, h);
    int curRot = currentBlock.GetRotationState();
    int curRow = currentBlock.GetRowOffset();
    int curCol = currentBlock.GetColumnOffset();
    h = fnv1a64_value(curRot, h);
    h = fnv1a64_value(curRow, h);
    h = fnv1a64_value(curCol, h);
    // Next block
    h = fnv1a64_value(nextBlock.id, h);
    int nxtRot = nextBlock.GetRotationState();
    int nxtRow = nextBlock.GetRowOffset();
    int nxtCol = nextBlock.GetColumnOffset();
    h = fnv1a64_value(nxtRot, h);
    h = fnv1a64_value(nxtRow, h);
    h = fnv1a64_value(nxtCol, h);
    // RNG state, score, game flags, gravity
    uint64_t rngState = rng.getState();
    h = fnv1a64_value(rngState, h);
    h = fnv1a64_value(score, h);
    int over = gameOver ? 1 : 0;
    h = fnv1a64_value(over, h);
    h = fnv1a64_value(gravityCounterTicks, h);
    h = fnv1a64_value(dropIntervalTicks, h);
    return static_cast<unsigned long long>(h);
}
