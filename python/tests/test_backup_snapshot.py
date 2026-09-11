"""A live WAL database can be handed over without copying changing WAL files."""
import importlib.util
from pathlib import Path
import sqlite3

import pytest

spec = importlib.util.spec_from_file_location(
    "backup_meta_db", Path(__file__).resolve().parents[2] / "scripts/backup_meta_db.py")
backup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(backup)


def test_online_snapshot_preserves_committed_wal_and_refuses_overwrite(tmp_path):
    source = tmp_path / "live.db"
    target = tmp_path / "snapshot.db"
    db = sqlite3.connect(source)
    try:
        db.execute("PRAGMA journal_mode=WAL")
        db.execute("CREATE TABLE players (id INTEGER, token TEXT)")
        db.execute("INSERT INTO players VALUES (1, 'test-identity')")
        db.commit()
        backup.snapshot(source, target)
        with sqlite3.connect(target) as restored:
            assert restored.execute("SELECT * FROM players").fetchall() == [(1, "test-identity")]
            assert restored.execute("PRAGMA integrity_check").fetchone() == ("ok",)
        before = target.read_bytes()
        with pytest.raises(FileExistsError):
            backup.snapshot(source, target)
        assert target.read_bytes() == before
    finally:
        db.close()
