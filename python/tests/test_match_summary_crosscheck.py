"""Ranked results derive from replayed inputs, never agreement between claims.

Raw wire tests cover invented summaries, disconnects and a real deterministic
terminal game through both relay implementations. Authentication compatibility in
these historical fixtures does not disable gameplay validation.
"""
from __future__ import annotations

import json
import os
import socket
import sqlite3
import struct
import subprocess
import time
import urllib.request
from pathlib import Path

import pytest

from netbot.framing import FramingError, MsgType, build_frame, parse_frames

TEST_RELAY_SECRET = "test-relay-secret"


# ---- helpers (test_relay_meta_smoke.py 와 같은 패턴 — 코드 분리 우선) -----------

def _find_bin(name: str, env_var: str) -> Path | None:
    env = os.environ.get(env_var)
    if env:
        p = Path(env)
        return p if p.exists() else None
    repo = Path(__file__).resolve().parents[2]
    suffix = ".exe" if os.name == "nt" else ""
    base = name + suffix
    # Release 후보 추가 — 문서 권장 'cmake --build build --config Release' 는
    # Windows 멀티컨피그에서 build/Release/ (또는 build-meta/·build-relay/
    # 의 Release/) 아래에 exe 를 두는데, 이 경로가 빠져 있어 빌드해 두고도
    # 테스트가 조용히 skip 되고 있었다.
    for c in [
        repo / f"build-{name.split('_')[1]}" / "Release" / base,
        repo / f"build-{name.split('_')[1]}" / "Debug" / base,
        repo / f"build-{name.split('_')[1]}" / base,
        repo / "build" / "Release" / base,
        repo / "build" / "Debug" / base,
        repo / "build" / base,
    ]:
        if c.exists():
            return c
    return None


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # 0.0.0.0 으로 잡는다 — 릴레이가 INADDR_ANY 로 bind 하기 때문이다
        # (net/socket.cpp 의 tcp_listen). 127.0.0.1 로만 예약하면 그 주소에서만
        # 비어 있는 번호를 받을 수 있고, 릴레이는 모든 주소에서 그 번호를 잡아야 하니
        # bind 가 실패한다. 코드가 틀려서가 아니라 예약한 범위가 달라서 나는 실패라
        # 무관한 테스트가 빨갛게 된다. (meta 는 --http HOST:PORT 로 127.0.0.1 에만
        # 붙으므로 이 사정이 없지만, 더 강한 예약이 해가 되지 않아 같은 함수를 쓴다.)
        s.bind(("0.0.0.0", 0))
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


def _post(url: str, body: dict | None = None) -> dict:
    req = urllib.request.Request(
        url, data=json.dumps(body or {}).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=5.0) as r:
        return json.loads(r.read().decode())


def _qjoin(token: str) -> bytes:
    return build_frame(MsgType.QUEUE_JOIN,
                       bytes([len(token)]) + token.encode("ascii"))


def _summary(won: int, my_score: int, my_lines: int,
             opp_score: int, opp_lines: int, dur_s: int = 60) -> bytes:
    payload = struct.pack("<BIIIII", won, my_score, my_lines,
                          opp_score, opp_lines, dur_s)
    return build_frame(MsgType.MATCH_SUMMARY, payload)


def _recv_until(sock: socket.socket, want_type: MsgType,
                timeout: float = 5.0) -> bytes | None:
    sock.settimeout(timeout)
    buf = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            return None
        buf.extend(chunk)
        try:
            frames = parse_frames(buf)
        except FramingError:
            # C++ 대응(parse_frames false → 호출자 close): relay 가 오버사이즈
            # 길이를 선언했다는 뜻. 버퍼는 parse_frames 가 이미 비웠으니
            # 소켓만 닫고 그대로 테스트 실패로 띄운다.
            sock.close()
            raise
        for t, p in frames:
            if t == want_type:
                return bytes(p)
    return None


def _await_match_found(sock: socket.socket, timeout: float = 5.0) -> bool:
    return _recv_until(sock, MsgType.MATCH_FOUND, timeout) is not None


