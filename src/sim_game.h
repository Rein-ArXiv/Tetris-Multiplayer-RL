#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include "sim_grid.h"
#include "sim_block.h"
#include "sim_blocks.h"
#include "../core/rng.h"
#include "../core/input.h"
#include "../core/constants.h"
#include "../core/saturating_count.h"

// [NET/RL] Headless Tetris simulation. No renderer, no audio, no I/O.
//
// SimGame owns the rule state shared by game, server and training adapters.
// State hashes are diagnostic summaries with explicitly documented field sets.
//
// Two action levels:
//   - frame-level (lockstep net play):    SubmitInput(mask) + Tick()
//   - placement-level (RL):                LegalPlacements() + ApplyPlacement(col, rot)
//
// Observations for Python/pybind11 are exposed via accessor methods.
class SimGame
{
public:
    static_assert(std::numeric_limits<int>::digits == 31, "SimGame requires 32-bit int");
    static constexpr int kNextPreviewCount = 3;

    explicit SimGame(uint64_t seed = 0);

    // ---- Placement-level action API (for RL training) ----
    struct Placement
    {
        int col;
        int rot;
    };
    // Enumerates (col, rot) in the fixed action domain col=0..kCols-1.
    // Some negative origins fit rotated pieces, but are not enumerated here;
    // ApplyPlacement can accept them. Candidates relocate the rotated shape
    // directly, then project downward; no intermediate key route or Tick runs. col is the piece's columnOffset
    // after moving, rot is the target rotation state.
    std::vector<Placement> LegalPlacements() const;
    // Atomically commits a geometric endpoint (rotate -> relocate -> project -> lock).
    // This API does not consume intermediate key requests or gravity ticks.
    // Returns the number of lines cleared, or -1 if the placement is illegal.
    int ApplyPlacement(int col, int rot);

    // ---- Frame-level action API (for lockstep net play) ----
    void SubmitInput(uint8_t inputMask);
    void Tick();
    void MoveBlockDown();

    // ---- Observation accessors ----
    // Borrowed row-major grid observation; hashing encodes each cell explicitly.
    const int (&Grid() const)[SimGrid::kRows][SimGrid::kCols] { return sim_grid.grid; }

    const SimBlock& CurrentBlock() const { return currentBlock; }
    // Cached landing hint for a live game; never draw it after game over.
    const SimBlock& GhostBlock() const { return ghostBlock; }
    // Read-only borrowed view, not consumption. Construction/refill keep it nonempty.
    // Copy a value if it must survive a lock; erase/refill invalidates element views.
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
    // Legacy wire/golden checksum, now explicitly LE. Omits the remaining bag.
    // Keep its field set until peer protocol/version negotiation is introduced.
    uint64_t StateHash() const;
    // Local diagnostic format SIMH/2 adds the remaining bag in order.
    // Assumes the same fixed block definitions/rules; not used by HASH messages.
    uint64_t DiagnosticStateHashV2() const;
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
    // Both counters saturate at int max; saturation stops further total deltas.
    // pendingGarbage: next-lock backlog; at most kRows are inserted in that batch.
    int AttackLinesSent() const { return attackLinesSent; }
    int PendingGarbage() const { return pendingGarbage; }
    void AddPendingGarbage(int rows) { pendingGarbage = saturating_add_count(pendingGarbage, rows); }

    // ---- Public mutable state (for renderer wrapper backward-compat) ----
    // main.cpp reads/writes Game::gameOver and reads Game::score via reference
    // members; exposing them here lets the Game wrapper alias them directly.
    bool gameOver;
    int score;                 // Nonnegative; saturates at int max.

    // ---- Coalescing audio flags, not an ordered event queue ----
    // SubmitInput consumes rotate/drop flags; Game::Tick consumes clear/garbage.
    // A bool remembers occurrence, not the number or order before consumption.
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
    mutable bool gameOverEvent = false;   // 종료 전이에서 설정; 표현 호출자가 읽고 지운다.

    // ---- Level system ----
    int totalLinesCleared = 0;  // 누적 클리어 라인 수 (int 상한에서 포화)
    int level = 1;              // 현재 레벨 (10라인마다 +1, 최대 20)

private:
    uint64_t ComputeStateHash(bool includeBag) const;
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

    // T-spin eligibility: successful rotation sets; successful left/right/down
    // clears. Failed moves and hard drop preserve it. Lock resets for the next
    // piece. This project uses a three-corner test, with no Mini/B2B/combo.
    bool lastMoveWasRotate = false;

    // Combat state
    int attackLinesSent = 0;
    int pendingGarbage = 0;
};
