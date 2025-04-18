#include "game.h"
#include <random>

Game::Game()
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
    if (blocks.empty())
    {
        blocks = GetAllBlocks();
    }
    int randomIndex = rand() % blocks.size();
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

void Game::HandleInput()
{
    int keyPressed = GetKeyPressed();
    if (gameOver && keyPressed != 0)
    {
        gameOver = false;
        Reset();
    }

    switch(keyPressed)
    {
    case KEY_LEFT:
        MoveBlockLeft();
        break;

    case KEY_RIGHT:
        MoveBlockRight();
        break;

    case KEY_DOWN:
        MoveBlockDown();
        break;
    
    case KEY_UP:
        RotateBlock();
        break;
    
    case KEY_SPACE:
        MoveBlockDrop();
        break;
    }

    DropExpectation();
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
