"""Secure admission integration: real TLS, native transport and both relay engines.
No external network, private credentials or installed browser is used.
"""
from __future__ import annotations
import base64
import concurrent.futures
import hashlib
import os
from pathlib import Path
import socket
import ssl
import struct
import sys
import subprocess
import time
import pytest
from netbot.framing import MsgType, build_frame, parse_frames
from .test_meta_db_smoke import _find_meta_bin, _free_port, _post, _wait_listen

SECRET = "secure-integration-test-only"


def executable(name):
    suffix = ".exe" if os.name == "nt" else ""
    env = os.environ.get("TETRIS_SECURE_BUILD")
    folders = [Path(env)] if env else [Path("build-secure"), Path("build/Release"), Path("build")]
    for folder in folders:
        candidate = (folder / (name + suffix)).resolve()
        if candidate.is_file():
            return candidate
    if env:
        pytest.fail(f"{name} missing from explicit TETRIS_SECURE_BUILD={env}")
    pytest.skip(f"{name} not built; set TETRIS_SECURE_BUILD")


@pytest.fixture
def meta(tmp_path):
    binary = _find_meta_bin()
    if not binary:
        pytest.skip("meta not built")
    port = _free_port()
    process = subprocess.Popen([str(binary.resolve()), "--http", f"127.0.0.1:{port}",
                               "--db", str(tmp_path / "meta.db"), "--relay-secret", SECRET],
                              stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        assert _wait_listen(port)
        yield f"http://127.0.0.1:{port}"
    finally:
        process.terminate(); process.communicate(timeout=10)


def guest(meta):
    status, data = _post(meta + "/v1/guest")
    assert status == 200
    return data["token"]


def ticket(meta, token):
    status, data = _post(meta + "/v1/game-tickets", {"token": token})
    assert status == 200, data
    assert data["expires_in"] == 60 and data["ticket"].startswith("gt1.")
    return data["ticket"]


def consume(meta, value, secret=SECRET):
    return _post(meta + "/v1/game-tickets/consume", {"ticket": value}, headers={"X-Relay-Secret": secret})


def test_ticket_scope_secret_and_atomic_consumption(meta):
    account = guest(meta)
    value = ticket(meta, account)
    assert consume(meta, value, "wrong")[0] == 403
    assert _post(meta + "/v1/auth/verify", {"token": value})[0] == 404
    assert _post(meta + "/v1/game-tickets", {"token": value})[0] == 401
    assert consume(meta, account)[0] == 401
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        results = list(pool.map(lambda _: consume(meta, value)[0], range(8)))
    assert results.count(200) == 1 and results.count(401) == 7
    newer = ticket(meta, account)
    newest = ticket(meta, account)
    assert consume(meta, newer)[0] == 401
    assert consume(meta, newest)[0] == 200


def test_game_ticket_issue_rate_limit(meta):
    account = guest(meta)
    for _ in range(10):
        ticket(meta, account)
    assert _post(meta + "/v1/game-tickets", {"token": account})[0] == 429


@pytest.fixture(scope="module")
def certificates(tmp_path_factory):
    folder = tmp_path_factory.mktemp("tls")
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                    "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                    "-keyout", str(folder / "key.pem"), "-out", str(folder / "cert.pem")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["openssl", "x509", "-x509toreq", "-in", str(folder / "cert.pem"),
                    "-signkey", str(folder / "key.pem"), "-out", str(folder / "request.pem")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    (folder / "index").write_text("")
    (folder / "serial").write_text("01\n")
    (folder / "ca.cnf").write_text("[ca]\ndefault_ca=test\n[test]\n"
        + f"database={folder.as_posix()}/index\nserial={folder.as_posix()}/serial\nnew_certs_dir={folder.as_posix()}\n"
        + f"certificate={folder.as_posix()}/cert.pem\nprivate_key={folder.as_posix()}/key.pem\n"
        + "default_md=sha256\npolicy=policy\n[policy]\ncommonName=supplied\n[server]\nsubjectAltName=DNS:localhost\n")
    subprocess.run(["openssl", "ca", "-batch", "-selfsign", "-config", str(folder / "ca.cnf"),
                    "-in", str(folder / "request.pem"), "-startdate", "20200101000000Z", "-enddate", "20200102000000Z",
                    "-extensions", "server", "-out", str(folder / "expired.pem")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return folder


@pytest.fixture(params=["tetris_relay_reactor", "tetris_relay"])
def stack(meta, certificates, request):
    if sys.platform == "darwin" and request.param == "tetris_relay_reactor":
        pytest.skip("macOS has no reactor backend; portable relay is tested")
    relay = executable(request.param)
    gateway = executable("tetris_wss_gateway")
    backend_port, public_port = _free_port(), _free_port()
    relay_process = subprocess.Popen([str(relay), "--port", str(backend_port), "--loopback-only",
                                     "--meta", meta, "--meta-secret", SECRET],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    gateway_process = None
    try:
        assert _wait_listen(backend_port)
        gateway_process = subprocess.Popen([str(gateway), "--bind", "127.0.0.1", "--port", str(public_port),
                                           "--backend-port", str(backend_port), "--cert", str(certificates / "cert.pem"),
                                           "--key", str(certificates / "key.pem"), "--origin", "https://game.example.test"],
                                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        assert _wait_listen(public_port)
        yield meta, certificates, backend_port, public_port
    finally:
        if gateway_process:
            gateway_process.terminate(); gateway_process.communicate(timeout=10)
        relay_process.terminate(); relay_process.communicate(timeout=10)


class WebSocket:
    """Tiny test peer; production code uses Beast's RFC 6455 implementation."""
    def __init__(self, port, ca, *, origin=None, path="/play", compression=False):
        context = ssl.create_default_context(cafile=str(ca))
        self.socket = context.wrap_socket(socket.create_connection(("127.0.0.1", port), timeout=3), server_hostname="localhost")
        key = base64.b64encode(os.urandom(16)).decode()
        extra = f"Origin: {origin}\r\n" if origin is not None else ""
        if compression:
            extra += "Sec-WebSocket-Extensions: permessage-deflate\r\n"
        self.socket.sendall((f"GET {path} HTTP/1.1\r\nHost: localhost:{port}\r\nUpgrade: websocket\r\n"
                             f"Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n{extra}\r\n").encode())
        header = b""
        while not header.endswith(b"\r\n\r\n"):
            data = self.socket.recv(1)
            if not data:
                self.socket.close()
                raise ConnectionError("upgrade rejected")
            header += data
            assert len(header) < 8192
        assert b" 101 " in header
        accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest())
        assert accept in header and b"permessage-deflate" not in header.lower()

    def close(self):
        self.socket.close()

    def send(self, payload, opcode=2, *, masked=True, final=True):
        n = len(payload)
        header = bytes([(128 if final else 0) | opcode])
        header += bytes([(128 if masked else 0) | (n if n < 126 else 126 if n < 65536 else 127)])
        if n >= 126:
            header += struct.pack("!H" if n < 65536 else "!Q", n)
        if masked:
            key = os.urandom(4)
            payload = bytes(b ^ key[i % 4] for i, b in enumerate(payload))
            header += key
        self.socket.sendall(header + payload)

    def exact(self, count):
        data = b""
        while len(data) < count:
            chunk = self.socket.recv(count - len(data))
            if not chunk:
                raise ConnectionError("closed")
            data += chunk
        return data

    def receive(self):
        a, b = self.exact(2)
        n = b & 127
        if n == 126: n = struct.unpack("!H", self.exact(2))[0]
        if n == 127: n = struct.unpack("!Q", self.exact(8))[0]
        assert not b & 128
        data = self.exact(n)
        if a & 15 == 8: raise ConnectionError("close frame")
        return a & 15, data


def room_create(credential):
    value = credential.encode()
    return build_frame(MsgType.ROOM_CREATE, bytes([len(value)]) + value)


def test_native_wss_ticket_and_certificate_validation(stack, tmp_path):
    meta, certs, backend, port = stack
    account = guest(meta)
    value = ticket(meta, account)
    payload = tmp_path / "first-frame.bin"
    payload.write_bytes(room_create(value))
    probe = executable("wss_probe")
    env = os.environ.copy(); env["TETRIS_CA_FILE"] = str(certs / "cert.pem")
    # Hostname mismatch and untrusted CA must fail before ticket redemption.
    wrong = subprocess.run([str(probe), f"wss://127.0.0.1:{port}/play", str(payload)], env=env, capture_output=True, timeout=10)
    assert wrong.returncode == 4
    untrusted_env = dict(env); untrusted_env.pop("TETRIS_CA_FILE")
    untrusted = subprocess.run([str(probe), f"wss://localhost:{port}/play", str(payload)], env=untrusted_env, capture_output=True, timeout=10)
    assert untrusted.returncode == 4
    result = subprocess.run([str(probe), f"wss://localhost:{port}/play", str(payload)], env=env, capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr
    assert any(kind == MsgType.ROOM_INFO for kind, _ in parse_frames(bytearray(result.stdout)))
    # Consumed credentials and account credentials are rejected by both relays.
    for bad in [value, account]:
        payload.write_bytes(room_create(bad))
        result = subprocess.run([str(probe), f"wss://localhost:{port}/play", str(payload)], env=env, capture_output=True, timeout=10)
        assert result.returncode == 6, (result.returncode, result.stderr)


def test_browser_origin_fragmentation_ping_and_raw_token_rejection(stack):
    meta, certs, backend, port = stack
    account = guest(meta)
    for args in ({"origin": "https://evil.example.test"}, {"origin": "null"}, {"origin": ""}, {"path": "/play?token=x"}):
        with pytest.raises((ConnectionError, ssl.SSLError)):
            WebSocket(port, certs / "cert.pem", **args)
    ws = WebSocket(port, certs / "cert.pem", origin="https://game.example.test", compression=True)
    try:
        ws.send(b"ping", opcode=9)
        assert ws.receive() == (10, b"ping")
        frame = room_create(ticket(meta, account))
        ws.send(frame[:4], final=False)
        ws.send(frame[4:], opcode=0)
        opcode, data = ws.receive()
        assert opcode == 2 and parse_frames(bytearray(data))[0][0] == MsgType.ROOM_INFO
    finally:
        ws.close()
    with socket.create_connection(("127.0.0.1", backend), timeout=3) as raw:
        raw.sendall(room_create(account))
        assert raw.recv(1024) == b""


@pytest.mark.parametrize("mode", ["text", "unmasked", "oversized"])
def test_gateway_rejects_bad_frames(stack, mode):
    _, certs, _, port = stack
    ws = WebSocket(port, certs / "cert.pem")
    try:
        ws.send(b"x" * (16385 if mode == "oversized" else 1),
                opcode=1 if mode == "text" else 2, masked=mode != "unmasked")
        with pytest.raises((ConnectionError, ssl.SSLError)):
            ws.receive()
    finally:
        ws.close()


def test_expired_certificate_is_rejected(certificates, tmp_path):
    port = _free_port()
    process = subprocess.Popen([str(executable("tetris_wss_gateway")), "--bind", "127.0.0.1", "--port", str(port),
                                "--cert", str(certificates / "expired.pem"), "--key", str(certificates / "key.pem")],
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        assert _wait_listen(port)
        payload = tmp_path / "frame.bin"; payload.write_bytes(b"x")
        env = dict(os.environ, TETRIS_CA_FILE=str(certificates / "expired.pem"))
        result = subprocess.run([str(executable("wss_probe")), f"wss://localhost:{port}/play", str(payload)],
                                env=env, capture_output=True, timeout=10)
        assert result.returncode == 4
    finally:
        process.terminate(); process.communicate(timeout=10)


def receive_game(ws, wanted, buffer):
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        for kind, payload in parse_frames(buffer):
            if kind == wanted:
                return payload
        opcode, data = ws.receive()
        assert opcode == 2
        buffer.extend(data)
    raise TimeoutError(f"missing {wanted}")


@pytest.mark.parametrize("mode", ["queue", "room"])
def test_secure_match_and_bidirectional_input(stack, mode):
    meta, certs, _, port = stack
    credentials = [ticket(meta, guest(meta)).encode() for _ in range(2)]
    a = WebSocket(port, certs / "cert.pem")
    b = WebSocket(port, certs / "cert.pem", origin="https://game.example.test")
    a_buffer, b_buffer = bytearray(), bytearray()
    try:
        if mode == "queue":
            for ws, value in zip([a, b], credentials):
                ws.send(build_frame(MsgType.QUEUE_JOIN, bytes([len(value)]) + value))
        else:
            a.send(room_create(credentials[0].decode()))
            info = receive_game(a, MsgType.ROOM_INFO, a_buffer)
            code = info[1:1 + info[0]]
            value = credentials[1]
            b.send(build_frame(MsgType.ROOM_JOIN, bytes([len(code)]) + code + bytes([len(value)]) + value))
            receive_game(b, MsgType.ROOM_INFO, b_buffer)
            a.send(build_frame(MsgType.READY, b"\x01"))
            b.send(build_frame(MsgType.READY, b"\x01"))
        matches = [receive_game(a, MsgType.MATCH_FOUND, a_buffer), receive_game(b, MsgType.MATCH_FOUND, b_buffer)]
        assert {match[0] for match in matches} == {1, 2}
        assert matches[0][1:9] == matches[1][1:9]
        if mode == "queue":
            a.send(build_frame(MsgType.READY, b"\x01"))
            b.send(build_frame(MsgType.READY, b"\x01"))
        for sender, receiver, buffer, tick in [(a, b, b_buffer, 100), (b, a, a_buffer, 101)]:
            payload = struct.pack("<IH", tick, 1) + b"\x00"
            sender.send(build_frame(MsgType.INPUT, payload))
            assert receive_game(receiver, MsgType.INPUT, buffer) == payload
    finally:
        a.close(); b.close()


def test_gateway_reclaims_completed_connections(stack):
    # More than the per-IP simultaneous limit over time must remain possible.
    _, certs, _, port = stack
    for _ in range(20):
        ws = WebSocket(port, certs / "cert.pem")
        ws.send(b"lifecycle", opcode=9)
        assert ws.receive() == (10, b"lifecycle")
        ws.close()
        time.sleep(0.01)


def test_native_session_issues_ticket_and_leaves_room(stack, tmp_path):
    meta, certs, _, port = stack
    account_file = tmp_path / "account"
    account_file.write_text(guest(meta))
    env = dict(os.environ, TETRIS_CA_FILE=str(certs / "cert.pem"))
    probe = executable("wss_probe")
    # Plain HTTP to a non-loopback API is rejected before any connection attempt.
    unsafe = subprocess.run([str(probe), "--session", f"wss://localhost:{port}/play",
                             "http://api.example.test", str(account_file)], env=env, capture_output=True, timeout=10)
    assert unsafe.returncode == 4
    result = subprocess.run([str(probe), "--session", f"wss://localhost:{port}/play", meta, str(account_file)],
                            env=env, capture_output=True, timeout=12)
    assert result.returncode == 0, (result.returncode, result.stderr)