def _queue_accept(a: socket.socket, b: socket.socket) -> None:
    """MATCH_FOUND 수신 후 수락 로비 통과용 — 양쪽 READY(1) 송신 + peer forward 수신 확인.

    송신만 하고 리턴하면 안 된다: 호출자가 곧바로 close 하는 테스트
    (몰수패 시나리오) 에서, 로비가 우리 READY 를 아직 처리하기 전에 EOF 를
    먼저 관측하면 몰수패가 아니라 로비 abort 경로로 빠지는 경합이 있었다.
    relay 는 각 READY(1) 을 소비하는 즉시 상대에게 forward 하므로, 양쪽에서
    forward 된 상대 READY(1) 를 수신 확인하면 "로비가 양쪽 READY 를 모두
    처리했고 매치가 forwarder 단계로 넘어간다" 가 보장된다 — 이후의 close
    는 결정적으로 게임 중 disconnect 로 취급된다.
    """
    a.sendall(build_frame(MsgType.READY, bytes([1])))
    b.sendall(build_frame(MsgType.READY, bytes([1])))
    ready_from_b = _recv_until(a, MsgType.READY)
    ready_from_a = _recv_until(b, MsgType.READY)
    assert ready_from_b == b"\x01" and ready_from_a == b"\x01", \
        "lobby must forward peer READY(1) to both sides"


