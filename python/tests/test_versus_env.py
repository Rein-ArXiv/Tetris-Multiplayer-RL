"""Tests for the 2-board competitive (versus/garbage) environment.

Skipped if the native ``tetris_py`` module or ``gymnasium`` is unavailable
(e.g. CI without the built sim). When present, these exercise the garbage
plumbing the env relies on, a full episode, determinism, and attack routing.
"""
from __future__ import annotations

import numpy as np
import pytest

sim_mod = pytest.importorskip("sim")
pytest.importorskip("gymnasium")

from common.action_mask import decode_action  # noqa: E402
from common.env_versus import (  # noqa: E402
    GreedyBCTSOpponent,
    RandomLegalOpponent,
    TetrisVersusEnv,
    _terminal_bonus,
)


def _occupied(board_obs: np.ndarray) -> int:
    return int(np.count_nonzero(board_obs))


def _first_legal(mask: np.ndarray) -> int:
    idx = np.flatnonzero(mask)
    assert idx.size > 0, "no legal action available"
    return int(idx[0])


def test_garbage_injection_binding():
    """add_pending_garbage queues rows that are injected at the next lock,
    raising the stack — the mechanic the versus env routes attacks through."""
    SimGame = sim_mod.SimGame
    g = SimGame(seed=7)
    g.add_pending_garbage(4)
    assert g.pending_garbage() == 4
    p = g.legal_placements()[0]
    g.apply_placement(p.col, p.rot)
    assert g.last_garbage_received() == 4   # injected at this lock
    assert g.pending_garbage() == 0         # queue drained


def test_env_reset_and_obs_shapes():
    env = TetrisVersusEnv(seed=1, opponent=RandomLegalOpponent(seed=0))
    obs, info = env.reset()
    assert obs["board"].shape == (1, 20, 10)
    assert obs["current"].shape == (7,)
    assert obs["next"].shape == (7,)
    assert info["legal_mask"].shape == (40,)
    for key in ("incoming_garbage", "agent_attack", "opp_attack", "opp_alive"):
        assert key in info


def test_env_runs_to_completion():
    """A masked-random agent vs the greedy opponent reaches terminated or
    truncated within max_pieces, with finite rewards throughout."""
    env = TetrisVersusEnv(seed=5, opponent=GreedyBCTSOpponent(), max_pieces=300)
    obs, info = env.reset()
    rng = np.random.default_rng(123)
    steps = 0
    done = False
    while not done and steps < 5000:
        legal = np.flatnonzero(info["legal_mask"])
        action = int(rng.choice(legal)) if legal.size else 0
        obs, reward, terminated, truncated, info = env.step(action)
        assert np.isfinite(reward)
        done = terminated or truncated
        steps += 1
    assert done, "episode never ended"


def test_simultaneous_topout_penalizes_agent():
    assert _terminal_bonus(True, True, 10.0, 7.0) == -7.0
    assert _terminal_bonus(False, True, 10.0, 7.0) == 10.0
    assert _terminal_bonus(True, False, 10.0, 7.0) == -7.0


def test_env_garbage_raises_agent_stack():
    """Queue garbage directly on the agent board, then step: the board's
    occupancy should jump by the injected rows and the queue should drain."""
    env = TetrisVersusEnv(seed=2, opponent=RandomLegalOpponent(seed=1))
    obs, info = env.reset()
    env.simA.add_pending_garbage(4)
    before = _occupied(obs["board"])
    action = _first_legal(info["legal_mask"])
    obs, _, _, _, info = env.step(action)
    assert env.simA.last_garbage_received() == 4
    # 4 garbage rows (each one cell short of full) + the locked piece minus any
    # cleared lines → net occupancy strictly increases.
    assert _occupied(obs["board"]) > before


def test_env_determinism():
    """Same seeds + same agent actions + deterministic opponent → identical
    agent state-hash trajectory."""
    def run() -> list[int]:
        env = TetrisVersusEnv(seed=99, opponent=GreedyBCTSOpponent(), max_pieces=120)
        _, info = env.reset()
        hashes: list[int] = []
        for _ in range(60):
            action = _first_legal(info["legal_mask"])
            _, _, terminated, truncated, info = env.step(action)
            hashes.append(info["state_hash"])
            if terminated or truncated:
                break
        return hashes

    assert run() == run()


def test_env_attack_routes_to_opponent():
    """When the agent sends attack lines (info['agent_attack'] > 0) the
    opponent must receive at least that much garbage over the episode."""
    env = TetrisVersusEnv(seed=11, opponent=GreedyBCTSOpponent(),
                          max_pieces=400)
    _, info = env.reset()
    total_agent_attack = 0
    total_opp_garbage_received = 0
    for _ in range(400):
        # A random stack usually tops out before clearing a line, which made
        # the old conditional assertion vacuous. BCTS guarantees this fixture
        # exercises at least one attack while keeping both boards alive longer.
        action = GreedyBCTSOpponent().act(env.simA)
        assert action is not None
        _, _, terminated, truncated, info = env.step(action)
        total_agent_attack += info["agent_attack"]
        # The opponent consumes pending garbage during its placement in the
        # same env.step(), so inspect the lock event rather than the drained
        # pending queue after the step.
        total_opp_garbage_received += env.simB.last_garbage_received()
        if terminated or truncated:
            break
    assert total_agent_attack > 0, "fixture must send attack (non-vacuous regression)"
    assert total_opp_garbage_received == total_agent_attack
