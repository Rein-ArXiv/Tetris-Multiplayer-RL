"""Language-boundary checks with a direct C++ Round oracle; no learning library."""
import argparse
import gc
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def rejected(call, exception):
    try:
        call()
    except exception:
        return
    raise AssertionError(f"expected {exception}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--module-dir", required=True)
    parser.add_argument("--oracle", required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.module_dir)
    import study_py as sim

    # Include every valid bit combination, neutral gravity, and an end state.
    masks = [m for m in range(sim.KNOWN + 1) if not m & ~sim.KNOWN]
    streams = [[0] * 125, masks * 3, [sim.LEFT | sim.ROTATE, sim.DROP] * 100]
    for seed in [0, 1, 77, (1 << 64)-1]:
        for interval in [1, 7, 30]:
            for stream in streams:
                game = sim.Session(seed, interval)
                native = subprocess.run(
                    [args.oracle, str(seed), str(interval), *map(str, stream)],
                    text=True, capture_output=True, check=True, timeout=10).stdout.splitlines()
                actual = [(-1, game.score(), int(game.finished()), game.state_bytes().hex())]
                for mask in stream:
                    result = game.step(mask)
                    actual.append((int(result), game.score(), int(game.finished()), game.state_bytes().hex()))
                require(native == [f"{s} {score} {end} {data}" for s,score,end,data in actual], "direct Round parity")

    game = sim.Session(77)
    original = game.state_bytes()
    grid = game.grid()
    require(len(grid) == sim.ROWS and all(len(row) == sim.COLS for row in grid), "dimensions")
    grid[0][0] = 99
    require(game.grid()[0][0] != 99 and game.state_bytes() == original, "copy isolates native state")
    grid_before = game.grid()
    clone = game.clone()
    require(clone is not game and clone.state_bytes() == original, "initial clone equality")
    clone.step(sim.DROP)
    require(game.state_bytes() == original and clone.state_bytes() != original, "clone independence")
    game.step(sim.DROP)
    require(game.state_bytes() == clone.state_bytes(), "same next input after clone")
    require(grid_before != game.grid(), "observed state advances")
    require(all(cell == 0 for row in grid_before for cell in row), "old observation unchanged")
    saved = game.state_bytes()
    for mask in [sim.KNOWN + 1, 256, (1 << 32)-1]:
        rejected(lambda: game.step(mask), ValueError)
        require(game.state_bytes() == saved, "invalid bits unchanged")
    for mask in [-1, 1 << 32, 1.5, "1", None]:
        rejected(lambda: game.step(mask), TypeError)
        require(game.state_bytes() == saved, "failed conversion unchanged")
    for interval in [0, -1]:
        rejected(lambda: game.reset(10, interval), ValueError)
        require(game.state_bytes() == saved, "failed reset unchanged")
    for seed in [-1, 1 << 64, 0.5, "77"]:
        rejected(lambda: game.reset(seed), TypeError)
        require(game.state_bytes() == saved, "failed seed conversion unchanged")
    game.reset(77)
    require(game.state_bytes() == original, "reset restores owned RNG and counters")
    for _ in range(1000):
        if game.finished():
            break
        game.step(sim.DROP)
    require(game.finished(), "terminal fixture")
    terminal = game.state_bytes()
    require(game.step(0) == sim.Step.stopped and game.state_bytes() == terminal, "terminal no-op")
    rejected(lambda: game.step(256), ValueError)
    require(game.state_bytes() == terminal, "validate even after end")
    del game
    gc.collect()
    clone.step(0)
    require(isinstance(saved, bytes) and grid_before[0][0] == 0, "snapshots survive owner")
    print("C++ direct Round/Python exact bytes: seed, interval, masks, terminal parity passed")
    print("Owned clone, independent snapshots, reset, conversion errors and unchanged failure state passed")


if __name__ == "__main__":
    main()
