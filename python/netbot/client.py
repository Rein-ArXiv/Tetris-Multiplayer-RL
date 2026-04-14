"""60Hz lockstep client — joins a running C++ Tetris host as a peer.

This is the entry point of the Python netbot. It mirrors ``main.cpp:130-212``
(the host's tick accumulator + safeTickInclusive lockstep loop) so the bot
stays in lockstep with the C++ host without modifying any C++ code.

Run::

    python -m netbot.client --connect 127.0.0.1:7777               # rule-based
    python -m netbot.client --connect 127.0.0.1:7777 --policy x.pt # trained

Lockstep contract (from main.cpp):

- ``localTickNext`` increments every tick after we've sent our input
- ``lastLocalSent = localTickNext - 1`` (or -1 before the first send)
- ``safeTickInclusive = min(lastLocalSent, lastRemote) - inputDelay``
- We can advance our two ``SimGame`` mirrors (own + opponent) from
  ``sim_tick`` up to and including ``safeTickInclusive``, but no further
- Both sims must be reseeded with the SEED message's seed for the boards
  to stay bit-equal between us and the C++ host
"""

from __future__ import annotations

import argparse
import logging
import sys
import time
from collections import deque
from pathlib import Path

# When run as ``python -m netbot.client`` this resolves automatically. When
# run as a script directly we need to make ``python/`` importable.
_PY_DIR = Path(__file__).resolve().parents[1]
if str(_PY_DIR) not in sys.path:
    sys.path.insert(0, str(_PY_DIR))

from netbot.input_expander import (  # noqa: E402
    INPUT_NONE,
    expand_placement,
    fallback_placement,
)
from netbot.policy_runner import PolicyRunner, Runner, RuleBasedRunner  # noqa: E402
from netbot.session import BotSession, GameOverChoice  # noqa: E402

TICK_HZ = 60
TICK_PERIOD = 1.0 / TICK_HZ

log = logging.getLogger("netbot")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Tetris netbot client")
    parser.add_argument(
        "--connect",
        required=True,
        help="host:port of the C++ tetris host (e.g. 127.0.0.1:7777)",
    )
    parser.add_argument(
        "--policy",
        default=None,
        help="Path to a TetrisPolicyNet checkpoint .pt file. "
        "If omitted, falls back to the rule-based BCTS runner.",
    )
    parser.add_argument(
        "--device",
        default="cpu",
        help="Torch device for policy inference (default: cpu)",
    )
    parser.add_argument(
        "--hash-interval",
        type=int,
        default=60,
        help="Send a HASH cross-check every N ticks (0 to disable)",
    )
    parser.add_argument(
        "--log-level",
        default="INFO",
        help="Logging level (DEBUG, INFO, WARNING, ERROR)",
    )
    return parser.parse_args(argv)


def split_endpoint(value: str) -> tuple[str, int]:
    if ":" not in value:
        raise ValueError(f"--connect must be host:port, got {value!r}")
    host, port = value.rsplit(":", 1)
    return host, int(port)


def make_runner(policy_path: str | None, device: str) -> Runner:
    if policy_path is None:
        log.info("No --policy supplied; using rule-based BCTS runner")
        return RuleBasedRunner()
    log.info("Loading policy checkpoint from %s on %s", policy_path, device)
    return PolicyRunner(policy_path, device=device)


