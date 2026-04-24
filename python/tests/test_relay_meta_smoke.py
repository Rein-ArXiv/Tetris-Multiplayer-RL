"""Smoke test: tetris_meta + tetris_relay end-to-end auth path.

Spawns both binaries on free ports, has two TCP clients fetch guest tokens,
QUEUE_JOIN with those tokens, and verifies they pair up via MATCH_FOUND.

A third connection with an invalid token must be closed by the relay (no
MATCH_FOUND, recv returns EOF).

Run::

    python -m pytest python/tests/test_relay_meta_smoke.py -v
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


def _build_queue_join(token: str) -> bytes:
    payload = bytes([len(token)]) + token.encode("ascii")
    return build_frame(MsgType.QUEUE_JOIN, payload)


def _recv_match_found(sock: socket.socket, timeout: float = 5.0) -> tuple[int, int]:
    sock.settimeout(timeout)
    buf = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            raise RuntimeError("relay closed before MATCH_FOUND")
        buf.extend(chunk)
        for t, p in parse_frames(buf):
            if t == MsgType.MATCH_FOUND:
                role = p[0]
                seed = struct.unpack_from("<Q", p, 1)[0]
                return role, seed
    raise TimeoutError("no MATCH_FOUND within deadline")


@pytest.fixture
def meta_and_relay(tmp_path):
    meta_bin  = _find_bin("tetris_meta",  "TETRIS_META_BIN")
    relay_bin = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    if not meta_bin:  pytest.skip("tetris_meta binary missing")
    if not relay_bin: pytest.skip("tetris_relay binary missing")

    meta_port  = _free_port()
    relay_port = _free_port()

    db = tmp_path / "test.db"
    meta_proc = subprocess.Popen(
        [str(meta_bin), "--db", str(db), "--http", f"127.0.0.1:{meta_port}"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if not _wait_listen(meta_port, 5.0):
        meta_proc.kill()
        pytest.fail("tetris_meta failed to listen")

    relay_proc = subprocess.Popen(
        [str(relay_bin), "--port", str(relay_port),
         "--meta", f"http://127.0.0.1:{meta_port}"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if not _wait_listen(relay_port, 5.0):
        relay_proc.kill()
        meta_proc.kill()
        pytest.fail("tetris_relay failed to listen")

    try:
        yield {
            "meta_url":   f"http://127.0.0.1:{meta_port}",
            "relay_host": "127.0.0.1",
            "relay_port": relay_port,
        }
    finally:
        for proc in (relay_proc, meta_proc):
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()


def test_two_authed_clients_pair(meta_and_relay):
    base = meta_and_relay["meta_url"]
    rh   = meta_and_relay["relay_host"]
    rp   = meta_and_relay["relay_port"]

    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    a = socket.create_connection((rh, rp), timeout=2.0)
    b = socket.create_connection((rh, rp), timeout=2.0)
    try:
        a.sendall(_build_queue_join(p1["token"]))
        b.sendall(_build_queue_join(p2["token"]))

        role_a, seed_a = _recv_match_found(a)
        role_b, seed_b = _recv_match_found(b)
        assert seed_a == seed_b
        assert {role_a, role_b} == {1, 2}

        # 수락 로비를 통과시켜 게임 포워딩 단계로 넘기기.
        a.sendall(build_frame(MsgType.READY, bytes([1])))
        b.sendall(build_frame(MsgType.READY, bytes([1])))
    finally:
        a.close(); b.close()


def test_invalid_token_rejected(meta_and_relay):
    rh   = meta_and_relay["relay_host"]
    rp   = meta_and_relay["relay_port"]

    s = socket.create_connection((rh, rp), timeout=2.0)
    try:
        # 미등록 토큰. relay 가 verify 실패로 즉시 close 해야 함.
        s.sendall(_build_queue_join("ff" * 16))
        s.settimeout(3.0)
        # EOF 나 ConnectionReset 둘 다 가능 (플랫폼 차).
        try:
            data = s.recv(4096)
        except (socket.timeout, ConnectionResetError):
            data = b""
        assert data == b""  # 어떠한 MATCH_FOUND 도 오면 안 됨.
    finally:
        s.close()


def test_empty_token_rejected_when_meta_active(meta_and_relay):
    rh = meta_and_relay["relay_host"]
    rp = meta_and_relay["relay_port"]

    s = socket.create_connection((rh, rp), timeout=2.0)
    try:
        # tok_len=0 → 토큰 없음. meta 활성화 모드에서는 reject.
        s.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        s.settimeout(3.0)
        try:
            data = s.recv(4096)
        except (socket.timeout, ConnectionResetError):
            data = b""
        assert data == b""
    finally:
        s.close()
