"""Guest credential migration, revocation, recovery and durable native handoff."""
from __future__ import annotations
import concurrent.futures
import contextlib
import hashlib
import json
import os
from pathlib import Path
import secrets
import socket
import sqlite3
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import pytest
from .test_meta_db_smoke import _find_meta_bin, _free_port, _post, _wait_listen
from .test_secure_admission import executable, SECRET


def digest(purpose, raw):
    return hashlib.sha256(f"tetris-{purpose}-v1:{raw}".encode()).hexdigest()


def replacements():
    return secrets.token_hex(16), "rc1." + secrets.token_hex(32)


@contextlib.contextmanager
def running_meta(db):
    binary = _find_meta_bin()
    if not binary:
        pytest.skip("meta not built")
    port = _free_port()
    process = subprocess.Popen([str(binary.resolve()), "--http", f"127.0.0.1:{port}", "--db", str(db), "--relay-secret", SECRET],
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        assert _wait_listen(port), "meta did not start"
        yield f"http://127.0.0.1:{port}"
    finally:
        process.terminate(); process.communicate(timeout=10)


@pytest.fixture
def account_api(tmp_path):
    db = tmp_path / "account.db"
    with running_meta(db) as base:
        status, guest = _post(base + "/v1/guest")
        assert status == 200
        yield base, db, guest


def change(base, operation, credential, token, recovery):
    return _post(base + "/v1/account/" + operation,
                 {"credential": credential, "next_token": token, "next_recovery": recovery})


def verify(base, token):
    return _post(base + "/v1/auth/verify", {"token": token})


def stored(db):
    with sqlite3.connect(db) as con:
        con.row_factory = sqlite3.Row
        return dict(con.execute("SELECT * FROM players ORDER BY id LIMIT 1").fetchone())


def test_new_accounts_store_only_hashes(account_api):
    base, db, guest = account_api
    row = stored(db)
    assert "token" not in row and row["token_hash"] == digest("account", guest["token"])
    assert row["recovery_hash"] is None and row["auth_epoch"] == 0
    assert verify(base, row["token_hash"])[0] == 404
    # A digest-shaped token or a recovery code cannot be submitted as account auth.
    assert verify(base, "rc1." + "a" * 64)[0] == 404
    for path in [db, Path(str(db) + "-wal")]:
        if path.exists(): assert guest["token"].encode() not in path.read_bytes()


def test_legacy_tokens_migrate_once_without_losing_progress(tmp_path):
    db = tmp_path / "legacy.db"
    token = secrets.token_hex(16)
    with sqlite3.connect(db) as con:
        con.executescript("""PRAGMA user_version=1;
            CREATE TABLE players(id INTEGER PRIMARY KEY,username TEXT,token TEXT UNIQUE NOT NULL,
              elo INTEGER NOT NULL DEFAULT 0,wins INTEGER NOT NULL DEFAULT 0,losses INTEGER NOT NULL DEFAULT 0,
              bp INTEGER NOT NULL DEFAULT 0,xp INTEGER NOT NULL DEFAULT 0,selected_icon_id TEXT NOT NULL DEFAULT 'default',
              created_at INTEGER NOT NULL);""")
        con.execute("INSERT INTO players VALUES(7,'player',?,123,4,2,80,900,'default',1234)", (token,))
    assert token.encode() in db.read_bytes()
    for _ in range(2):
        with running_meta(db) as base:
            status, profile = verify(base, token)
            assert status == 200 and profile["player_id"] == 7
            assert (profile["elo"], profile["bp"], profile["xp"]) == (123, 80, 900)
            assert stored(db)["token_hash"] == digest("account", token)
            for path in [db, Path(str(db) + "-wal")]:
                if path.exists(): assert token.encode() not in path.read_bytes()
    with sqlite3.connect(db) as con:
        restored = tmp_path / "restored.db"
        with sqlite3.connect(restored) as target: target.executescript("\n".join(con.iterdump()))
    with running_meta(restored) as base:
        assert verify(base, token)[1]["elo"] == 123


def test_invalid_legacy_credential_aborts_without_partial_hashing(tmp_path):
    binary = _find_meta_bin()
    if not binary:
        pytest.skip("meta not built")
    db = tmp_path / "invalid-legacy.db"
    good = secrets.token_hex(16)
    with sqlite3.connect(db) as con:
        con.executescript("""PRAGMA user_version=1;
            CREATE TABLE players(id INTEGER PRIMARY KEY,username TEXT,token TEXT UNIQUE NOT NULL,
                elo INTEGER NOT NULL DEFAULT 0,wins INTEGER NOT NULL DEFAULT 0,
                losses INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL);""")
        con.executemany("INSERT INTO players VALUES(?,NULL,?,10,1,0,1)",
                        [(1, good), (2, "damaged-legacy-key")])
    result = subprocess.run([str(binary.resolve()), "--http", f"127.0.0.1:{_free_port()}",
                             "--db", str(db), "--relay-secret", SECRET],
                            capture_output=True, timeout=10)
    assert result.returncode != 0
    assert good.encode() not in result.stdout + result.stderr
    with sqlite3.connect(db) as con:
        assert con.execute("SELECT token FROM players ORDER BY id").fetchall() == [(good,), ("damaged-legacy-key",)]
        assert con.execute("SELECT COUNT(*) FROM schema_migrations WHERE name='credential_hash_v1'").fetchone()[0] == 0


def test_rotation_revokes_old_key_and_outstanding_game_ticket(account_api):
    base, db, guest = account_api
    old = guest["token"]
    status, ticket = _post(base + "/v1/game-tickets", {"token": old})
    assert status == 200
    new, recovery = replacements()
    status, profile = change(base, "rotate", old, new, recovery)
    assert status == 200 and profile["player_id"] == guest["player_id"]
    assert verify(base, old)[0] == 404 and verify(base, new)[0] == 200
    assert _post(base + "/v1/game-tickets", {"token": old})[0] == 401
    assert _post(base + "/v1/game-tickets/consume", {"ticket": ticket["ticket"]}, headers={"X-Relay-Secret": SECRET})[0] == 401
    assert _post(base + "/v1/icons/select", {"token": old, "icon_id": "default"})[0] == 404
    row = stored(db)
    assert row["auth_epoch"] == 1 and row["recovery_hash"] == digest("recovery", recovery)
    assert old not in json.dumps(row) and new not in json.dumps(row) and recovery not in json.dumps(row)
    # Exact retry acknowledges the same operation and does not increment generation.
    assert change(base, "rotate", old, new, recovery)[0] == 200
    assert stored(db)["auth_epoch"] == 1
    # Retired key by itself cannot request a different replacement.
    assert change(base, "rotate", old, *replacements())[0] == 401


def test_recovery_preserves_wallet_and_consumes_old_backup_once(account_api):
    base, db, guest = account_api
    old = guest["token"]; _, code = replacements()
    with sqlite3.connect(db) as con:
        con.execute("UPDATE players SET bp=250,xp=1000,elo=123,wins=2,losses=1 WHERE id=?", (guest["player_id"],))
    assert change(base, "backup", old, old, code)[0] == 200
    new, next_code = replacements()
    status, profile = change(base, "recover", code, new, next_code)
    assert status == 200 and profile["player_id"] == guest["player_id"]
    assert (profile["bp"], profile["xp"], profile["elo"]) == (250, 1000, 123)
    assert verify(base, old)[0] == 404
    assert change(base, "recover", code, *replacements())[0] == 401
    assert change(base, "recover", code, new, next_code)[0] == 200  # exact acknowledged retry
    assert change(base, "recover", next_code, *replacements())[0] == 200
    assert change(base, "recover", code, new, next_code)[0] == 401  # old receipt also gone


def test_simultaneous_recovery_has_one_winner(account_api):
    base, db, guest = account_api
    _, code = replacements(); old = guest["token"]
    assert change(base, "backup", old, old, code)[0] == 200
    attempts = [replacements() for _ in range(8)]
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        results = list(pool.map(lambda pair: change(base, "recover", code, *pair)[0], attempts))
    assert results.count(200) == 1 and results.count(401) == 7
    assert stored(db)["auth_epoch"] == 2


def test_candidate_collision_rolls_back_and_rate_limit_applies(account_api):
    base, db, guest = account_api
    other = _post(base + "/v1/guest")[1]["token"]
    _, code = replacements()
    assert change(base, "rotate", guest["token"], other, code)[0] == 409
    assert verify(base, guest["token"])[0] == 200 and stored(db)["auth_epoch"] == 0
    for _ in range(9):
        assert change(base, "recover", "rc1." + "0" * 64, *replacements())[0] == 401
    assert change(base, "recover", "rc1." + "0" * 64, *replacements())[0] == 429


def scoped_folder(root, origin):
    value = 14695981039346656037
    for byte in origin.encode():
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return root / "Tetris" / "accounts" / f"{value:016x}"


def write_token(folder, base, token):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "account.json").write_text(json.dumps({"api_url": base, "token": token}))


