#!/usr/bin/env python3
"""Portable SQLite online snapshot for Linux -> Windows server handover.

Usage: python scripts/backup_meta_db.py SOURCE.db SNAPSHOT.db
The output must not exist. Copy only the completed snapshot to the standby host.
"""
import argparse
from contextlib import closing
import os
from pathlib import Path
import sqlite3
import tempfile


def snapshot(source: Path, destination: Path) -> None:
    source = source.resolve(strict=True)
    destination = destination.resolve()
    if destination.exists():
        raise FileExistsError(f"Snapshot already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=".tetris-backup-", dir=destination.parent)
    os.close(fd)  # mkstemp uses owner-only permissions on POSIX.
    temporary = Path(name)
    try:
        with closing(sqlite3.connect(source.as_uri() + "?mode=ro", uri=True)) as src:
            with closing(sqlite3.connect(temporary)) as dst:
                src.backup(dst)
                if dst.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
                    raise RuntimeError("Snapshot integrity check failed")
        # Same-directory hard link publishes atomically and refuses an existing file.
        os.link(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    snapshot(args.source, args.destination)
    print(f"Snapshot ready: {args.destination} (integrity_check=ok)")


if __name__ == "__main__":
    main()
