"""Compare native SimGame observations with a reviewed legacy-format capture.

A match covers these seeds, inputs and observation boundaries. The capture alone
has no verified OS/compiler provenance; cross-platform evidence requires recording
and comparing actual builds on each target. It is not a proof of all rule paths.

Generate a separate candidate, inspect differences and then deliberately adopt it:
    cmake --build build --target sim_hash_dump
    ./build/sim_hash_dump > candidate-sim-hash.txt

The committed reference must be present and structurally complete. Native-only
checks skip when the binding is unavailable; reference validation still runs.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from .determinism_reference import compare_records, parse_reference

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


def test_reference_capture_is_complete() -> None:
    # This gate must run even when the optional native extension is absent.
    parse_reference(REFERENCE_FILE.read_text(encoding="utf-8"), SEEDS, SCRIPT)


@pytest.mark.skipif(not _have_native(), reason="Native tetris_py not built")
def test_matches_cpp_reference_dump() -> None:
    """Compare initial state, every step field, row count and final summary."""
    from sim import SimGame

    expected = parse_reference(REFERENCE_FILE.read_text(encoding="utf-8"), SEEDS, SCRIPT)
    for seed in SEEDS:
        reference = expected[seed]
        initial = SimGame(seed).state_hash()
        assert initial == reference.initial_hash, f"seed {seed:x}: initial hash differs"
        rows = [(r.step, r.total_ticks, r.score, r.over, r.state_hash) for r in reference.records]
        actual = _run_script(seed)
        difference = compare_records(rows, actual)
        assert difference is None, f"seed {seed:x}: {difference}"
        assert actual[-1][2:] == (reference.final_score, reference.final_over, reference.final_hash)