def read_token(folder):
    return json.loads((folder / "account.json").read_text())["token"]


@pytest.fixture
def account_home(tmp_path, account_api):
    # Explicit profile override works on every OS, including SHGetFolderPath Windows.
    env = dict(os.environ, TETRIS_USER_DATA_ROOT=str(tmp_path))
    folder = scoped_folder(tmp_path, account_api[0])
    folder.mkdir(parents=True)
    return env, folder


def native(action, base, env):
    return subprocess.run([str(executable("wss_probe")), "--account", action, base], env=env, capture_output=True, timeout=12)


def test_native_backup_rotate_restore_and_private_files(account_api, account_home):
    base, _, guest = account_api; env, folder = account_home
    write_token(folder, base, guest["token"])
    result = native("backup", base, env)
    assert result.returncode == 0, result.stderr
    backup = json.loads((folder / "account-recovery.json").read_text())
    assert backup["api_url"] == base and backup["player_id"] == guest["player_id"]
    assert guest["token"].encode() not in result.stdout
    for name in ["account.json", "account-recovery.json"]:
        path = folder / name
        if os.name != "nt":
            assert path.stat().st_mode & 0o777 == 0o600
        else:
            script = "$a=Get-Acl -LiteralPath $env:TETRIS_TEST_FILE; if (-not $a.AreAccessRulesProtected) {exit 1}; " \
                "$me=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value; " \
                "foreach ($r in $a.Access) {$s=$r.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value; " \
                "if ($s -ne $me -and $s -ne 'S-1-5-18') {exit 2}}"
            subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                           env=dict(env, TETRIS_TEST_FILE=str(path)), check=True, capture_output=True)
    assert not (folder / "account-change.pending.json").exists()
    assert native("rotate", base, env).returncode == 0
    token = read_token(folder)
    assert verify(base, guest["token"])[0] == 404 and verify(base, token)[0] == 200
    (folder / "account.json").unlink()  # Simulate a lost installation, retaining the backup.
    assert native("recover", base, env).returncode == 0
    recovered = read_token(folder)
    assert recovered != token and verify(base, recovered)[1]["player_id"] == guest["player_id"]
    assert verify(base, token)[0] == 404


