"""Cross-check + meta-failure tests for MATCH_SUMMARY → MATCH_RESULT path.

Covers:
  · 양쪽이 모순된 won/score 를 보고 → relay 가 winner=NULL → RP 미반영 (delta=0).
  · 양쪽이 일치하는 won/score 를 보고 → RP 변동 (+ leaderboard 갱신).
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


def _spawn_relay(meta_url: str | None, relay_secret: str | None = TEST_RELAY_SECRET):
    bin_ = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
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
        # 승자 delta > 0 → RP 적용됨. 패자는 0(RP 바닥)에서 시작하므로
        # 바닥 clamp 으로 delta 0 (meta/elo.h 의 0-시작/0-바닥 스케일).
        delta_a = struct.unpack_from("<i", ra, 8)[0]
        delta_b = struct.unpack_from("<i", rb, 8)[0]
        assert delta_a > 0 and delta_b == 0
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
        # score 교차 검증 실패 → winner=null → RP 미반영.
        delta_a = struct.unpack_from("<i", ra, 8)[0]
        delta_b = struct.unpack_from("<i", rb, 8)[0]
        assert delta_a == 0 and delta_b == 0
    finally:
        a.close(); b.close()


def test_disconnect_before_summary_is_forfeit(meta_relay):
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

        # relay 의 몰수패 처리(finalizeForfeit)는 요약 0개면 담합 RP 파밍
        # 방지를 위해 meta 에 아무것도 반영하지 않는다 (delta=0). RP/BP/XP
        # 반영을 검증하는 이 테스트는 "생존자 요약 1개" 분기여야 하므로,
        # b(생존자) 가 먼저 승리 요약을 제출한 뒤 a 가 끊긴다.
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
        assert struct.unpack_from("<i", result, 8)[0] > 0

        winner = _post(f"{base}/v1/auth/verify", {"token": p2["token"]})
        loser = _post(f"{base}/v1/auth/verify", {"token": p1["token"]})
        assert winner["bp"] == 30 and winner["xp"] == 100
        assert loser["bp"] == 10 and loser["xp"] == 50
    finally:
        a.close(); b.close()


def test_self_reported_win_by_the_leaver_earns_nothing(meta_relay):
    """이탈자가 자기 승리를 신고하고 끊으면 아무것도 적립되면 안 된다.

    바로 위 테스트가 못 박은 "생존자 요약 1개 -> 몰수승" 과 짝이다. 요약이 하나뿐인
    상황은 둘로 갈리는데, 그 둘을 구분하지 않으면 한쪽이 공격이 된다:

      · 생존자가 냈다  -> 상대가 자리를 떴고 남은 사람이 결과를 보고했다. 존중한다.
      · 이탈자가 냈다  -> 자기 승리를 자기가 신고하고 자리를 떴다. 그 주장을 반증할
                         상대는 아직 경기 중이라 아무것도 제출하지 못했다. 교차검증은
                         이 경로에 개입하지 않으므로 그 한 장이 곧 판결이 된다.

    수정 전 실측(배포 바이너리): READY 직후 MATCH_SUMMARY{won=1} 한 장을 보내고 소켓을
    닫자, 게임 프레임을 한 장도 주고받지 않은 채 신고자의 elo 가 0 에서 16 으로 올랐다.
    큐에서 만난 아무에게나 성립하므로 공모자도 플레이도 필요 없었다.

    승자를 뒤집지 않고 비우는 것이 계약이다. 뒤집으면 이번에는 자폭이 도구가 된다 —
    지고 있는 사람이 패배 요약을 낸 뒤 끊어 상대의 승리를 지울 수 있다.
    """
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
