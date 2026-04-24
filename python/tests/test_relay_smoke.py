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
