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


def test_missing_source_is_not_created(tmp_path):
    source = tmp_path / "missing.db"
    with pytest.raises(FileNotFoundError):
        backup.snapshot(source, tmp_path / "snapshot.db")
    assert not list(tmp_path.iterdir())


@pytest.mark.parametrize("suffix", ["-wal", "-shm", "-journal"])
def test_existing_sidecar_is_preserved_and_refused(tmp_path, suffix):
    source = tmp_path / "source.db"
    with sqlite3.connect(source) as db: db.execute("CREATE TABLE x (id INTEGER)")
    target = tmp_path / "target.db"
    sidecar = Path(str(target) + suffix)
    sidecar.write_bytes(b"preserve")
    with pytest.raises(FileExistsError): backup.snapshot(source, target)
    assert not target.exists() and sidecar.read_bytes() == b"preserve"


def test_dangling_destination_link_is_not_followed(tmp_path):
    source = tmp_path / "source.db"
    with sqlite3.connect(source) as db: db.execute("CREATE TABLE x (id INTEGER)")
    target = tmp_path / "target.db"
    elsewhere = tmp_path / "unexpected.db"
    try: target.symlink_to(elsewhere)
    except OSError: pytest.skip("symlink creation requires privileges on this host")
    with pytest.raises(FileExistsError): backup.snapshot(source, target)
    assert target.is_symlink() and not elsewhere.exists()


def test_structurally_valid_foreign_key_violation_is_not_published(tmp_path):
    source = tmp_path / "source.db"
    with sqlite3.connect(source) as db:
        db.executescript("CREATE TABLE p(id INTEGER PRIMARY KEY);"
            "CREATE TABLE c(id INTEGER REFERENCES p(id)); INSERT INTO c VALUES(9);")
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
    with pytest.raises(RuntimeError, match="foreign key"):
        backup.snapshot(source, tmp_path / "rejected.db")
    assert list(tmp_path.iterdir()) == [source]


def test_failed_publication_keeps_destination_winner(tmp_path, monkeypatch):
    source = tmp_path / "source.db"
    with sqlite3.connect(source) as db: db.execute("CREATE TABLE x (id INTEGER)")
    target = tmp_path / "target.db"
    real_link = backup.os.link
    def competing_link(src, dst):
        target.write_bytes(b"another publisher")
        return real_link(src, dst)
    monkeypatch.setattr(backup.os, "link", competing_link)
    with pytest.raises(FileExistsError): backup.snapshot(source, target)
    assert target.read_bytes() == b"another publisher"
    assert not list(tmp_path.glob(".tetris-backup-*"))


def test_locked_source_has_a_cooperative_deadline(tmp_path):
    source = tmp_path / "source.db"
    db = sqlite3.connect(source)
    try:
        db.execute("CREATE TABLE x (id INTEGER)")
        db.execute("BEGIN EXCLUSIVE")
        with pytest.raises((TimeoutError, sqlite3.OperationalError)):
            backup.snapshot(source, tmp_path / "target.db", timeout=0.1)
        assert not (tmp_path / "target.db").exists()
        assert not list(tmp_path.glob(".tetris-backup-*"))
    finally: db.close()


@pytest.mark.parametrize("timeout", [0, -1, float('inf'), float('nan')])
def test_bad_deadline_changes_no_files(tmp_path, timeout):
    with pytest.raises(ValueError): backup.snapshot(tmp_path / 'source', tmp_path / 'dest', timeout=timeout)
    assert not list(tmp_path.iterdir())


@pytest.mark.skipif(__import__('os').name != 'posix', reason='POSIX directory fsync')
@pytest.mark.parametrize('phase', ['file', 'directory'])
def test_sync_failure_preserves_phase_specific_publication(tmp_path, monkeypatch, phase):
    import stat
    source = tmp_path / 'source.db'
    with sqlite3.connect(source) as db: db.execute('CREATE TABLE marker (id INTEGER)')
    target = tmp_path / 'target.db'
    real_sync = backup.os.fsync
    def fail_sync(fd):
        is_directory = stat.S_ISDIR(backup.os.fstat(fd).st_mode)
        if is_directory == (phase == 'directory'): raise OSError('injected sync failure')
        return real_sync(fd)
    monkeypatch.setattr(backup.os, 'fsync', fail_sync)
    with pytest.raises(OSError, match='injected sync'):
        backup.snapshot(source, target)
    assert target.exists() == (phase == 'directory')
    if target.exists():
        with sqlite3.connect(target) as db:
            assert db.execute('PRAGMA integrity_check').fetchall() == [('ok',)]
    assert not list(tmp_path.glob('.tetris-backup-*'))


def test_product_restore_preserves_identity_and_rolls_back_revocation(tmp_path):
    """Exercise actual HTTP authentication, not just SQLite page readability."""
    import secrets
    from .meta_process import local_meta_server
    from .test_meta_db_smoke import _find_meta_bin, _post
    binary = _find_meta_bin()
    if binary is None: pytest.skip('meta not built')
    secret = 'snapshot-drill-test-secret'
    source = tmp_path / 'live.db'
    early, latest = tmp_path / 'early.db', tmp_path / 'latest.db'
    with local_meta_server(binary, source, secret) as base:
        status, guest = _post(base + '/v1/guest')
        assert status == 200
        old = guest['token']
        backup.snapshot(source, early)
        new = secrets.token_hex(16)
        recovery = 'rc1.' + secrets.token_hex(32)
        status, changed = _post(base + '/v1/account/rotate', {
            'credential': old, 'next_token': new, 'next_recovery': recovery})
        assert status == 200 and changed['player_id'] == guest['player_id']
        assert _post(base + '/v1/auth/verify', {'token': old})[0] == 404
        backup.snapshot(source, latest)
    for label, snapshot, accepted, rejected in (
            ('latest-restore', latest, new, old), ('historical-restore', early, old, new)):
        restored = tmp_path / label / 'accounts.db'
        backup.snapshot(snapshot, restored)
        with local_meta_server(binary, restored, secret) as base:
            status, account = _post(base + '/v1/auth/verify', {'token': accepted})
            assert status == 200 and account['player_id'] == guest['player_id']
            assert _post(base + '/v1/auth/verify', {'token': rejected})[0] == 404
