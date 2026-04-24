"""Smoke test: custom-room create/join/ready path.

Two clients connect to the running relay, A creates a room, B joins with the
returned code, both send READY → both must receive MATCH_FOUND with equal
seeds and HOST/GUEST roles.

Run separately (requires ``tetris_relay.exe`` running on ``--port 7788``)::

    python -m pytest python/tests/test_room_smoke.py -v
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


def _recv_until(sock: socket.socket, wanted: MsgType, deadline: float, buf: bytearray):
    """Read from ``sock`` until a frame of ``wanted`` type arrives; return its payload.

    Other frames received before the wanted one are returned as a list alongside
    so callers can still observe the sequence.
    """
    earlier: list[tuple[MsgType, bytes]] = []
    sock.settimeout(max(0.05, deadline - time.monotonic()))
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            raise TimeoutError(f"no {wanted.name} within deadline")
        if not chunk:
            raise RuntimeError(f"relay closed before {wanted.name}")
        buf.extend(chunk)
        for t, payload in parse_frames(buf):
            if t == wanted:
                return payload, earlier
            earlier.append((t, payload))
    raise TimeoutError(f"no {wanted.name} within deadline")


def _parse_room_info(payload: bytes) -> tuple[str, int, int]:
    # [code_len:1][code:N][status:1][peer_count:1]
    assert len(payload) >= 1
    n = payload[0]
    assert len(payload) >= 1 + n + 2, f"short ROOM_INFO: {len(payload)}"
    code = payload[1 : 1 + n].decode("ascii")
    status = payload[1 + n]
    peer_count = payload[2 + n]
    return code, status, peer_count


def _parse_match_found(payload: bytes) -> tuple[int, int]:
    assert len(payload) >= 9
    role = payload[0]
    seed = struct.unpack_from("<Q", payload, 1)[0]
    return role, seed


def test_room_create_join_ready_match_found() -> None:
    try:
        a = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    except OSError:
        pytest.skip(f"relay not running on {RELAY_HOST}:{RELAY_PORT}")

    b = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    a_buf = bytearray()
    b_buf = bytearray()
    try:
        # A: create room. tok_len=0 → unranked (relay 가 --meta 없이 띄워졌다고 가정).
        a.sendall(build_frame(MsgType.ROOM_CREATE, b"\x00"))
        deadline = time.monotonic() + RECV_TIMEOUT
        payload, _ = _recv_until(a, MsgType.ROOM_INFO, deadline, a_buf)
        code, status, peer_count = _parse_room_info(payload)
        assert len(code) == 5
        assert status == 0  # waiting
        assert peer_count == 1

        # B: join with code. payload = [code_len][code][tok_len=0].
        join_payload = bytes([len(code)]) + code.encode("ascii") + b"\x00"
        b.sendall(build_frame(MsgType.ROOM_JOIN, join_payload))

        deadline = time.monotonic() + RECV_TIMEOUT
        payload_b, _ = _recv_until(b, MsgType.ROOM_INFO, deadline, b_buf)
        _, status_b, peer_b = _parse_room_info(payload_b)
        assert status_b == 0
        assert peer_b == 2

        deadline = time.monotonic() + RECV_TIMEOUT
        payload_a2, _ = _recv_until(a, MsgType.ROOM_INFO, deadline, a_buf)
        _, status_a2, peer_a2 = _parse_room_info(payload_a2)
        assert status_a2 == 0
        assert peer_a2 == 2

        # Both send READY(1). Server forwards READY to peer and, when it sees
        # both ready, issues MATCH_FOUND to both.
        a.sendall(build_frame(MsgType.READY, bytes([1])))
        b.sendall(build_frame(MsgType.READY, bytes([1])))

        deadline = time.monotonic() + RECV_TIMEOUT
        mf_a, _ = _recv_until(a, MsgType.MATCH_FOUND, deadline, a_buf)
        role_a, seed_a = _parse_match_found(mf_a)

        deadline = time.monotonic() + RECV_TIMEOUT
        mf_b, _ = _recv_until(b, MsgType.MATCH_FOUND, deadline, b_buf)
        role_b, seed_b = _parse_match_found(mf_b)

        assert seed_a == seed_b, "both clients must share the same seed"
        assert {role_a, role_b} == {1, 2}, f"roles must be HOST+GUEST: {role_a},{role_b}"
    finally:
        a.close()
        b.close()


def test_room_join_nonexistent_code_gets_notfound() -> None:
    try:
        s = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    except OSError:
        pytest.skip(f"relay not running on {RELAY_HOST}:{RELAY_PORT}")

    buf = bytearray()
    try:
        code = "ZZZZZ"
        # ROOM_JOIN payload = [code_len][code][tok_len=0]
        s.sendall(build_frame(MsgType.ROOM_JOIN,
                               bytes([len(code)]) + code.encode() + b"\x00"))
        deadline = time.monotonic() + RECV_TIMEOUT
        payload, _ = _recv_until(s, MsgType.ROOM_INFO, deadline, buf)
        rcode, status, _ = _parse_room_info(payload)
        assert rcode == code
        assert status == 2  # notfound
    finally:
        s.close()