def test_native_pending_survives_local_finalize_failure(account_api, account_home):
    base, db, guest = account_api; env, _ = account_home
    class Proxy(BaseHTTPRequestHandler):
        break_save = True
        def log_message(self, *_): pass
        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            status, response = _post(base + self.path, body)
            if Proxy.break_save:
                Proxy.break_save = False
                # The preflight passed and the server committed; now local save fails.
                bad_destination.mkdir()
            data = json.dumps(response).encode()
            self.send_response(status); self.send_header("Content-Length", str(len(data)))
            self.end_headers(); self.wfile.write(data)
    proxy = ThreadingHTTPServer(("127.0.0.1", 0), Proxy)
    worker = threading.Thread(target=proxy.serve_forever, daemon=True); worker.start()
    url = f"http://127.0.0.1:{proxy.server_port}"
    folder = scoped_folder(Path(env["TETRIS_USER_DATA_ROOT"]), url)
    folder.mkdir(parents=True)
    bad_destination = folder / "account-recovery.json"
    try:
        write_token(folder, url, guest["token"])
        result = native("rotate", url, env)
        assert result.returncode == 8
        journal = json.loads((folder / "account-change.pending.json").read_text())
        assert verify(base, journal["next_token"])[0] == 200
        assert verify(base, guest["token"])[0] == 404
        assert read_token(folder) == guest["token"]
        epoch = stored(db)["auth_epoch"]
        bad_destination.rmdir()
        assert native("resume", url, env).returncode == 0
        assert read_token(folder) == journal["next_token"]
        assert stored(db)["auth_epoch"] == epoch
    finally:
        proxy.shutdown(); proxy.server_close(); worker.join(timeout=2)


