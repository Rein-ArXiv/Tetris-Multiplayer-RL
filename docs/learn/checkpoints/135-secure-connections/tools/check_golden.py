"""Read-only comparison and exclusive candidate capture for LRND traces."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

MAX_BYTES = 2 * 1024 * 1024
METADATA = ("format", "rules", "hash_format", "scenario", "seed")
ROW_KEYS = {"tick", "mask", "garbage", "step", "hash", "bytes"}

def fnv64(data: bytes) -> str:
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"{value:016x}"

def unique_object(pairs: list) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result

def validate_trace(trace: dict) -> None:
    if not isinstance(trace, dict) or set(trace) != {*METADATA, "records"}:
        raise ValueError("trace fields are incomplete or unknown")
    if any(not isinstance(trace[k], str) or not trace[k] for k in METADATA):
        raise ValueError("metadata must contain nonempty strings")
    if trace["format"] != "LRND-trace/1" or trace["hash_format"] != "LRND/1":
        raise ValueError("unsupported trace/hash format")
    if not re.fullmatch(r"0|[1-9][0-9]{0,19}", trace["seed"]) or int(trace["seed"]) >= 1 << 64:
        raise ValueError("seed must be a canonical uint64 decimal string")
    if trace["scenario"] not in {"mixed-v1", "overflow-v1"}:
        raise ValueError("unknown scenario")
    rows = trace["records"]
    if not isinstance(rows, list) or not 2 <= len(rows) <= 1000:
        raise ValueError("records must contain initial state and at least one tick, at most 1000")
    for index, row in enumerate(rows):
        if not isinstance(row, dict) or set(row) != ROW_KEYS:
            raise ValueError(f"record {index}: incomplete or unknown fields")
        for key, maximum in (("tick", 999), ("mask", 31), ("garbage", 20)):
            if type(row[key]) is not int or not 0 <= row[key] <= maximum:
                raise ValueError(f"record {index}: invalid {key}")
        if row["tick"] != index:
            raise ValueError(f"record {index}: noncontiguous tick")
        if not isinstance(row["step"], str) or row["step"] not in {"initial", "waiting", "changed", "locked", "game_over", "stopped"}:
            raise ValueError(f"record {index}: invalid step")
        if (index == 0) != (row["step"] == "initial") or (index == 0 and (row["mask"] or row["garbage"])):
            raise ValueError("initial record must have zero requests and step initial")
        if not isinstance(row["bytes"], str) or not re.fullmatch(r"(?:[0-9a-f]{2}){8,512}", row["bytes"]):
            raise ValueError(f"record {index}: invalid canonical byte string")
        data = bytes.fromhex(row["bytes"])
        if data[:8] != b"LRND\x01\0\0\0":
            raise ValueError(f"record {index}: wrong byte schema tag")
        if not isinstance(row["hash"], str) or not re.fullmatch(r"[0-9a-f]{16}", row["hash"]) or row["hash"] != fnv64(data):
            raise ValueError(f"record {index}: hash does not match bytes")


def parse_trace(data: bytes) -> dict:
    if len(data) > MAX_BYTES:
        raise ValueError("trace exceeds size limit")
    trace = json.loads(data.decode("utf-8"), object_pairs_hook=unique_object)
    validate_trace(trace)
    return trace


def compare_traces(expected: dict, actual: dict) -> str | None:
    """Return None when traces match, otherwise the first human-readable difference."""
    # Metadata fields are compared in the required order.
    for key in ("format", "rules", "hash_format", "scenario", "seed"):
        e, a = expected[key], actual[key]
        if e != a:
            return f"metadata {key}: expected={e!r} actual={a!r}"

    # zip covers only the shared prefix, so length drift is reported later.
    for i, (er, ar) in enumerate(zip(expected["records"], actual["records"])):
        # Input/position fields come before the textual step.
        for key in ("tick", "mask", "garbage", "step"):
            x, y = er[key], ar[key]
            if x != y:
                return f"records[{i}].{key}: expected={x!r} actual={y!r}"

        eb, ab = er["bytes"], ar["bytes"]
        if eb != ab:
            eb_raw, ab_raw = bytes.fromhex(eb), bytes.fromhex(ab)
            # Locate the first differing byte within the common prefix.
            for off, (xb, yb) in enumerate(zip(eb_raw, ab_raw)):
                if xb != yb:
                    return f"records[{i}].bytes offset={off} expected={xb:02x} actual={yb:02x}"
            # Identical prefix but different total length.
            return f"records[{i}].bytes length expected={len(eb_raw)} actual={len(ab_raw)}"

        # Equal payload bytes but different hashes indicate an inconsistent digest.
        if er["hash"] != ar["hash"]:
            return f"records[{i}].hash mismatch expected={er['hash']} actual={ar['hash']}"

    # Every shared row matched; a differing row count is the last possible gap.
    if len(expected["records"]) != len(actual["records"]):
        return f"records length expected={len(expected['records'])} actual={len(actual['records'])}"

    return None

def run_trace(executable: str, scenario: str, seed: str) -> tuple[dict, bytes]:
    # Check return code before parsing output; a failed emitter may leave a prefix.
    result = subprocess.run([str(Path(executable).resolve()), scenario, seed],
                            check=True, capture_output=True, timeout=10)
    return parse_trace(result.stdout), result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    check = commands.add_parser("check")
    check.add_argument("reference", type=Path)
    check.add_argument("executable")
    capture = commands.add_parser("capture")
    capture.add_argument("executable")
    capture.add_argument("candidate", type=Path)
    capture.add_argument("--scenario", choices=["mixed-v1", "overflow-v1"], default="mixed-v1")
    capture.add_argument("--seed", default="1")
    args = parser.parse_args()
    try:
        if args.command == "capture":
            actual, raw = run_trace(args.executable, args.scenario, args.seed)
            with args.candidate.open("xb") as output:  # Never overwrite a baseline or existing candidate.
                output.write(raw)
            print(f"candidate written: {args.candidate} ({len(actual['records'])} records); review before adoption")
            return 0
        with args.reference.open("rb") as source:
            expected = parse_trace(source.read(MAX_BYTES + 1))
        actual, _ = run_trace(args.executable, expected["scenario"], expected["seed"])
        difference = compare_traces(expected, actual)
        if difference:
            print(difference)
            return 1
        print(f"match: {len(actual['records'])} records, {actual['scenario']}, seed={actual['seed']}")
        return 0
    except (OSError, ValueError, TypeError, subprocess.SubprocessError) as error:
        print(f"cannot compare: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
