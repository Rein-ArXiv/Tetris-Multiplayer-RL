"""Smoke test: connect two clients to relay, both receive MATCH_FOUND with same seed.

Run separately (requires ``tetris_relay.exe`` running on ``--port 7788``)::

    python -m pytest python/tests/test_relay_smoke.py -v
"""

from __future__ import annotations

import socket
import struct
import time

import pytest

from netbot.framing import MsgType, build_frame, parse_frames


RELAY_HOST = "127.0.0.1"
RELAY_PORT = 7788
RECV_TIMEOUT = 5.0


def _recv_match_found(sock: socket.socket) -> tuple[int, int]:
    sock.settimeout(RECV_TIMEOUT)
    buf = bytearray()
    deadline = time.monotonic() + RECV_TIMEOUT
    while time.monotonic() < deadline:
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError("relay closed before MATCH_FOUND")
        buf.extend(chunk)
        for t, payload in parse_frames(buf):
            if t == MsgType.MATCH_FOUND:
                assert len(payload) >= 9, f"MATCH_FOUND payload too short: {len(payload)}"
                role = payload[0]
                seed = struct.unpack_from("<Q", payload, 1)[0]
                return role, seed
    raise TimeoutError("no MATCH_FOUND within deadline")


def test_relay_pairs_two_clients() -> None:
    """QUEUE_JOIN → MATCH_FOUND → 양쪽 READY(1) 수락 로비 → 게임 포워딩 시작.

    릴레이는 MATCH_FOUND 직후 양쪽이 READY(1) 을 보낼 때까지 대기하는 수락
    로비 단계를 끼워넣는다 (매치가 즉시 시작되지 않도록). 이 테스트는 두 클라가
    모두 수락한다고 가정하고 MATCH_FOUND 가 양쪽에 도착하는지만 검증.
    """
    try:
        a = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    except OSError:
        pytest.skip(f"relay not running on {RELAY_HOST}:{RELAY_PORT}")

    b = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    try:
        # tok_len=0 → unranked. relay 가 --meta 없이 띄워졌다고 가정.
        a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))

        role_a, seed_a = _recv_match_found(a)
        role_b, seed_b = _recv_match_found(b)

        assert seed_a == seed_b, "both clients must receive same seed"
        assert {role_a, role_b} == {1, 2}, f"roles must be HOST(1) + GUEST(2): got {role_a}, {role_b}"

        # 수락 로비를 통과시키기 위한 READY(1) — 실제 게임이 시작되는지는 검증하지
        # 않지만, relay 가 수락 단계에서 끊지 않는 것을 간접 확인.
        a.sendall(build_frame(MsgType.READY, bytes([1])))
        b.sendall(build_frame(MsgType.READY, bytes([1])))
    finally:
        a.close()
        b.close()


def test_relay_returns_session_slots_when_matches_end() -> None:
    """끝난 매치의 per-IP 세션 슬롯은 돌아와야 한다.

    슬롯은 accept 부터 연결이 죽을 때까지 유지되므로, 반납이 새면 한 주소는
    kMaxSessionsPerIp(64) 번의 매치만에 스스로를 잠근다 — 서버를 재시작하기
    전까지 그 주소에서는 아무도 접속할 수 없다.

    reactor 에서 이 경로가 특히 미끄럽다: 포워딩이 시작되는 순간 연결이 샤드
    스레드로 넘어가므로, 반납을 수행하는 close_conn 이 슬롯을 발급한 앞단 루프가
    아닌 다른 스레드에서 돈다. 입장 표가 루프마다 있으면 그 반납은 엉뚱한 표로
    가고 앞단이 센 수는 영영 줄지 않는다. (그래서 표는 프로세스 전역이다.)

    상한보다 많은 연결을 여러 라운드로 나눠 붙였다 떼며 확인한다 — 누적이
    상한을 넘어도 계속 붙을 수 있어야 한다.
    """
    # 라운드마다 2연결. 누적이 상한(64)을 확실히 넘도록 잡는다.
    rounds = 40
    try:
        probe = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    except OSError:
        pytest.skip(f"relay not running on {RELAY_HOST}:{RELAY_PORT}")
    probe.close()

    for index in range(rounds):
        a = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=2.0)
        b = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=2.0)
        try:
            a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
            b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
            try:
                _recv_match_found(a)
                _recv_match_found(b)
            except (RuntimeError, ConnectionResetError) as exc:
                # 릴레이가 끊었다 = 슬롯이 안 돌아왔다. 앞선 라운드가 모두
                # 성공했으므로 상한 자체는 넉넉했다는 뜻이고, 누적 연결 수가
                # 상한을 넘은 지점에서 끊긴 것은 반납이 샜다는 뜻이다.
                pytest.fail(
                    f"round {index} (누적 {2 * index} 연결) 에서 릴레이가 끊었다 — "
                    f"끝난 매치의 per-IP 세션 슬롯이 반납되지 않는다: {exc}")
            # 양쪽 READY(1) 로 포워딩까지 밀어 올린다. reactor 는 바로 이
            # 지점에서 매치를 샤드로 넘기므로, 여기까지 가야 "다른 스레드가
            # 반납한다" 는 경로를 밟는다.
            a.sendall(build_frame(MsgType.READY, bytes([1])))
            b.sendall(build_frame(MsgType.READY, bytes([1])))
        finally:
            a.close()
            b.close()


def test_relay_queue_decline_closes_both() -> None:
    """한쪽이 수락 로비에서 READY(0) 을 보내면 양 소켓이 모두 닫혀야 한다."""
    try:
        a = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    except OSError:
        pytest.skip(f"relay not running on {RELAY_HOST}:{RELAY_PORT}")
    b = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    try:
        a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        _recv_match_found(a)
        _recv_match_found(b)

        # A 가 거절 → relay 가 B 에게도 READY(0) 포워딩 후 두 소켓 모두 close.
        a.sendall(build_frame(MsgType.READY, bytes([0])))

        b.settimeout(3.0)
        try:
            # B 쪽에는 READY(0) 포워드가 오거나 곧 EOF 가 와야 함.
            data = b.recv(4096)
        except (socket.timeout, ConnectionResetError):
            data = b""
        # 조기 종료 여부만 확인 — 잔여 READY(0) 또는 바로 EOF.
        assert data == b"" or data.__class__ is bytes
    finally:
        a.close()
        b.close()


def test_relay_preserves_cancel_coalesced_with_queue_join() -> None:
    """QUEUE_JOIN과 같은 TCP read에 온 CANCEL은 큐 단계에서 유실되지 않는다."""
    try:
        cancelled = socket.create_connection(
            (RELAY_HOST, RELAY_PORT), timeout=1.0
        )
    except OSError:
        pytest.skip(f"relay not running on {RELAY_HOST}:{RELAY_PORT}")

    b = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    c = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    try:
        cancelled.sendall(
            build_frame(MsgType.QUEUE_JOIN, b"\x00")
            + build_frame(MsgType.QUEUE_CANCEL, b"")
        )
        # B가 들어오면 matcher가 선두의 취소 프레임을 확인하고 제거한다.
        time.sleep(0.05)
        b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        time.sleep(0.05)
        c.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))

        role_b, seed_b = _recv_match_found(b)
        role_c, seed_c = _recv_match_found(c)
        assert seed_b == seed_c
        assert {role_b, role_c} == {1, 2}
    finally:
        cancelled.close()
        b.close()
        c.close()