def test_native_rejects_wrong_server_and_unwritable_journal(account_api, account_home):
    base, db, guest = account_api; env, folder = account_home
    write_token(folder, base, guest["token"])
    (folder / "account-recovery.json").write_text(json.dumps({"api_url": "https://another.example", "recovery_code": replacements()[1]}))
    assert native("recover", base, env).returncode == 9
    assert stored(db)["auth_epoch"] == 0
    (folder / "account-change.pending.json").mkdir()
    assert native("rotate", base, env).returncode == 8
    assert stored(db)["auth_epoch"] == 0


def test_native_cross_process_account_lock(account_api, account_home):
    base, db, guest = account_api; env, folder = account_home
    write_token(folder, base, guest["token"])
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                     wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.CreateFileW(str(folder / "account-change.lock"), 0xC0000000, 0, None, 4, 128, None)
        assert handle != ctypes.c_void_p(-1).value
        try:
            assert native("rotate", base, env).returncode == 8
            assert stored(db)["auth_epoch"] == 0
        finally:
            kernel.CloseHandle(handle)
    else:
        import fcntl
        with (folder / "account-change.lock").open("w") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            assert native("rotate", base, env).returncode == 8
            assert stored(db)["auth_epoch"] == 0

    assert native("rotate", base, env).returncode == 0


def test_native_retries_lost_response_without_changing_keys_twice(account_api, account_home):
    base, db, guest = account_api; env, folder = account_home
    class Proxy(BaseHTTPRequestHandler):
        drop = True
        def log_message(self, *_): pass
        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            status, response = _post(base + self.path, body)
            if Proxy.drop:
                Proxy.drop = False
                self.connection.shutdown(socket.SHUT_RDWR)
                self.connection.close(); return
            data = json.dumps(response).encode()
            self.send_response(status);self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)));self.end_headers();self.wfile.write(data)
    proxy = ThreadingHTTPServer(("127.0.0.1", 0), Proxy)
    worker = threading.Thread(target=proxy.serve_forever, daemon=True); worker.start()
    url = f"http://127.0.0.1:{proxy.server_port}"
    try:
        folder = scoped_folder(Path(env["TETRIS_USER_DATA_ROOT"]), url)
        write_token(folder, url, guest["token"])
        result = native("rotate", url, env)
        assert result.returncode == 8, result.stdout
        pending = json.loads((folder / "account-change.pending.json").read_text())
        assert stored(db)["auth_epoch"] == 1
        assert native("resume", url, env).returncode == 0
        assert read_token(folder) == pending["next_token"]
        assert stored(db)["auth_epoch"] == 1
    finally:
        proxy.shutdown(); proxy.server_close();worker.join(timeout=3)


