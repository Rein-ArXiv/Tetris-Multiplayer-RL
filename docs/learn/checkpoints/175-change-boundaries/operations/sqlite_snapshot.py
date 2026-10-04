#!/usr/bin/env python3
"""Publish a checked SQLite online snapshot without replacing another file.

Use a trusted, application-owned directory with no concurrent destination user.
Copy the completed snapshot to a NEW restore directory. Never overlay a live DB.
An exception after publication may leave a complete destination: inspect it,
never assume failure means rollback. POSIX sync covers the immediate parent,
not newly created ancestors. Windows directory durability needs separate policy.
"""
import argparse
from contextlib import closing
import math
import os
from pathlib import Path
import sqlite3
import tempfile
import time


def _refuse_destination(destination: Path) -> None:
    # lexists rejects dangling symlinks too; do not resolve the final component.
    for path in (destination, *(Path(str(destination) + suffix)
                                for suffix in ('-wal', '-shm', '-journal'))):
        if os.path.lexists(path):
            raise FileExistsError(f"Snapshot destination or sidecar exists: {path}")


def snapshot(source: Path, destination: Path, *, timeout: float = 30.0) -> None:
    """Copy/validate with cooperative deadlines, then publish the closed file.

    Deadlines are checked between backup steps and SQLite VM progress callbacks;
    they do not preempt arbitrary filesystem I/O or promise a hard wall-clock cap.
    Logical/database checks do not certify application identity or revocations.
    """
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("timeout must be positive and finite")
    deadline = time.monotonic() + timeout
    source = Path(source).resolve(strict=True)
    requested = Path(destination).absolute()
    destination = requested.parent.resolve() / requested.name
    _refuse_destination(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)

    def check_deadline(*_):
        if time.monotonic() >= deadline:
            raise TimeoutError("Snapshot deadline exceeded")

    fd, name = tempfile.mkstemp(prefix=".tetris-backup-", dir=destination.parent)
    os.close(fd)  # owner-only mode on POSIX; Windows uses directory ACL policy.
    temporary = Path(name)
    try:
        with closing(sqlite3.connect(source.as_uri() + "?mode=ro", uri=True,
                                     timeout=min(timeout, 0.2))) as src:
            with closing(sqlite3.connect(temporary, timeout=min(timeout, 0.2))) as dst:
                src.backup(dst, pages=128, progress=check_deadline, sleep=0.02)
                dst.set_progress_handler(lambda: int(time.monotonic() >= deadline), 1000)
                # The published artifact must not depend on its temporary WAL.
                if dst.execute("PRAGMA journal_mode=DELETE").fetchone() != ('delete',):
                    raise RuntimeError("Snapshot journal conversion failed")
                if dst.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
                    raise RuntimeError("Snapshot integrity check failed")
                if dst.execute("PRAGMA foreign_key_check").fetchone() is not None:
                    raise RuntimeError("Snapshot foreign key check failed")
        check_deadline()
        with temporary.open('r+b') as file:
            os.fsync(file.fileno())
        _refuse_destination(destination)
        # Atomic, no-overwrite publication. Do not fall back to replace/copy.
        os.link(temporary, destination)
    finally:
        for suffix in ('', '-wal', '-shm', '-journal'):
            Path(str(temporary) + suffix).unlink(missing_ok=True)
    if os.name == 'posix':
        directory = os.open(destination.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    snapshot(args.source, args.destination, timeout=args.timeout)
    print(f"Snapshot ready: {args.destination} (integrity and foreign keys checked)")


if __name__ == "__main__":
    main()
