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
    try:
        a = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    except OSError:
        pytest.skip(f"relay not running on {RELAY_HOST}:{RELAY_PORT}")

    b = socket.create_connection((RELAY_HOST, RELAY_PORT), timeout=1.0)
    try:
        a.sendall(build_frame(MsgType.QUEUE_JOIN, b""))
        b.sendall(build_frame(MsgType.QUEUE_JOIN, b""))

        role_a, seed_a = _recv_match_found(a)
        role_b, seed_b = _recv_match_found(b)

        assert seed_a == seed_b, "both clients must receive same seed"
        assert {role_a, role_b} == {1, 2}, f"roles must be HOST(1) + GUEST(2): got {role_a}, {role_b}"
    finally:
        a.close()
        b.close()
