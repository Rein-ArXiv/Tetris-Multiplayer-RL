#include "game.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>

Game::Game() : state(GameState::MENU), score(0), level(1), linesCleared(0), 
               lastClearedLines(0), dropInterval(1.0), dropTimer(0.0), hasHeld(false) {
    srand(static_cast<unsigned int>(time(nullptr)));
    Initialize();
}

Game::~Game() {
    // 리소스 정리
}

void Game::Initialize() {
    // 그리드 초기화
    grid = Grid();
    
    // 블록 초기화
    currentBlock = GetRandomBlock();
    nextBlock = GetRandomBlock();
    UpdateGhostBlock();
    
    // 게임 상태 초기화
    state = GameState::PLAYING;
    score = 0;
    level = 1;
    linesCleared = 0;
    lastClearedLines = 0;
    dropInterval = 1.0;
    dropTimer = 0.0;
    hasHeld = false;
    
    // 홀드 블록 초기화
    heldBlock = Block(I_BLOCK);
}

void Game::Draw(WINDOW* win) {
    // 경계선 그리기
    box(win, 0, 0);
    
    // 그리드 그리기
    grid.Draw(win);
    
    // 현재 블록 그리기
    currentBlock.Draw(win);
    
    // 고스트 블록 그리기
    ghostBlock.Draw(win);
    
    // 게임 정보 그리기
    mvwprintw(win, 1, grid.GetWidth() + 2, "Score: %d", score);
    mvwprintw(win, 2, grid.GetWidth() + 2, "Level: %d", level);
    mvwprintw(win, 3, grid.GetWidth() + 2, "Lines: %d", linesCleared);
    
    // 다음 블록 표시
    mvwprintw(win, 5, grid.GetWidth() + 2, "Next:");
    WINDOW* nextWin = derwin(win, 6, 6, 6, grid.GetWidth() + 2);
    box(nextWin, 0, 0);
    
    // 다음 블록의 위치 조정을 위한 임시 복사본
    Block nextBlockCopy = nextBlock;
    nextBlockCopy.rowOffset = 1;
    nextBlockCopy.columnOffset = 1;
    nextBlockCopy.Draw(nextWin);
    
    // 홀드 블록 표시
    mvwprintw(win, 13, grid.GetWidth() + 2, "Hold:");
    WINDOW* holdWin = derwin(win, 6, 6, 14, grid.GetWidth() + 2);
    box(holdWin, 0, 0);
    
    if (hasHeld) {
        Block heldBlockCopy = heldBlock;
        heldBlockCopy.rowOffset = 1;
        heldBlockCopy.columnOffset = 1;
        heldBlockCopy.Draw(holdWin);
    }
    
    // 게임 상태에 따라 다른 메시지 표시
    if (state == GameState::GAME_OVER) {
        mvwprintw(win, grid.GetHeight() / 2 - 1, grid.GetWidth() / 2 - 4, "GAME OVER");
        mvwprintw(win, grid.GetHeight() / 2, grid.GetWidth() / 2 - 9, "PRESS 'R' TO RESTART");
    } else if (state == GameState::PAUSED) {
        mvwprintw(win, grid.GetHeight() / 2 - 1, grid.GetWidth() / 2 - 3, "PAUSED");
        mvwprintw(win, grid.GetHeight() / 2, grid.GetWidth() / 2 - 9, "PRESS 'P' TO CONTINUE");
    }
}

InputAction Game::HandleInput(int key) {
    if (state == GameState::GAME_OVER) {
        if (key == 'r' || key == 'R') {
            Reset();
            return InputAction::NONE;
        }
    } else if (state == GameState::PAUSED) {
        if (key == 'p' || key == 'P') {
            state = GameState::PLAYING;
            return InputAction::NONE;
        }
    }
    
    if (state != GameState::PLAYING) {
        return InputAction::NONE;
    }
    
    // 플레이 중에만 처리할 입력
    switch (key) {
        case KEY_LEFT:
            return InputAction::MOVE_LEFT;
        case KEY_RIGHT:
            return InputAction::MOVE_RIGHT;
        case KEY_DOWN:
            return InputAction::MOVE_DOWN;
        case KEY_UP:
            return InputAction::ROTATE;
        case ' ':
            return InputAction::HARD_DROP;
        case 'c': case 'C':
            return InputAction::HOLD;
        case 'p': case 'P':
            return InputAction::PAUSE;
        case 'q': case 'Q':
            return InputAction::QUIT;
        default:
            return InputAction::NONE;
    }
}

void Game::ProcessAction(InputAction action) {
    if (state != GameState::PLAYING) {
        return;
    }
    
    switch (action) {
        case InputAction::MOVE_LEFT:
            MoveBlockLeft();
            break;
        case InputAction::MOVE_RIGHT:
            MoveBlockRight();
            break;
        case InputAction::MOVE_DOWN:
            MoveBlockDown();
            break;
        case InputAction::ROTATE:
            RotateBlock();
            break;
        case InputAction::HARD_DROP:
            HardDropBlock();
            break;
        case InputAction::HOLD:
            HoldBlock();
            break;
        case InputAction::PAUSE:
            state = GameState::PAUSED;
            break;
        case InputAction::QUIT:
            state = GameState::GAME_OVER;
            break;
        default:
            break;
    }
}

