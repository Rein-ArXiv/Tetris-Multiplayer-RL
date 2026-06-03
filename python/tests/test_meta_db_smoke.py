"""Smoke test: tetris_meta HTTP+SQLite endpoints.

Spawns ``tetris_meta`` as a subprocess on a fresh DB, exercises the 4 endpoints
end-to-end, and verifies ELO math + leaderboard ordering.

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
    assert body["elo"] == 1200
    assert body["player_id"] >= 1
    token = body["token"]

    code, body = _post(f"{base}/v1/auth/verify", {"token": token})
    assert code == 200
    assert body["elo"] == 1200
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

    # p1 wins; both at 1200, K=24, expected=0.5 → +12 / -12
    code, body = _post(f"{base}/v1/matches", {
        "player_a":   p1["player_id"], "player_b": p2["player_id"],
        "winner":     p1["player_id"],
        "score_a":    5000, "score_b": 3000,
        "lines_a":    20,   "lines_b": 12,
        "duration_s": 90,
    })
    assert code == 200
    assert body["a"]["delta"] == 12
    assert body["b"]["delta"] == -12
    assert body["a"]["elo_after"] == 1212
    assert body["b"]["elo_after"] == 1188


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
        assert body["a"]["delta"] == 12
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
    # p3 는 한 판도 안 했으니 1200, p2 는 졌으니 1188.
    elos = sorted([r["elo"] for r in rows], reverse=True)
    assert elos == [1212, 1200, 1188]
