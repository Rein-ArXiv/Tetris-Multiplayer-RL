"""Smoke test: tetris_meta HTTP+SQLite endpoints.

Spawns ``tetris_meta`` as a subprocess on a fresh DB, exercises guest/auth,
icons, matches, and leaderboard endpoints end-to-end, and verifies RP/BP/XP
updates and ordering.

Skips if the ``tetris_meta`` binary is missing — set ``TETRIS_META_BIN`` env var
to override the search path.

Run::

    python -m pytest python/tests/test_meta_db_smoke.py -v
"""
from __future__ import annotations

import json
import os
import socket
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

import pytest


def _find_meta_bin() -> Path | None:
    env = os.environ.get("TETRIS_META_BIN")
    if env:
        p = Path(env)
        return p if p.exists() else None
    repo = Path(__file__).resolve().parents[2]
    candidates = [
        repo / "build-meta" / "Debug" / "tetris_meta.exe",
        repo / "build-meta" / "tetris_meta.exe",
        repo / "build-meta" / "tetris_meta",
        repo / "build" / "Debug" / "tetris_meta.exe",
        repo / "build" / "tetris_meta",
    ]
    for c in candidates:
        if c.exists():
            return c
    return None


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _wait_listen(port: int, timeout_s: float = 5.0) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def _post(url: str, body: dict | None = None, timeout: float = 5.0,
          headers: dict[str, str] | None = None) -> tuple[int, dict]:
    data = json.dumps(body or {}).encode()
    req_headers = {"Content-Type": "application/json"}
    if headers:
        req_headers.update(headers)
    req = urllib.request.Request(url, data=data,
                                  headers=req_headers,
                                  method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode() or "{}")


def _get(url: str, timeout: float = 5.0) -> tuple[int, list | dict]:
    req = urllib.request.Request(url, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode() or "{}")


