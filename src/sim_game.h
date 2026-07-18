#pragma once
#include <cstdint>
#include <vector>
#include "sim_grid.h"
#include "sim_block.h"
#include "sim_blocks.h"
#include "../core/rng.h"
#include "../core/input.h"
#include "../core/constants.h"

// [NET/RL] Headless Tetris simulation. No raylib, no audio, no I/O.
//
// SimGame is the single source of truth for game logic and must produce the
// same state transitions as the old Game class (verified via ComputeStateHash).
//
// Two action levels:
//   - frame-level (lockstep net play):    SubmitInput(mask) + Tick()
//   - placement-level (RL):                LegalPlacements() + ApplyPlacement(col, rot)
//
// Observations for Python/pybind11 are exposed via accessor methods.
class SimGame
{
public:
    static constexpr int kNextPreviewCount = 3;

    explicit SimGame(uint64_t seed = 0);

    // ---- Placement-level action API (for RL training) ----
    struct Placement
    {
        int col;
        int rot;
    };
    // Enumerates every (col, rot) where the current piece can land via
    // rotate-then-translate-then-hard-drop. col is the piece's columnOffset
    // after moving, rot is the target rotation state.
    std::vector<Placement> LegalPlacements() const;
    // Applies a placement decision atomically (rotate -> translate -> hard drop -> lock).
    // Returns the number of lines cleared, or -1 if the placement is illegal.
    int ApplyPlacement(int col, int rot);

    // ---- Frame-level action API (for lockstep net play) ----
    void SubmitInput(uint8_t inputMask);
    void Tick();
    void MoveBlockDown();

    // ---- Observation accessors ----
    // Returns a const reference to the raw 20x10 grid. Layout matches old
    // Grid::grid for bitwise hash parity.
    const int (&Grid() const)[SimGrid::kRows][SimGrid::kCols] { return sim_grid.grid; }

    const SimBlock& CurrentBlock() const { return currentBlock; }
    const SimBlock& GhostBlock() const { return ghostBlock; }
    const SimBlock& NextBlock() const { return nextBlocks.front(); }
    const std::vector<SimBlock>& NextBlocks() const { return nextBlocks; }

    int CurrentBlockId() const { return currentBlock.id; }
    int CurrentRotation() const { return currentBlock.rotationState; }
    int CurrentRow() const { return currentBlock.rowOffset; }
    int CurrentCol() const { return currentBlock.columnOffset; }
    int NextBlockId() const { return NextBlock().id; }
    int Score() const { return score; }
    bool IsGameOver() const { return gameOver; }

    // ---- Determinism / debugging ----
    // Matches Game::ComputeStateHash bitwise (hash parity gate).
    uint64_t StateHash() const;
    uint64_t RngState() const { return rng.getState(); }

    // DESYNC 원인 특정용 섹션별 해시. 두 인스턴스에서 이 값을 비교하면 어느
    // 부분(그리드/블록/RNG/콤바트)이 달라졌는지 즉시 좁힐 수 있다.
    struct HashBreakdown {
        uint64_t grid;
        uint64_t currentBlock;
        uint64_t nextBlock;
        uint64_t rng;
        uint64_t scoreFlags;    // score, gameOver, gravity/drop timers, level, T-spin setup
        uint64_t combat;        // garbageRng, attackLinesSent, pendingGarbage
    };
    HashBreakdown StateHashBreakdown() const;

    // ---- Combat API (Section I) ----
    // attackLinesSent: 세션 전체 누적 공격 라인 수. 외부에서 델타를 뽑아
    //   상대 SimGame::AddPendingGarbage 로 전달한다. 네트워크 프레임 없음.
    // pendingGarbage: 다음 LockBlock 시점에 하단으로 삽입될 가비지 행 수.
    int AttackLinesSent() const { return attackLinesSent; }
    int PendingGarbage() const { return pendingGarbage; }
    void AddPendingGarbage(int rows) { if (rows > 0) pendingGarbage += rows; }

