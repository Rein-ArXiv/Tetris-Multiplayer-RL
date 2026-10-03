"""Live PvE proof and exactly-once BP tests. No training or model dependency."""
from pathlib import Path
import os
import subprocess
import sqlite3
import time
import pytest
from .test_meta_db_smoke import _find_meta_bin, _free_port, _wait_listen, _post, meta_server


@pytest.fixture
def server(tmp_path):
    binary = _find_meta_bin()
    if not binary:
        pytest.skip("tetris_meta not built")
    binary = binary.resolve()
    (tmp_path / "assets").mkdir()
    # A deliberately fast opponent tops out quickly; the normal roster is slower.
    (tmp_path / "assets/opponents.cfg").write_text("rush|Rush|@heuristic|||Test|1|0|1\n")
    port = _free_port()
    process = subprocess.Popen([str(binary), "--db", str(tmp_path / "meta.db"),
                               "--http", f"127.0.0.1:{port}", "--relay-secret", "test-secret", "--bot-rewards"],
                              cwd=tmp_path, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        assert _wait_listen(port), process.poll()
        url = f"http://127.0.0.1:{port}"
        def guest():
            status, body = _post(url + "/v1/guest")
            assert status == 200
            return body["token"]
        yield url, guest(), guest(), binary, tmp_path / "meta.db"
    finally:
        process.terminate()
        process.communicate(timeout=10)


def start(url, token):
    status, body = _post(url + "/v1/bots/challenge", {"token": token, "opponent_id": "rush"})
    assert status == 200, body
    assert (body["input_ticks"], body["think_ticks"], body["min_piece_ticks"]) == (1, 0, 1)
    return body


def test_verified_win_retry_and_ownership(server):
    url, token, other, binary, _ = server
    helper = binary.with_name("bot_replay_test" + binary.suffix)
    assert helper.exists(), "build the bot_replay_test target"
    began = time.monotonic()
    challenge = start(url, token)
    replay = subprocess.check_output([str(helper), str(challenge["seed"])], text=True).strip()
    payload = {"token": token, "ticket": challenge["ticket"], "inputs_hex": replay}
    assert _post(url + "/v1/bots/claim", dict(payload, token=other))[0] == 403
    assert _post(url + "/v1/bots/claim", payload)[0] == 409  # sped-up proof
    time.sleep(max(0, len(replay) / 120 - (time.monotonic() - began)) + 0.1)
    status, result = _post(url + "/v1/bots/claim", payload, timeout=10)
    assert status == 200, result
    assert result["awarded_bp"] == 10
    # Reply loss/retry remains idempotent even with missing proof on retry.
    status, retry = _post(url + "/v1/bots/claim", {"token": token, "ticket": challenge["ticket"]})
    assert status == 200 and retry == result
    assert _post(url + "/v1/bots/claim", dict(payload, token=other))[0] == 403


def test_forgery_supersession_and_rate_limit(server):
    url, token, other, _, _ = server
    assert _post(url + "/v1/bots/challenge", {"token": "fake", "opponent_id": "rush"})[0] == 401
    first = start(url, token)
    second = start(url, token)
    base = {"token": token, "ticket": second["ticket"]}
    assert _post(url + "/v1/bots/claim", dict(base, ticket=first["ticket"], inputs_hex="00"))[0] == 403
    assert _post(url + "/v1/bots/claim", dict(base, winner=True, score=999999))[0] == 400
    assert _post(url + "/v1/bots/claim", dict(base, inputs_hex="ff"))[0] == 400
    assert _post(url + "/v1/bots/claim", dict(base, inputs_hex="00" * 30001))[0] == 400
    assert _post(url + "/v1/bots/claim", dict(base, inputs_hex="00"))[0] == 422
    assert _post(url + "/v1/bots/claim", dict(base, inputs_hex="00"))[0] == 403
    start(url, token)
    start(url, other)
    assert _post(url + "/v1/bots/challenge", {"token": token, "opponent_id": "rush"})[0] == 429


def test_rewards_disabled_by_default(meta_server):
    # Existing deployments opt in only after installing their official catalog.
    assert _post(meta_server + "/v1/bots/challenge", {"token": "x", "opponent_id": "rush"})[0] == 404


@pytest.mark.parametrize("earned_today,overflow", [(95, False), (100, False), (0, True)])
def test_daily_receipts_zero_award_and_atomic_failure(server, earned_today, overflow):
    url, token, _, binary, db_path = server
    helper = binary.with_name("bot_replay_test" + binary.suffix)
    began = time.monotonic()
    challenge = start(url, token)
    replay = subprocess.check_output([str(helper), str(challenge["seed"])], text=True).strip()
    with sqlite3.connect(db_path) as db:
        # This isolated server creates our account first. Direct DB edits are fault fixtures.
        player = db.execute("SELECT id FROM players ORDER BY id LIMIT 1").fetchone()[0]
        if earned_today:
            db.execute("INSERT INTO bot_rewards VALUES (?,?,?,?,?)",
                       ("f" * 32, player, "rush", earned_today, int(time.time())))
        before = 2147483647 if overflow else 0
        db.execute("UPDATE players SET bp=? WHERE id=?", (before, player))
    time.sleep(max(0, len(replay) / 120 - (time.monotonic() - began)) + 0.1)
    payload = {"token": token, "ticket": challenge["ticket"], "inputs_hex": replay}
    status, body = _post(url + "/v1/bots/claim", payload, timeout=10)
    if overflow:
        assert status == 503, body
        with sqlite3.connect(db_path) as db:
            assert db.execute("SELECT count(*) FROM bot_rewards WHERE ticket=?", (challenge["ticket"],)).fetchone()[0] == 0
            assert db.execute("SELECT bp FROM players WHERE id=?", (player,)).fetchone()[0] == before
            db.execute("UPDATE players SET bp=0 WHERE id=?", (player,))
        status, body = _post(url + "/v1/bots/claim", payload, timeout=10)
    expected = min(10, max(0, 100 - earned_today))
    assert status == 200 and body["awarded_bp"] == expected and body["bp"] == expected, body
    # A recorded zero is still a receipt; retry needs neither a live ticket nor replay.
    assert _post(url + "/v1/bots/claim", {"token": token, "ticket": challenge["ticket"]}) == (200, body)
    with sqlite3.connect(db_path) as db:
        assert db.execute("SELECT awarded_bp FROM bot_rewards WHERE ticket=?", (challenge["ticket"],)).fetchall() == [(expected,)]
        assert db.execute("SELECT bp FROM players WHERE id=?", (player,)).fetchone()[0] == expected
