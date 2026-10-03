"""Strict parser for the legacy sim_hash_dump text protocol used by regression tests."""
from __future__ import annotations
from dataclasses import dataclass
import re

@dataclass(frozen=True)
class Record:
    step: int
    mask: int
    ticks: int
    total_ticks: int
    score: int
    over: bool
    state_hash: int

@dataclass(frozen=True)
class Reference:
    seed: int
    initial_hash: int
    records: tuple[Record, ...]
    final_hash: int
    final_score: int
    final_over: bool


def parse_reference(text: str, seeds: list[int], script: list[tuple[int, int]]) -> dict[int, Reference]:
    """Reject empty, partial, duplicate, reordered or malformed captures.

    Validate framing and script metadata independently of the native simulator.
    Rule correctness still requires reviewing the chosen input/output cases.
    """
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    position = 0
    def take(pattern: str) -> tuple[str, ...]:
        nonlocal position
        if position >= len(lines):
            raise ValueError(f"truncated reference after {position} nonempty lines")
        line = lines[position]
        position += 1
        match = re.fullmatch(pattern, line)
        if not match:
            raise ValueError(f"reference line {position}: unexpected {line!r}")
        return match.groups()

    if not seeds or len(set(seeds)) != len(seeds) or not script:
        raise ValueError("expected seed set and script must be nonempty and unique")
    result = {}
    for expected_seed in seeds:
        seed = int(take(r"==== seed 0x([0-9a-f]{16}) ====")[0], 16)
        if seed != expected_seed:
            raise ValueError(f"seed order/set mismatch: expected {expected_seed:x}, got {seed:x}")
        repeated_seed = int(take(r"seed=0x([0-9a-f]{16})")[0], 16)
        if repeated_seed != seed:
            raise ValueError("repeated seed does not match block header")
        initial = int(take(r"initial_hash=0x([0-9a-f]{16})")[0], 16)
        records = []
        total = 0
        for step, (mask, ticks) in enumerate(script):
            fields = take(r"step=([0-9]{3}) mask=0x([0-9a-f]{2}) ticks=([0-9]+) total_ticks=([0-9]+) score=([0-9]+) over=([01]) hash=0x([0-9a-f]{16})")
            index, raw_mask, count, elapsed, score, over, digest = fields
            total += ticks
            if (int(index), int(raw_mask, 16), int(count), int(elapsed)) != (step, mask, ticks, total):
                raise ValueError(f"seed {seed:x} step {step}: script metadata mismatch")
            if int(score) > 2147483647:
                raise ValueError("score is outside the SimGame int32 contract")
            row = Record(step, mask, ticks, total, int(score), over == "1", int(digest, 16))
            records.append(row)
            if row.over:
                stopped = int(take(r"game_over_at_step=([0-9]+)")[0])
                if stopped != len(records):
                    raise ValueError("game-over marker does not match completed step count")
                break
        digest, score, over = take(r"final_hash=0x([0-9a-f]{16}) final_score=([0-9]+) final_over=([01])")
        final = (int(digest, 16), int(score), over == "1")
        last = records[-1]
        if final != (last.state_hash, last.score, last.over):
            raise ValueError("final summary does not match last step")
        result[seed] = Reference(seed, initial, tuple(records), *final)
    if position != len(lines):
        raise ValueError("unexpected extra reference records or seed blocks")
    return result


def compare_records(expected: list[tuple], actual: list[tuple]) -> str | None:
    """Report the first shared-row difference, then a possible prefix length difference."""
    fields = ("step", "total_ticks", "score", "game_over", "state_hash")
    for index, (left, right) in enumerate(zip(expected, actual)):
        if len(left) != len(fields) or len(right) != len(fields):
            return f"row {index}: invalid field count"
        for name, want, got in zip(fields, left, right):
            if want != got:
                return f"row {index} {name}: expected={want} actual={got}"
    if len(expected) != len(actual):
        return f"record count: expected={len(expected)} actual={len(actual)}"
    return None
