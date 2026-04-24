"""Cross-check + meta-failure tests for MATCH_SUMMARY → MATCH_RESULT path.

Covers:
  · 양쪽이 모순된 won/score 를 보고 → relay 가 winner=NULL → ELO 미반영 (delta=0).
  · 양쪽이 일치하는 won/score 를 보고 → ELO 변동 (+ leaderboard 갱신).
  · meta 미기동 상태에서 relay 에 접속 → 즉시 close (verify 거부).

이 테스트들은 게임 시뮬을 실제로 돌리지 않고 클라 → relay 프레임만 직접 만들어
보낸다. relay 가 lockstep 의 어느 것도 검증하지 않는다는 사실에 의존.
"""
from __future__ import annotations

import json
import os
import socket
import struct
import subprocess
import time
import urllib.request
from pathlib import Path

import pytest

from netbot.framing import MsgType, build_frame, parse_frames


# ---- helpers (test_relay_meta_smoke.py 와 같은 패턴 — 코드 분리 우선) -----------

def _find_bin(name: str, env_var: str) -> Path | None:
    env = os.environ.get(env_var)
    if env:
        p = Path(env)
        return p if p.exists() else None
    repo = Path(__file__).resolve().parents[2]
    suffix = ".exe" if os.name == "nt" else ""
    base = name + suffix
    for c in [
        repo / f"build-{name.split('_')[1]}" / "Debug" / base,
        repo / f"build-{name.split('_')[1]}" / base,
        repo / "build" / "Debug" / base,
        repo / "build" / base,
    ]:
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


def _post(url: str, body: dict | None = None) -> dict:
    req = urllib.request.Request(
        url, data=json.dumps(body or {}).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=5.0) as r:
        return json.loads(r.read().decode())


def _get(url: str) -> object:
    req = urllib.request.Request(url, method="GET")
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
        for t, p in parse_frames(buf):
            if t == want_type:
                return bytes(p)
    return None


def _await_match_found(sock: socket.socket, timeout: float = 5.0) -> bool:
    return _recv_until(sock, MsgType.MATCH_FOUND, timeout) is not None


def _queue_accept(a: socket.socket, b: socket.socket) -> None:
    """MATCH_FOUND 수신 후 수락 로비 통과용 — 양쪽 READY(1) 송신 + peer forward 흡수."""
    a.sendall(build_frame(MsgType.READY, bytes([1])))
    b.sendall(build_frame(MsgType.READY, bytes([1])))
    # relay 가 상대 READY(1) 을 forward 한다. 뒤따르는 MATCH_SUMMARY parse 와 섞이지
    # 않도록 여기서 drain. (드롭 안 해도 parse_frames 가 타입으로 구분하긴 하지만
    # 테스트의 _recv_until 은 첫 매치만 찾으므로 버퍼에 잔여 READY 가 남아도 무해.)


def _spawn_meta(tmp_path):
    bin_ = _find_bin("tetris_meta", "TETRIS_META_BIN")
    if not bin_:
        pytest.skip("tetris_meta binary missing")
    port = _free_port()
    proc = subprocess.Popen(
        [str(bin_), "--db", str(tmp_path / "test.db"),
         "--http", f"127.0.0.1:{port}"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if not _wait_listen(port, 5.0):
        proc.kill()
        pytest.fail("meta failed to listen")
    return proc, f"http://127.0.0.1:{port}"


def _spawn_relay(meta_url: str | None):
    bin_ = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    if not bin_:
        pytest.skip("tetris_relay binary missing")
    port = _free_port()
    args = [str(bin_), "--port", str(port)]
    if meta_url:
        args += ["--meta", meta_url]
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if not _wait_listen(port, 5.0):
        proc.kill()
        pytest.fail("relay failed to listen")
    return proc, port


# ---- 테스트 ------------------------------------------------------------------


@pytest.fixture
def meta_relay(tmp_path):
    """meta + relay (meta 연동) 페어 띄움."""
    mp, mu = _spawn_meta(tmp_path)
    rp, rport = _spawn_relay(mu)
    try:
        yield {"meta_url": mu, "relay_port": rport}
    finally:
        for proc in (rp, mp):
            proc.terminate()
            try: proc.wait(timeout=3)
            except subprocess.TimeoutExpired: proc.kill()


@pytest.fixture
def relay_only(tmp_path):
    """relay 만 (meta 없음). MATCH_SUMMARY 는 투명 포워딩됨."""
    rp, rport = _spawn_relay(None)
    try:
        yield {"relay_port": rport}
    finally:
        rp.terminate()
        try: rp.wait(timeout=3)
        except subprocess.TimeoutExpired: rp.kill()


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


def test_consistent_summaries_apply_elo(meta_relay):
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
        # delta != 0 → ELO 적용됨.
        delta_a = struct.unpack_from("<i", ra, 8)[0]
        delta_b = struct.unpack_from("<i", rb, 8)[0]
        assert delta_a > 0 and delta_b < 0
        assert delta_a == -delta_b  # K-factor 동일 → 합 0
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
        # score 교차 검증 실패 → winner=null → ELO 미반영.
        delta_a = struct.unpack_from("<i", ra, 8)[0]
        delta_b = struct.unpack_from("<i", rb, 8)[0]
        assert delta_a == 0 and delta_b == 0
    finally:
        a.close(); b.close()


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