    // ---- Public mutable state (for renderer wrapper backward-compat) ----
    // main.cpp reads/writes Game::gameOver and reads Game::score via reference
    // members; exposing them here lets the Game wrapper alias them directly.
    bool gameOver;
    int score;

    // ---- One-shot event flags for audio in the Game wrapper ----
    // Set by SimGame when the corresponding event occurs (successful rotate,
    // line clear). The Game wrapper reads and clears them each tick.
    mutable bool rotateSoundEvent  = false;
    mutable bool clearSoundEvent   = false;
    mutable bool dropSoundEvent    = false;  // 하드드롭(Space) 시
    mutable bool garbageSoundEvent = false;  // 가비지 행 수신 시
    // 하드드롭 화면 흔들림(약) 트리거용. dropSoundEvent 와 별개 — 그쪽은
    // 오디오(game.cpp)가 소비·리셋하므로 흔들림이 그것에 의존하면 안 된다.
    // 렌더 전용 1회 플래그 (해시/lockstep/replay 와 무관).
    mutable bool hardDropEvent     = false;  // 하드드롭(Space) 시 (흔들림용)

    // ---- Combat event flags (Section I) ----
    // LockBlock 내부에서 세팅되고 렌더러(쉐이크/이펙트)가 소비 후 클리어.
    mutable int  lastLinesCleared = 0;    // 마지막 LockBlock의 라인 클리어 수 (0..4)
    mutable int  lastTSpinLines = -1;     // T-spin 이벤트면 0..3, 아니면 -1
    mutable int  lastGarbageReceived = 0; // 마지막 LockBlock에서 실제 주입된 가비지 행 수
    mutable bool gameOverEvent = false;   // 이 틱에 gameOver 로 전이한 경우 1회

    // ---- Level system ----
    int totalLinesCleared = 0;  // 누적 클리어 라인 수
    int level = 1;              // 현재 레벨 (10라인마다 +1, 최대 20)

private:
    void MoveBlockLeft();
    void MoveBlockRight();
    void MoveBlockDrop();
    void DropExpectation();
    void RotateBlockImpl();
    void LockBlock();
    void UpdateScore(int linesCleared, int levelUp, bool tSpin);
    void InsertGarbage(int rows);

    bool IsTSpinLock() const;
    bool IsBlockOutside(const SimBlock& block) const;
    bool BlockFits(const SimBlock& block) const;

    SimBlock GetRandomBlock();
    std::vector<SimBlock> GetAllBlocks() const;
    SimBlock MakeGhostBlock(const SimBlock& block) const;

    SimGrid sim_grid;
    std::vector<SimBlock> blocks;
    XorShift64Star rng;
    // 가비지 홀 컬럼용 별도 RNG 스트림. 시드에서 유도되어 양쪽 클라이언트가
    // 동일한 홀 시퀀스를 뽑는다. piece-bag RNG 와 상태가 섞이지 않음이 중요.
    XorShift64Star garbageRng;
    SimBlock currentBlock;
    SimBlock ghostBlock;
    std::vector<SimBlock> nextBlocks;

    int gravityCounterTicks;
    int dropIntervalTicks;

    // Soft-drop (held DOWN) rate limit — 일반 테트리스는 중력보다 빠르지만
    // 프레임레이트(60Hz) 그대로 내리면 60셀/초로 과도. 아래 카운터로 N틱마다
    // 한 번만 MoveBlockDown 호출. 최초 눌림은 즉시 반응(카운터=0 시작).
    int softDropCounterTicks = 0;

    // T-spin 판정 상태. 마지막 "성공한" 위치 변경이 회전이면 다음 lock 에서
    // T-piece pivot 주변 네 모서리 중 3개 이상이 막힌 경우 T-spin 으로 본다.
    bool lastMoveWasRotate = false;

    // Combat state
    int attackLinesSent = 0;
    int pendingGarbage = 0;
};