def _spawn_meta(tmp_path, relay_secret: str | None = TEST_RELAY_SECRET):
    bin_ = _find_bin("tetris_meta", "TETRIS_META_BIN")
    if not bin_:
        pytest.skip("tetris_meta binary missing")
    port = _free_port()
    args = [str(bin_), "--db", str(tmp_path / "test.db"),
            "--http", f"127.0.0.1:{port}"]
    if relay_secret:
        args += ["--relay-secret", relay_secret]
    else:
        args += ["--allow-public-matches"]
    proc = subprocess.Popen(
        args,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if not _wait_listen(port, 5.0):
        proc.kill()
        pytest.fail("meta failed to listen")
    return proc, f"http://127.0.0.1:{port}"


def _spawn_relay(meta_url: str | None, relay_secret: str | None = TEST_RELAY_SECRET, binary=None):
    bin_ = binary or _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    if not bin_:
        pytest.skip("tetris_relay binary missing")
    port = _free_port()
    args = [str(bin_), "--port", str(port)]
    if meta_url:
        args += ["--meta", meta_url]
        if relay_secret:
            args += ["--meta-secret", relay_secret]
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if not _wait_listen(port, 5.0):
        proc.kill()
        pytest.fail("relay failed to listen")
    return proc, port


# ---- 테스트 ------------------------------------------------------------------


@pytest.fixture(params=["tetris_relay", "tetris_relay_reactor"])
def meta_relay(tmp_path, request):
    """meta + relay (meta 연동) 페어 띄움."""
    mp, mu = _spawn_meta(tmp_path)
    configured = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    build = Path(os.environ.get("TETRIS_SECURE_BUILD", str(configured.parent if configured else "build-secure")))
    binary = build / (request.param + (".exe" if os.name == "nt" else ""))
    if not binary.exists():
        mp.terminate(); mp.wait(timeout=3)
        pytest.skip(f"{binary} not built")
    rp, rport = _spawn_relay(mu, binary=binary.resolve())
    try:
        yield {"meta_url": mu, "relay_port": rport, "db": tmp_path / "test.db", "build": build}
    finally:
        for proc in (rp, mp):
            proc.terminate()
            try: proc.wait(timeout=3)
            except subprocess.TimeoutExpired: proc.kill()


def _consistent_summaries(my1_score=5000, my1_lines=20,
                          my2_score=3000, my2_lines=10):
    """둘 다 'A 가 이김' 으로 일치하고 score/lines 도 교차 일치."""
    a_summary = _summary(won=1,
                         my_score=my1_score, my_lines=my1_lines,
                         opp_score=my2_score, opp_lines=my2_lines)
    b_summary = _summary(won=0,
                         my_score=my2_score, my_lines=my2_lines,
                         opp_score=my1_score, opp_lines=my1_lines)
    return a_summary, b_summary


def test_matching_fabricated_summaries_earn_nothing(meta_relay):
    base   = meta_relay["meta_url"]
    rport  = meta_relay["relay_port"]
    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    a = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    b = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    try:
        a.sendall(_qjoin(p1["token"]))
        b.sendall(_qjoin(p2["token"]))
        assert _await_match_found(a)
        assert _await_match_found(b)
        _queue_accept(a, b)

        sa, sb = _consistent_summaries()
        a.sendall(sa)
        b.sendall(sb)

        # MATCH_RESULT 양쪽 도착
        ra = _recv_until(a, MsgType.MATCH_RESULT)
        rb = _recv_until(b, MsgType.MATCH_RESULT)
        assert ra is not None and rb is not None
        assert struct.unpack_from("<i", ra, 8)[0] == 0
        assert struct.unpack_from("<i", rb, 8)[0] == 0
        assert ra[12] == rb[12] == 3  # Incomplete, never simulated a game.
        for player in (p1, p2):
            profile = _post(f"{base}/v1/auth/verify", {"token": player["token"]})
            assert profile["bp"] == profile["xp"] == 0
    finally:
        a.close(); b.close()


def test_inconsistent_summaries_no_elo(meta_relay):
    base   = meta_relay["meta_url"]
    rport  = meta_relay["relay_port"]
    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    a = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    b = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    try:
        a.sendall(_qjoin(p1["token"]))
        b.sendall(_qjoin(p2["token"]))
        assert _await_match_found(a)
        assert _await_match_found(b)
        _queue_accept(a, b)

        # 둘 다 "내가 이겼다" 주장 → exclusive_win 실패.
        a_summary = _summary(won=1, my_score=5000, my_lines=20,
                             opp_score=3000, opp_lines=10)
        b_summary = _summary(won=1, my_score=3000, my_lines=10,
                             opp_score=5000, opp_lines=20)
        a.sendall(a_summary); b.sendall(b_summary)

        ra = _recv_until(a, MsgType.MATCH_RESULT)
        rb = _recv_until(b, MsgType.MATCH_RESULT)
        assert ra is not None and rb is not None
        delta_a = struct.unpack_from("<i", ra, 8)[0]
        delta_b = struct.unpack_from("<i", rb, 8)[0]
        assert delta_a == 0 and delta_b == 0
    finally:
        a.close(); b.close()


def test_score_mismatch_no_elo(meta_relay):
    """A 가 '내가 봤을 때 상대 score 가 X' 라고 하는데 B 는 '내 진짜 score 가 X 아님'."""
    base   = meta_relay["meta_url"]
    rport  = meta_relay["relay_port"]
    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    a = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    b = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    try:
        a.sendall(_qjoin(p1["token"]))
        b.sendall(_qjoin(p2["token"]))
        assert _await_match_found(a)
        assert _await_match_found(b)
        _queue_accept(a, b)

        # exclusive_win 통과, 라인수 일치, 점수 불일치 (조작 시도).
        a_summary = _summary(won=1, my_score=99999, my_lines=20,
                             opp_score=3000, opp_lines=10)
        b_summary = _summary(won=0, my_score=3000, my_lines=10,
                             opp_score=5000, opp_lines=20)  # observed 5000, A claims 99999
        a.sendall(a_summary); b.sendall(b_summary)

        ra = _recv_until(a, MsgType.MATCH_RESULT)
        rb = _recv_until(b, MsgType.MATCH_RESULT)
        assert ra is not None and rb is not None
        # No terminal simulation exists, regardless of claimed scores.
        delta_a = struct.unpack_from("<i", ra, 8)[0]
        delta_b = struct.unpack_from("<i", rb, 8)[0]
        assert delta_a == 0 and delta_b == 0
    finally:
        a.close(); b.close()


def test_disconnect_with_unverified_survivor_claim_earns_nothing(meta_relay):
    base = meta_relay["meta_url"]
    rport = meta_relay["relay_port"]
    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    a = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    b = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    try:
        a.sendall(_qjoin(p1["token"]))
        b.sendall(_qjoin(p2["token"]))
        assert _await_match_found(a)
        assert _await_match_found(b)
        _queue_accept(a, b)

        # A surviving client claim is not proof that a game finished.
        b.sendall(_summary(won=1, my_score=5000, my_lines=20,
                           opp_score=1000, opp_lines=3))
        # MATCH_SUMMARY 는 relay 가 가로채 forward 하지 않으므로 처리 완료를
        # 직접 관측할 수 없다. 같은 소켓으로 뒤이어 보낸 게임 프레임(PING)은
        # forward 되므로, 그것이 a 에 도착했다면 TCP/포워더의 순서 보존에
        # 의해 요약도 이미 소비·기록된 것이다 — 그 뒤에 close 해야 요약
        # 도착 전에 forwarder 가 내려가는 경합 없이 몰수패가 결정적이다.
        b.sendall(build_frame(MsgType.PING, b"\x00" * 8))
        assert _recv_until(a, MsgType.PING) is not None

        a.close()
        result = _recv_until(b, MsgType.MATCH_RESULT)
        assert result is not None
        assert struct.unpack_from("<i", result, 8)[0] == 0
        assert result[12] == 3

        winner = _post(f"{base}/v1/auth/verify", {"token": p2["token"]})
        loser = _post(f"{base}/v1/auth/verify", {"token": p1["token"]})
        assert winner["bp"] == winner["xp"] == 0
        assert loser["bp"] == loser["xp"] == 0
    finally:
        a.close(); b.close()


def test_self_reported_win_by_the_leaver_earns_nothing(meta_relay):
    """Disconnecting cannot turn an unverified win claim into a reward."""
    base = meta_relay["meta_url"]
    rport = meta_relay["relay_port"]
    victim = _post(f"{base}/v1/guest")
    leaver = _post(f"{base}/v1/guest")

    v = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    a = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
    try:
        v.sendall(_qjoin(victim["token"]))
        a.sendall(_qjoin(leaver["token"]))
        assert _await_match_found(v)
        assert _await_match_found(a)
        _queue_accept(v, a)

        # 이탈자가 승리를 신고한다. 위 테스트와 같은 이유로 뒤이어 PING 을 보내
        # 요약이 소비된 것을 관측한 뒤에 닫는다 — 그래야 "요약 없음" 분기로 새지
        # 않고 이 테스트가 겨눈 분기에 확실히 들어간다.
        a.sendall(_summary(won=1, my_score=999999, my_lines=999,
                           opp_score=0, opp_lines=0))
        a.sendall(build_frame(MsgType.PING, b"\x00" * 8))
        assert _recv_until(v, MsgType.PING) is not None
        a.close()

        # 피해자는 이탈 통지를 받는다. 그러나 장부는 움직이지 않아야 한다.
        result = _recv_until(v, MsgType.MATCH_RESULT)
        if result is not None:
            assert struct.unpack_from("<i", result, 8)[0] == 0, \
                "이탈자의 자기신고로 피해자 RP 가 움직였다"

        cheat = _post(f"{base}/v1/auth/verify", {"token": leaver["token"]})
        prey  = _post(f"{base}/v1/auth/verify", {"token": victim["token"]})
        assert cheat["bp"] == 0 and cheat["xp"] == 0, \
            f"이탈자가 자기신고로 적립했다: bp={cheat['bp']} xp={cheat['xp']}"
        assert prey["bp"] == 0 and prey["xp"] == 0, \
            f"경기 중이던 피해자의 장부가 움직였다: bp={prey['bp']} xp={prey['xp']}"
    finally:
        v.close(); a.close()


def test_relay_without_meta_rejects_token(tmp_path):
    """meta 미기동인 채 --meta URL 만 주면 verify 가 실패해야 한다."""
    # 임의의 free port 를 --meta 로 쓰지만 그 포트에 아무것도 안 띄움 → connect 실패.
    fake_meta = f"http://127.0.0.1:{_free_port()}"
    rp, rport = _spawn_relay(fake_meta)
    try:
        s = socket.create_connection(("127.0.0.1", rport), timeout=2.0)
        try:
            # 임의 토큰. relay 가 meta 호출 → 네트워크 실패 → verify None → close.
            s.sendall(_qjoin("ab" * 16))
            s.settimeout(5.0)
            try:
                data = s.recv(4096)
            except (socket.timeout, ConnectionResetError):
                data = b""
            assert data == b""
        finally:
            s.close()
    finally:
        rp.terminate()
        try: rp.wait(timeout=3)
        except subprocess.TimeoutExpired: rp.kill()


@pytest.mark.parametrize("disconnect", [False, True])
def test_verified_terminal_game_ignores_false_claims(meta_relay, disconnect):
    base = meta_relay["meta_url"]
    players = [_post(f"{base}/v1/guest") for _ in range(2)]
    sockets = [socket.create_connection(("127.0.0.1", meta_relay["relay_port"]), timeout=2) for _ in range(2)]
    try:
        for sock, player in zip(sockets, players):
            sock.sendall(_qjoin(player["token"]))
        found = [_recv_until(sock, MsgType.MATCH_FOUND) for sock in sockets]
        assert all(found) and all(data[-1] == 1 for data in found)
        host_index = next(i for i, data in enumerate(found) if data[0] == 1)
        host, guest = sockets[host_index], sockets[1 - host_index]
        seed = struct.unpack_from("<Q", found[host_index], 1)[0]
        build = meta_relay["build"]
        probe = build / ("ranked_game_test.exe" if os.name == "nt" else "ranked_game_test")
        expected = json.loads(subprocess.check_output([str(probe.resolve()), str(seed)], text=True))
        _queue_accept(host, guest)
        count = expected["ticks"]
        host.sendall(build_frame(MsgType.INPUT, struct.pack("<IH", 0, count) + bytes(count)))
        guest.sendall(build_frame(MsgType.INPUT, struct.pack("<IH", 0, count) + bytes([16]) * count))
        # Observe input delivery before disconnecting. The relay must have consumed
        # both streams; it can award even when a client withholds its summary.
        assert _recv_until(host, MsgType.INPUT) is not None
        assert _recv_until(guest, MsgType.INPUT) is not None
        if disconnect:
            guest.close()
        else:
            host.sendall(_summary(0, 99999, 999, 88888, 888))
            guest.sendall(_summary(1, 88888, 888, 99999, 999))
        result = _recv_until(host, MsgType.MATCH_RESULT)
        assert result is not None and result[12] == 1
        assert struct.unpack_from("<i", result, 8)[0] > 0
        with sqlite3.connect(meta_relay["db"]) as con:
            rows = con.execute("SELECT winner,score_a,score_b,lines_a,lines_b FROM matches").fetchall()
        assert rows == [(players[host_index]["player_id"], expected["score_a"], expected["score_b"],
                         expected["lines_a"], expected["lines_b"])]
        for i, player in enumerate(players):
            profile = _post(f"{base}/v1/auth/verify", {"token": player["token"]})
            assert profile["bp"] == (30 if i == host_index else 10)
            assert profile["xp"] == (100 if i == host_index else 50)
    finally:
        for sock in sockets:
            sock.close()


@pytest.mark.parametrize("violation", ["mask", "rewrite", "seed"])
def test_invalid_input_never_posts_a_match(meta_relay, violation):
    base = meta_relay["meta_url"]
    players = [_post(f"{base}/v1/guest") for _ in range(2)]
    sockets = [socket.create_connection(("127.0.0.1", meta_relay["relay_port"]), timeout=2) for _ in range(2)]
    try:
        for sock, player in zip(sockets, players):
            sock.sendall(_qjoin(player["token"]))
        found = [_recv_until(sock, MsgType.MATCH_FOUND) for sock in sockets]
        assert all(found)
        host_index = next(i for i, data in enumerate(found) if data[0] == 1)
        host, guest = sockets[host_index], sockets[1 - host_index]
        _queue_accept(host, guest)
        if violation == "seed":
            seed = struct.unpack_from("<Q", found[host_index], 1)[0]
            host.sendall(build_frame(MsgType.SEED, struct.pack("<QIBB", seed ^ 1, 120, 2, 1)))
        elif violation == "mask":
            host.sendall(build_frame(MsgType.INPUT, struct.pack("<IHB", 0, 1, 32)))
        else:
            for mask in (0, 16):
                host.sendall(build_frame(MsgType.INPUT, struct.pack("<IHB", 0, 1, mask)))
        host.sendall(_summary(1, 99999, 999, 0, 0))
        guest.sendall(_summary(0, 0, 0, 99999, 999))
        for sock in (host, guest):
            result = _recv_until(sock, MsgType.MATCH_RESULT)
            assert result is not None and result[12] == 2  # InvalidReplay
            assert struct.unpack_from("<i", result, 8)[0] == 0
        with sqlite3.connect(meta_relay["db"]) as con:
            assert con.execute("SELECT count(*) FROM matches").fetchone()[0] == 0
    finally:
        for sock in sockets:
            sock.close()
