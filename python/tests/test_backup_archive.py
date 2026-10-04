"""Linux archive publication/retention over the actual Python/SQLite helper."""
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tarfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/backup_meta_db.sh'
pytestmark = pytest.mark.skipif(sys.platform != 'linux', reason='Linux GNU/flock wrapper')


def source_db(tmp_path):
    source = tmp_path / "source '한국어'.db"
    with sqlite3.connect(source) as db:
        db.execute('CREATE TABLE marker (value TEXT)')
        db.execute("INSERT INTO marker VALUES ('synthetic')")
    return source


def archive(source, folder, keep='1', extra_env=None):
    env = dict(os.environ, KEEP=keep)
    if extra_env: env.update(extra_env)
    return subprocess.run(['bash', str(SCRIPT), str(source), str(folder)],
                          capture_output=True, text=True, env=env, timeout=15)


def test_real_archive_and_exact_retention_scope(tmp_path):
    source = source_db(tmp_path)
    folder = tmp_path / 'output space\nand newline'
    folder.mkdir()
    old = folder / 'tetris-20000101T000000Z-1.tar.gz'
    old.write_bytes(b'old')
    unrelated = folder / 'tetris-my-notes.tar.gz'
    unrelated.write_bytes(b'keep unrelated')
    linked = folder / 'tetris-20000101T000000Z-2.tar.gz'
    linked.symlink_to(unrelated)
    result = archive(source, folder, '01')
    assert result.returncode == 0, result.stderr
    assert not old.exists() and linked.is_symlink()
    assert unrelated.read_bytes() == b'keep unrelated'
    files = [p for p in folder.glob('tetris-*.tar.gz') if p not in (linked, unrelated)]
    assert len(files) == 1
    with tarfile.open(files[0]) as bundle:
        members = bundle.getmembers()
        assert len(members) == 1 and members[0].isfile()
        data = bundle.extractfile(members[0]).read()
    restored = tmp_path / 'restored.db'
    restored.write_bytes(data)
    with sqlite3.connect(restored) as db:
        assert db.execute('SELECT value FROM marker').fetchall() == [('synthetic',)]
    assert not list(folder.glob('.tetris-stage.*'))


@pytest.mark.parametrize('keep', ['0', '000', '-1', 'bad', '999999999999999999999'])
def test_bad_keep_cannot_delete_or_create_archives(tmp_path, keep):
    source = source_db(tmp_path)
    folder = tmp_path / 'backups'
    folder.mkdir()
    old = folder / 'tetris-20000101T000000Z-1.tar.gz'
    old.write_bytes(b'preserve')
    result = archive(source, folder, keep)
    assert result.returncode == 2
    assert list(folder.iterdir()) == [old] and old.read_bytes() == b'preserve'


def test_failed_tar_does_not_publish_or_prune(tmp_path):
    source = source_db(tmp_path)
    folder = tmp_path / 'backups'
    folder.mkdir()
    old = folder / 'tetris-20000101T000000Z-1.tar.gz'
    old.write_bytes(b'preserve')
    commands = tmp_path / 'commands'
    commands.mkdir()
    tar = commands / 'tar'
    tar.write_text('#!/bin/sh\nexit 9\n')
    tar.chmod(0o700)
    result = archive(source, folder, extra_env={'PATH':str(commands)+os.pathsep+os.environ['PATH']})
    assert result.returncode != 0
    assert old.read_bytes() == b'preserve'
    assert list(folder.glob('*.tar.gz')) == [old]
    assert not list(folder.glob('.tetris-stage.*'))


def test_busy_archive_lock_preserves_output(tmp_path):
    import fcntl
    source = source_db(tmp_path)
    folder = tmp_path / 'backups'
    folder.mkdir()
    with (folder / '.tetris-backup.lock').open('wb') as guard:
        fcntl.flock(guard, fcntl.LOCK_EX | fcntl.LOCK_NB)
        result = archive(source, folder)
        assert result.returncode == 1 and 'owns this directory' in result.stderr
        assert not list(folder.glob('*.tar.gz'))
