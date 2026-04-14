"""Cross-platform determinism gate for ``SimGame``.

Runs the same scripted input sequence used by ``tests/sim_hash_dump.cpp`` and
checks the resulting state hashes against a frozen reference table. The
reference table is the C++ ``sim_hash_dump`` output captured on Windows; if
the same numbers come out of the Python binding (which links the same C++
sources), Linux and Windows builds are bitwise-identical.

How to (re)generate the reference numbers:

    cmake --build build --target sim_hash_dump
    ./build/sim_hash_dump > python/tests/_sim_hash_dump.txt

The fixture below parses ``_sim_hash_dump.txt`` if present and compares.
If the file is missing the test is skipped — that lets the suite stay green
until the C++ binary has been built at least once.
"""

from __future__ import annotations

from pathlib import Path

import pytest

# Mirror of the script in tests/sim_hash_dump.cpp. Keep these two in sync —
# any change here must be reflected in the C++ test driver.
SCRIPT: list[tuple[int, int]] = [
    (0x00,                30),  # INPUT_NONE
    (0x01,                 1),  # LEFT
    (0x01,                 1),
    (0x01,                 1),
    (0x08,                 1),  # ROTATE
    (0x10,                 2),  # DROP
    (0x00,                 5),
    (0x02,                 1),  # RIGHT
    (0x02,                 1),
    (0x08,                 1),
    (0x08,                 1),
    (0x04,                 1),  # DOWN
    (0x04,                 1),
    (0x10,                 2),
    (0x00,                10),
    (0x01,                 1),
    (0x10,                 1),
    (0x00,                 5),
    (0x02,                 1),
    (0x02,                 1),
    (0x02,                 1),
    (0x02,                 1),
    (0x08,                 1),
    (0x10,                 1),
    (0x00,               120),
    (0x01 | 0x08,          1),  # LEFT | ROTATE
    (0x10,                 1),
    (0x00,                30),
    (0x02 | 0x08,          1),  # RIGHT | ROTATE
    (0x10,                 1),
    (0x00,               180),
]

REFERENCE_FILE = Path(__file__).parent / "_sim_hash_dump.txt"

# Seeds in tests/sim_hash_dump.cpp default list.
SEEDS: list[int] = [
    0x0000000000000001,
    0x00000000DEADBEEF,
    0x0C0FFEE123456789,
]


def _have_native() -> bool:
    try:
        import sim  # noqa: F401, PLC0415
        return True
    except ImportError:
        return False


def _run_script(seed: int) -> list[tuple[int, int, int, bool, int]]:
    """Replay the script on a fresh SimGame and return the per-step state
    tuple ``(step, total_ticks, score, game_over, state_hash)``.

    Mirror of ``run_and_dump`` in ``sim_hash_dump.cpp``.
    """
    from sim import SimGame  # noqa: PLC0415

    sim = SimGame(seed)
    out: list[tuple[int, int, int, bool, int]] = []
    total_ticks = 0
    for step_index, (mask, ticks) in enumerate(SCRIPT):
        sim.submit_input(mask)
        for _ in range(ticks):
            sim.tick()
            total_ticks += 1
        out.append(
            (step_index, total_ticks, sim.score(), sim.game_over(), sim.state_hash())
        )
        if sim.game_over():
            break
    return out


@pytest.mark.skipif(not _have_native(), reason="Native tetris_py not built")
def test_initial_hash_stable_across_seeds() -> None:
    """Sanity check: same seed -> same initial hash on every call."""
    from sim import SimGame  # noqa: PLC0415

    for seed in SEEDS:
        a = SimGame(seed).state_hash()
        b = SimGame(seed).state_hash()
        assert a == b, f"unstable initial hash for seed 0x{seed:016x}"


@pytest.mark.skipif(not _have_native(), reason="Native tetris_py not built")
def test_script_replay_stable() -> None:
    """Determinism: replaying the script twice gives the same hash sequence."""
    for seed in SEEDS:
        a = _run_script(seed)
        b = _run_script(seed)
        assert a == b, f"unstable script replay for seed 0x{seed:016x}"


@pytest.mark.skipif(not REFERENCE_FILE.exists(), reason="No reference dump")
@pytest.mark.skipif(not _have_native(), reason="Native tetris_py not built")
def test_matches_cpp_reference_dump() -> None:
    """Cross-platform parity vs the C++ ``sim_hash_dump`` reference output.

    Parses the captured stdout of the C++ test driver and checks that
    every ``hash=0x...`` line matches the Python replay.
    """
    text = REFERENCE_FILE.read_text(encoding="utf-8")

    # Parse: each "==== seed 0x... ====" block contains lines like
    #   step=000 mask=0x00 ticks=30 total_ticks=30 score=0 over=0 hash=0x...
    blocks = text.split("==== seed ")
    expected_by_seed: dict[int, list[int]] = {}
    for block in blocks[1:]:
        head, _, body = block.partition("\n")
        seed_str = head.strip().split()[0]
        seed = int(seed_str, 16)
        hashes: list[int] = []
        for line in body.splitlines():
            line = line.strip()
            if not line.startswith("step="):
                continue
            tag = "hash="
            idx = line.find(tag)
            if idx == -1:
                continue
            hashes.append(int(line[idx + len(tag):].split()[0], 16))
        expected_by_seed[seed] = hashes

    for seed, expected_hashes in expected_by_seed.items():
        actual = [row[4] for row in _run_script(seed)]
        assert actual == expected_hashes, (
            f"hash divergence for seed 0x{seed:016x}: "
            f"first mismatch at step "
            f"{next(i for i, (e, a) in enumerate(zip(expected_hashes, actual)) if e != a)}"
        )