def test_native_scopes_accounts_and_never_auto_sends_legacy_key(account_api, account_home):
    base, db, guest = account_api; env, folder = account_home
    root = Path(env["TETRIS_USER_DATA_ROOT"])
    (root / "Tetris" / "token").write_text(guest["token"])
    assert native("bootstrap", base, env).returncode == 9
    assert not (folder / "account.json").exists()
    assert native("import", base, env).returncode == 0
    assert read_token(folder) == guest["token"]
    # A second API gets neither the first server's credential nor an automatic legacy import.
    with running_meta(root / "other.db") as other:
        assert native("bootstrap", other, env).returncode == 9
        with sqlite3.connect(root / "other.db") as con:
            assert con.execute("SELECT COUNT(*) FROM players").fetchone()[0] == 0
        assert native("create", other, env).returncode == 0
        other_token = read_token(scoped_folder(root, other))
        assert other_token != guest["token"] and read_token(folder) == guest["token"]


def test_native_normalizes_origin_and_refuses_wrong_origin_in_file(account_api, account_home):
    base, _, guest = account_api; env, folder = account_home
    write_token(folder, "https://wrong.example", guest["token"])
    assert native("bootstrap", base, env).returncode == 9
    assert read_token(folder) == guest["token"]
    a = native("paths", "https://EXAMPLE.com:443/", env)
    b = native("paths", "https://example.com", env)
    assert a.stdout == b.stdout
    for bad in [base + "/hidden", base + "?query", "https://user@example.com"]:
        assert native("bootstrap", bad, env).returncode == 9


@pytest.mark.parametrize("body", [b'{"token":"abc', b'{"n":123garbage}', b'[]',
    b'{"token":"a","token":"b"}', b'{"token":1,"toke\\u006e":2}',
    ('{"a":' * 25 + '{}' + '}' * 25).encode()])
def test_api_rejects_invalid_json_before_account_mutation(account_api, body):
    import urllib.request
    import urllib.error
    base, db, _ = account_api
    req = urllib.request.Request(base + "/v1/account/rotate", data=body,
                                 headers={"Content-Type": "application/json"})
    with pytest.raises(urllib.error.HTTPError) as error:
        urllib.request.urlopen(req, timeout=5)
    assert error.value.code == 400
    assert stored(db)["auth_epoch"] == 0


def test_initial_guest_save_failure_retries_same_key_without_second_guest(tmp_path):
    class Api(BaseHTTPRequestHandler):
        guests = 0
        def log_message(self, *_): pass
        def do_POST(self):
            self.rfile.read(int(self.headers["Content-Length"]))
            if self.path == "/v1/guest":
                Api.guests += 1
                (folder / "account.json").mkdir()
            data = json.dumps({"player_id": 1, "token": "a" * 32, "elo": 0, "bp": 0,
                               "xp": 0, "selected_icon_id": "default"}).encode()
            self.send_response(200); self.send_header("Content-Length", str(len(data)))
            self.end_headers(); self.wfile.write(data)
    server = ThreadingHTTPServer(("127.0.0.1", 0), Api)
    url = f"http://127.0.0.1:{server.server_port}"
    folder = scoped_folder(tmp_path, url)
    worker = threading.Thread(target=server.serve_forever, daemon=True); worker.start()
    env = dict(os.environ, TETRIS_USER_DATA_ROOT=str(tmp_path))
    process = subprocess.Popen([str(executable("wss_probe")), "--account", "bootstrap-retry", url],
                               env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        assert process.stdout.readline().strip() == b"save-required"
        (folder / "account.json").rmdir()
        out, err = process.communicate(b"\n", timeout=10)
        assert process.returncode == 0, (out, err)
        assert Api.guests == 1 and read_token(folder) == "a" * 32
    finally:
        if process.poll() is None: process.kill(); process.communicate()
        server.shutdown(); server.server_close(); worker.join(timeout=3)


def test_native_supports_unicode_profile_root(account_api, account_home):
    base, _, _ = account_api
    env, _ = account_home
    root = Path(env["TETRIS_USER_DATA_ROOT"]) / "한글 프로필"
    env = dict(env, TETRIS_USER_DATA_ROOT=str(root))
    assert native("bootstrap", base, env).returncode == 0
    folder = scoped_folder(root, base)
    token = read_token(folder)
    assert verify(base, token)[0] == 200
    assert native("backup", base, env).returncode == 0
    assert (folder / "account-recovery.json").is_file()