void Game::Update(double deltaTime) {
    if (state != GameState::PLAYING) {
        return;
    }
    
    // 자동 낙하 처리
    dropTimer += deltaTime;
    if (dropTimer >= dropInterval) {
        MoveBlockDown();
        dropTimer = 0;
    }
}

void Game::MoveBlockLeft() {
    currentBlock.Move(0, -1);
    if (!BlockFits(currentBlock)) {
        currentBlock.Move(0, 1);
    } else {
        UpdateGhostBlock();
    }
}

void Game::MoveBlockRight() {
    currentBlock.Move(0, 1);
    if (!BlockFits(currentBlock)) {
        currentBlock.Move(0, -1);
    } else {
        UpdateGhostBlock();
    }
}

void Game::MoveBlockDown() {
    currentBlock.Move(1, 0);
    if (!BlockFits(currentBlock)) {
        currentBlock.Move(-1, 0);
        LockBlock();
    }
}

void Game::RotateBlock() {
    currentBlock.Rotate();
    if (!BlockFits(currentBlock)) {
        // 회전이 가능하지 않으면 원상복구
        currentBlock.UndoRotate();
    } else {
        UpdateGhostBlock();
    }
}

void Game::HardDropBlock() {
    // 바닥에 닿을 때까지 블록을 아래로 이동
    while (BlockFits(currentBlock)) {
        currentBlock.Move(1, 0);
    }
    currentBlock.Move(-1, 0); // 충돌 위치에서 한 칸 위로
    LockBlock();
}

void Game::HoldBlock() {
    if (hasHeld) {
        // 이미, 홀드한 블록과 현재 블록 교체
        std::swap(currentBlock, heldBlock);
        UpdateGhostBlock();
    } else {
        // 처음 홀드
        heldBlock = currentBlock;
        currentBlock = nextBlock;
        nextBlock = GetRandomBlock();
        hasHeld = true;
        UpdateGhostBlock();
    }
}

bool Game::BlockFits(const Block& block) const {
    std::vector<Position> positions = block.GetCellPositions();
    
    for (const auto& pos : positions) {
        if (grid.IsCellOutside(pos.row, pos.column) || !grid.IsCellEmpty(pos.row, pos.column)) {
            return false;
        }
    }
    
    return true;
}

void Game::LockBlock() {
    // 현재 블록을 그리드에 고정
    std::vector<Position> positions = currentBlock.GetCellPositions();
    
    for (const auto& pos : positions) {
        if (!grid.IsCellOutside(pos.row, pos.column)) {
            grid.cells[pos.row][pos.column] = static_cast<int>(currentBlock.type);
        }
    }
    
    // 완성된 라인 제거 및 점수 갱신
    lastClearedLines = grid.ClearFullRows();
    if (lastClearedLines > 0) {
        UpdateScore(lastClearedLines);
    }
    
    // 다음 블록으로 교체
    currentBlock = nextBlock;
    nextBlock = GetRandomBlock();
    UpdateGhostBlock();
    
    // 홀드 사용 가능 상태로 리셋
    hasHeld = false;
    
    // 게임 오버 체크
    if (!BlockFits(currentBlock)) {
        state = GameState::GAME_OVER;
    }
}

void Game::UpdateGhostBlock() {
    // 고스트 블록 생성
    ghostBlock = Block::CreateGhostBlock(currentBlock);
    
    // 바닥까지 이동
    while (BlockFits(ghostBlock)) {
        ghostBlock.Move(1, 0);
    }
    ghostBlock.Move(-1, 0); // 충돌 직전으로 위치 조정
}

Block Game::GetRandomBlock() {
    int randomIndex = rand() % 7 + 1; // 1-7 사이의 값 (BlockType 열거형에 맞춤)
    return Block(static_cast<BlockType>(randomIndex));
}

void Game::UpdateScore(int clearedLines) {
    // 라인 수에 따른 점수 계산
    switch (clearedLines) {
        case 1:
            score += 100 * level;
            break;
        case 2:
            score += 300 * level;
            break;
        case 3:
            score += 500 * level;
            break;
        case 4:
            score += 800 * level; // 테트리스!
            break;
    }
    
    // 총 클리어한 라인 수 업데이트
    linesCleared += clearedLines;
    
    // 레벨 업데이트
    UpdateLevel();
}

void Game::UpdateLevel() {
    // 10줄마다 레벨업
    int newLevel = (linesCleared / 10) + 1;
    if (newLevel > level) {
        level = newLevel;
        UpdateDropInterval();
    }
}

void Game::UpdateDropInterval() {
    // 레벨에 따라 블록 낙하 속도 조절
    dropInterval = std::max(0.1, 1.0 - ((level - 1) * 0.1));
}

void Game::Reset() {
    Initialize();
}

std::vector<std::vector<int>> Game::GetBoardState() const {
    return grid.cells;
}

/////////////////////////
// src/player.cpp
/////////////////////////
#include "player.h"

Player::Player(int id, const std::string& name, PlayerType type) 
    : id(id), name(name), type(type), lastAction(InputAction::NONE) {
    // 플레이어 초기화
    game.Initialize();
}