@pytest.fixture
def meta_server(tmp_path):
    bin_path = _find_meta_bin()
    if not bin_path:
        pytest.skip("tetris_meta binary not built (set TETRIS_META_BIN to override)")
    port = _free_port()
    db = tmp_path / "test.db"
    proc = subprocess.Popen(
        [str(bin_path), "--db", str(db), "--http", f"127.0.0.1:{port}",
         "--allow-public-matches"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    try:
        if not _wait_listen(port, timeout_s=5.0):
            stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
            pytest.fail(f"tetris_meta did not listen on :{port}\n{stderr}")
        yield f"http://127.0.0.1:{port}"
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()


def test_guest_then_verify(meta_server):
    base = meta_server
    code, body = _post(f"{base}/v1/guest")
    assert code == 200
    assert "token" in body and len(body["token"]) == 32
    assert body["elo"] == 0          # RP 스케일: 0 시작 (meta/elo.h)
    assert body["xp"] == 0
    assert body["level"] == 1
    assert body["player_id"] >= 1
    token = body["token"]

    code, body = _post(f"{base}/v1/auth/verify", {"token": token})
    assert code == 200
    assert body["elo"] == 0
    assert body["xp"] == 0
    assert body["level"] == 1
    assert body["username"] is None

    code, body = _post(f"{base}/v1/auth/verify", {"token": "deadbeef" * 4})
    assert code == 404


def test_matches_secret_required_by_default(tmp_path):
    bin_path = _find_meta_bin()
    if not bin_path:
        pytest.skip("tetris_meta binary not built (set TETRIS_META_BIN to override)")
    port = _free_port()
    db = tmp_path / "strict.db"
    proc = subprocess.Popen(
        [str(bin_path), "--db", str(db), "--http", f"127.0.0.1:{port}"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    try:
        rc = proc.wait(timeout=3)
        stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
        assert rc != 0
        assert "refusing to start" in stderr
    finally:
        if proc.poll() is None:
            proc.kill()


def test_match_post_updates_elo(meta_server):
    base = meta_server
    _, p1 = _post(f"{base}/v1/guest")
    _, p2 = _post(f"{base}/v1/guest")

    # p1 wins; both at 0(RP), K=32, expected=0.5 → 승자 +16,
    # 패자는 0-16 이 바닥(0) 에 clamp 되어 delta 0.
    code, body = _post(f"{base}/v1/matches", {
        "player_a":   p1["player_id"], "player_b": p2["player_id"],
        "winner":     p1["player_id"],
        "score_a":    5000, "score_b": 3000,
        "lines_a":    20,   "lines_b": 12,
        "duration_s": 90,
    })
    assert code == 200
    assert body["a"]["delta"] == 16
    assert body["b"]["delta"] == 0          # 바닥 clamp
    assert body["a"]["elo_after"] == 16
    assert body["b"]["elo_after"] == 0

    # BP/XP 적립 — 승자 +30BP/+100XP, 패자 +10BP/+50XP.
    code, w = _post(f"{base}/v1/auth/verify", {"token": p1["token"]})
    assert code == 200
    assert w["bp"] == 30 and w["xp"] == 100 and w["level"] == 2
    code, l = _post(f"{base}/v1/auth/verify", {"token": p2["token"]})
    assert code == 200
    assert l["bp"] == 10 and l["xp"] == 50 and l["level"] == 1


def test_matches_requires_relay_secret_when_configured(tmp_path):
    bin_path = _find_meta_bin()
    if not bin_path:
        pytest.skip("tetris_meta binary not built (set TETRIS_META_BIN to override)")
    port = _free_port()
    db = tmp_path / "secret.db"
    secret = "test-relay-secret"
    proc = subprocess.Popen(
        [str(bin_path), "--db", str(db), "--http", f"127.0.0.1:{port}",
         "--relay-secret", secret],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    try:
        if not _wait_listen(port, timeout_s=5.0):
            stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
            pytest.fail(f"tetris_meta did not listen on :{port}\n{stderr}")
        base = f"http://127.0.0.1:{port}"
        _, p1 = _post(f"{base}/v1/guest")
        _, p2 = _post(f"{base}/v1/guest")
        payload = {
            "player_a": p1["player_id"], "player_b": p2["player_id"],
            "winner": p1["player_id"],
            "score_a": 1, "score_b": 0,
            "lines_a": 0, "lines_b": 0,
            "duration_s": 1,
        }

        code, body = _post(f"{base}/v1/matches", payload)
        assert code == 403
        assert body.get("error") == "forbidden"

        code, body = _post(f"{base}/v1/matches", payload,
                           headers={"X-Relay-Secret": secret})
        assert code == 200
        assert body["a"]["delta"] == 16   # 0(RP) 동률, K=32 → 승자 +16
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()


def test_draw_keeps_elo(meta_server):
    base = meta_server
    _, p1 = _post(f"{base}/v1/guest")
    _, p2 = _post(f"{base}/v1/guest")

    code, body = _post(f"{base}/v1/matches", {
        "player_a": p1["player_id"], "player_b": p2["player_id"],
        "winner": None,
        "score_a": 0, "score_b": 0, "lines_a": 0, "lines_b": 0,
        "duration_s": 5,
    })
    assert code == 200
    assert body["a"]["delta"] == 0 and body["b"]["delta"] == 0


def test_bad_request(meta_server):
    base = meta_server
    code, _ = _post(f"{base}/v1/matches", {"player_a": 1, "player_b": 2})
    assert code == 400

    code, _ = _post(f"{base}/v1/matches", {
        "player_a": 1, "player_b": 1, "winner": 1,
        "score_a": 0, "score_b": 0, "lines_a": 0, "lines_b": 0,
        "duration_s": 1,
    })
    assert code == 400


def test_winner_must_be_one_of_players(meta_server):
    base = meta_server
    _, p1 = _post(f"{base}/v1/guest")
    _, p2 = _post(f"{base}/v1/guest")
    _, p3 = _post(f"{base}/v1/guest")

    # winner 가 player_a/b 중 하나가 아니면 400.
    code, body = _post(f"{base}/v1/matches", {
        "player_a": p1["player_id"], "player_b": p2["player_id"],
        "winner":   p3["player_id"],   # 둘 다 아님
        "score_a": 1000, "score_b": 500,
        "lines_a": 5,    "lines_b": 3,
        "duration_s": 30,
    })
    assert code == 400
    assert "winner" in body.get("reason", "")

    # 음수 점수도 거부.
    code, _ = _post(f"{base}/v1/matches", {
        "player_a": p1["player_id"], "player_b": p2["player_id"],
        "winner":   p1["player_id"],
        "score_a": -1, "score_b": 0, "lines_a": 0, "lines_b": 0, "duration_s": 1,
    })
    assert code == 400


def test_unknown_token_returns_404_distinct_from_network_error(meta_server):
    """클라이언트의 stale-token 자동 재발급 정책의 핵심 전제:
    DB 가 살아있고 토큰이 잘못된 경우는 반드시 404 로 응답해야 한다.
    (메타 다운/타임아웃은 네트워크 레벨에서 이미 다른 시그널.)
    """
    base = meta_server
    code, body = _post(f"{base}/v1/auth/verify", {"token": "00" * 16})
    assert code == 404
    assert body.get("error") == "unknown_token"

    # 빈 token 도 명시적 400 (server-side 검증).
    code, _ = _post(f"{base}/v1/auth/verify", {"token": ""})
    assert code == 400


def test_leaderboard_order(meta_server):
    base = meta_server
    # 3명 만들고 첫 번째가 두 번째에게 이김 → 첫 번째가 elo 높아야 함
    _, p1 = _post(f"{base}/v1/guest")
    _, p2 = _post(f"{base}/v1/guest")
    _, p3 = _post(f"{base}/v1/guest")
    _post(f"{base}/v1/matches", {
        "player_a": p1["player_id"], "player_b": p2["player_id"],
        "winner": p1["player_id"],
        "score_a": 1, "score_b": 0, "lines_a": 0, "lines_b": 0,
        "duration_s": 1,
    })

    code, rows = _get(f"{base}/v1/leaderboard?limit=10")
    assert code == 200
    assert isinstance(rows, list) and len(rows) == 3
    assert rows[0]["player_id"] == p1["player_id"]
    assert rows[0]["elo"] > rows[1]["elo"]
    # 승자 16, 패자는 0 에서 바닥 clamp 으로 0, 미참가자도 0.
    elos = sorted([r["elo"] for r in rows], reverse=True)
    assert elos == [16, 0, 0]
    assert all("level" in r for r in rows)


def test_icons_catalog(meta_server):
    base = meta_server
    code, rows = _get(f"{base}/v1/icons/catalog")
    assert code == 200
    ids = {r["id"]: r for r in rows}
    assert {"default", "ruby", "gold"} <= set(ids)
    assert ids["default"]["default_owned"] is True
    assert ids["ruby"]["price_bp"] == 100
    assert ids["gold"]["price_bp"] == 250


def test_icons_buy_and_select_flow(meta_server):
    base = meta_server
    _, p1 = _post(f"{base}/v1/guest")
    _, p2 = _post(f"{base}/v1/guest")
    tok = p1["token"]

    # 신규 guest 는 BP 0 → ruby(100) 구매 불가(402), 미보유 select 불가(403).
    code, body = _post(f"{base}/v1/icons/buy", {"token": tok, "icon_id": "ruby"})
    assert code == 402 and body.get("error") == "insufficient_bp"
    code, body = _post(f"{base}/v1/icons/select", {"token": tok, "icon_id": "ruby"})
    assert code == 403 and body.get("error") == "not_owned"

    # 매치 4승 → 120 BP 적립 (kBpWin=30).
    for _ in range(4):
        _post(f"{base}/v1/matches", {
            "player_a": p1["player_id"], "player_b": p2["player_id"],
            "winner": p1["player_id"],
            "score_a": 1, "score_b": 0, "lines_a": 0, "lines_b": 0,
            "duration_s": 1,
        })
    code, w = _post(f"{base}/v1/auth/verify", {"token": tok})
    assert code == 200 and w["bp"] == 120

    # ruby 구매 성공 → BP 20 차감 후 잔액.
    code, body = _post(f"{base}/v1/icons/buy", {"token": tok, "icon_id": "ruby"})
    assert code == 200 and body["bp"] == 20

    # 중복 구매는 409.
    code, body = _post(f"{base}/v1/icons/buy", {"token": tok, "icon_id": "ruby"})
    assert code == 409 and body.get("error") == "already_owned"

    # 보유했으니 select 성공 → selected_icon_id 반영.
    code, body = _post(f"{base}/v1/icons/select", {"token": tok, "icon_id": "ruby"})
    assert code == 200 and body["selected_icon_id"] == "ruby"

    # 존재하지 않는 아이콘은 400.
    code, body = _post(f"{base}/v1/icons/buy", {"token": tok, "icon_id": "nope"})
    assert code == 400 and body.get("error") == "invalid_icon"


def test_bp_xp_accrual(meta_server):
    base = meta_server
    _, winner = _post(f"{base}/v1/guest")
    _, loser = _post(f"{base}/v1/guest")
    _post(f"{base}/v1/matches", {
        "player_a": winner["player_id"], "player_b": loser["player_id"],
        "winner": winner["player_id"],
        "score_a": 9, "score_b": 1, "lines_a": 2, "lines_b": 0,
        "duration_s": 9,
    })
    # 승자 +30 BP / +100 XP → level 2; 패자 +10 BP / +50 XP → level 1.
    _, w = _post(f"{base}/v1/auth/verify", {"token": winner["token"]})
    assert w["bp"] == 30 and w["xp"] == 100 and w["level"] == 2
    _, l = _post(f"{base}/v1/auth/verify", {"token": loser["token"]})
    assert l["bp"] == 10 and l["xp"] == 50 and l["level"] == 1


def test_elo_rp_migration_on_legacy_db(tmp_path):
    """구 ELO 스케일(1200 시작) DB 를 새 서버로 열면 RP(0 시작)로 1회 이관된다."""
    import sqlite3

    bin_path = _find_meta_bin()
    if not bin_path:
        pytest.skip("tetris_meta binary not built (set TETRIS_META_BIN to override)")

    db = tmp_path / "legacy.db"
    # user_version=0 + 구 스키마(elo 1200 기준, xp/bp/selected_icon_id 없음) 시드.
    # 새 서버가 ALTER 로 누락 컬럼을 붙이고 1회 리베이스를 적용해야 한다.
    con = sqlite3.connect(str(db))
    con.executescript(
        """
        PRAGMA user_version = 0;
        CREATE TABLE players (
          id INTEGER PRIMARY KEY, username TEXT, token TEXT UNIQUE NOT NULL,
          elo INTEGER NOT NULL DEFAULT 1200, wins INTEGER NOT NULL DEFAULT 0,
          losses INTEGER NOT NULL DEFAULT 0, created_at INTEGER NOT NULL);
        INSERT INTO players(id,username,token,elo,wins,losses,created_at)
          VALUES (1, NULL, 'tokenhi', 1500, 3, 1, 0);
        INSERT INTO players(id,username,token,elo,wins,losses,created_at)
          VALUES (2, NULL, 'tokenlo', 1100, 0, 2, 0);
        CREATE TABLE matches (
          id INTEGER PRIMARY KEY, player_a INTEGER NOT NULL, player_b INTEGER NOT NULL,
          winner INTEGER, score_a INTEGER NOT NULL, score_b INTEGER NOT NULL,
          lines_a INTEGER NOT NULL, lines_b INTEGER NOT NULL,
          duration_s INTEGER NOT NULL, created_at INTEGER NOT NULL);
        INSERT INTO matches VALUES (1, 1, 2, 1, 10, 5, 4, 1, 60, 0);
        CREATE TABLE elo_history (
          id INTEGER PRIMARY KEY, player_id INTEGER NOT NULL, match_id INTEGER NOT NULL,
          elo_before INTEGER NOT NULL, elo_after INTEGER NOT NULL,
          delta INTEGER NOT NULL, created_at INTEGER NOT NULL);
        INSERT INTO elo_history VALUES (1, 1, 1, 1490, 1500, 10, 0);
        INSERT INTO elo_history VALUES (2, 2, 1, 1110, 1100, -10, 0);
        """
    )
    con.commit()
    con.close()

    def _run_once(db_path):
        port = _free_port()
        proc = subprocess.Popen(
            [str(bin_path), "--db", str(db_path), "--http", f"127.0.0.1:{port}",
             "--allow-public-matches"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        if not _wait_listen(port, timeout_s=5.0):
            stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
            proc.kill()
            pytest.fail(f"tetris_meta did not listen on :{port}\n{stderr}")
        try:
            base = f"http://127.0.0.1:{port}"
            _, hi = _post(f"{base}/v1/auth/verify", {"token": "tokenhi"})
            _, lo = _post(f"{base}/v1/auth/verify", {"token": "tokenlo"})
            return hi, lo
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()

    # 1차 기동: 1500 → 300, 1100 → max(0,-100) = 0. xp/level 은 새 컬럼 기본값.
    hi, lo = _run_once(db)
    assert hi["elo"] == 300 and hi["xp"] == 0 and hi["level"] == 1
    assert lo["elo"] == 0

    con = sqlite3.connect(str(db))
    history = con.execute(
        "SELECT elo_before,elo_after,delta FROM elo_history ORDER BY id"
    ).fetchall()
    marker = con.execute(
        "SELECT name FROM schema_migrations WHERE name='elo_to_rp_v1'"
    ).fetchone()
    con.close()
    assert history == [(290, 300, 10), (0, 0, 0)]
    assert marker == ("elo_to_rp_v1",)

    # 2차 기동: marker 가 있으므로 이중 리베이스가 일어나지 않아야 한다.
    hi2, lo2 = _run_once(db)
    assert hi2["elo"] == 300 and lo2["elo"] == 0

    # sqlite3 .dump/restore 는 PRAGMA user_version 을 보존하지 않는다. DB 안의
    # migration marker 는 보존되므로 복원본도 다시 리베이스하면 안 된다.
    con = sqlite3.connect(str(db))
    dump_sql = "\n".join(con.iterdump())
    con.close()
    restored = tmp_path / "restored.db"
    con = sqlite3.connect(str(restored))
    con.executescript(dump_sql)
    assert con.execute("PRAGMA user_version").fetchone()[0] == 0
    con.close()

    hi3, lo3 = _run_once(restored)
    assert hi3["elo"] == 300 and lo3["elo"] == 0