def run() -> int:
    args = parse_args()
    logging.basicConfig(
        level=args.log_level,
        format="[%(asctime)s][%(name)s][%(levelname)s] %(message)s",
    )
    host, port = split_endpoint(args.connect)

    sess = BotSession(host, port)
    log.info("Connecting to %s:%d", host, port)
    sess.connect()

    # Wait for the host's SEED message before constructing the sim. The C++
    # host's HELLO + SEED arrive almost immediately after our HELLO is sent.
    handshake_deadline = time.perf_counter() + 10.0
    while not sess.ready:
        if sess.failed or time.perf_counter() > handshake_deadline:
            log.error("Handshake failed or timed out")
            sess.close()
            return 2
        sess.pump()
        time.sleep(0.002)

    log.info(
        "Handshake complete: seed=0x%016x start_tick=%d input_delay=%d role=%d",
        sess.seed_params.seed,
        sess.seed_params.start_tick,
        sess.seed_params.input_delay,
        sess.seed_params.role,
    )

    # Import the native sim only after the handshake — that way --help and
    # connection errors don't require the .pyd/.so to be present.
    from sim import SimGame  # noqa: PLC0415

    seed = sess.seed_params.seed
    input_delay = sess.seed_params.input_delay
    start_delay = sess.seed_params.start_tick

    # Two sim mirrors: one for our (bot's) board, one for the host's board.
    # Both reseeded with the same seed for bitwise determinism.
    sim_self = SimGame(seed)
    sim_opp = SimGame(seed)

    runner = make_runner(args.policy, args.device)

    pending_inputs: deque[int] = deque()
    last_piece_id = sim_self.current_block_id()

    local_tick_next = 0
    sim_tick = 0
    next_deadline = time.perf_counter()

    log.info("Entering 60Hz lockstep loop")

    while True:
        sess.pump()

        if sess.failed:
            log.error("Session failed mid-game; aborting")
            sess.close()
            return 3

        now = time.perf_counter()
        if now < next_deadline:
            time.sleep(min(0.002, next_deadline - now))
            continue
        # Accumulator-style next deadline -> drift-free 60Hz pacing.
        next_deadline += TICK_PERIOD

        # ---- Decide this tick's input ---------------------------------
        if start_delay > 0:
            mask = INPUT_NONE
            start_delay -= 1
        else:
            cur_id = sim_self.current_block_id()
            if cur_id != last_piece_id or not pending_inputs:
                col, rot = runner.select_placement(sim_self)
                if col < 0:
                    fb = fallback_placement(sim_self)
                    if fb is None:
                        log.warning("No legal placements available")
                        col, rot = 0, 0
                    else:
                        col, rot = fb
                seq = expand_placement(
                    sim_self.current_col(),
                    sim_self.current_rotation(),
                    col,
                    rot,
                )
                pending_inputs = deque(seq)
                last_piece_id = cur_id
            mask = pending_inputs.popleft() if pending_inputs else INPUT_NONE

        sess.send_input(local_tick_next, mask)
        local_tick_next += 1

        # ---- Lockstep simulation --------------------------------------
        last_local_sent = local_tick_next - 1
        safe_tick_inclusive = (
            min(last_local_sent, sess.last_remote_tick) - input_delay
        )
        while sim_tick <= safe_tick_inclusive:
            my = sess.get_own_input(sim_tick)
            op = sess.get_remote_input(sim_tick)
            if my is None or op is None:
                break
            sim_self.submit_input(my)
            sim_self.tick()
            sim_opp.submit_input(op)
            sim_opp.tick()
            sim_tick += 1

        # ---- Periodic hash cross-check --------------------------------
        if (
            args.hash_interval
            and sim_tick > 0
            and sim_tick % args.hash_interval == 0
        ):
            h = sim_self.state_hash()
            sess.send_hash(sim_tick - 1, h)
            if (
                sess.last_remote_hash_tick == sim_tick - 1
                and sess.last_remote_hash != sim_opp.state_hash()
            ):
                log.warning(
                    "DESYNC at tick %d: opp_hash=0x%016x remote=0x%016x",
                    sim_tick - 1,
                    sim_opp.state_hash(),
                    sess.last_remote_hash,
                )

        # ---- Game over handling ---------------------------------------
        if sim_self.game_over() or sim_opp.game_over():
            log.info(
                "Game over (self=%s opp=%s) — sending GO_TO_TITLE",
                sim_self.game_over(),
                sim_opp.game_over(),
            )
            sess.send_game_over_choice(GameOverChoice.GO_TO_TITLE)
            # Drain a few more pumps so the choice actually leaves the buffer.
            drain_until = time.perf_counter() + 0.5
            while time.perf_counter() < drain_until:
                sess.pump()
                time.sleep(0.005)
            sess.close()
            return 0


if __name__ == "__main__":
    raise SystemExit(run())
