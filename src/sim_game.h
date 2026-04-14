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
    const SimBlock& NextBlock() const { return nextBlock; }

    int CurrentBlockId() const { return currentBlock.id; }
    int CurrentRotation() const { return currentBlock.rotationState; }
    int CurrentRow() const { return currentBlock.rowOffset; }
    int CurrentCol() const { return currentBlock.columnOffset; }
    int NextBlockId() const { return nextBlock.id; }
    int Score() const { return score; }
    bool IsGameOver() const { return gameOver; }

    // ---- Determinism / debugging ----
    // Matches Game::ComputeStateHash bitwise (hash parity gate).
    uint64_t StateHash() const;
    uint64_t RngState() const { return rng.getState(); }

    // ---- Public mutable state (for raylib wrapper backward-compat) ----
    // main.cpp reads/writes Game::gameOver and reads Game::score via reference
    // members; exposing them here lets the Game wrapper alias them directly.
    bool gameOver;
    int score;

    // ---- One-shot event flags for audio in the raylib wrapper ----
    // Set by SimGame when the corresponding event occurs (successful rotate,
    // line clear). The raylib Game wrapper reads and clears them each tick.
    mutable bool rotateSoundEvent = false;
    mutable bool clearSoundEvent = false;

private:
    void MoveBlockLeft();
    void MoveBlockRight();
    void MoveBlockDrop();
    void DropExpectation();
    void RotateBlockImpl();
    void LockBlock();
    void UpdateScore(int linesCleared, int levelUp);

    bool IsBlockOutside(const SimBlock& block) const;
    bool BlockFits(const SimBlock& block) const;

    SimBlock GetRandomBlock();
    std::vector<SimBlock> GetAllBlocks() const;
    SimBlock MakeGhostBlock(const SimBlock& block) const;

    SimGrid sim_grid;
    std::vector<SimBlock> blocks;
    XorShift64Star rng;
    SimBlock currentBlock;
    SimBlock ghostBlock;
    SimBlock nextBlock;

    int gravityCounterTicks;
    int dropIntervalTicks;
};
