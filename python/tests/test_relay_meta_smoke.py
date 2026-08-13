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
import select
import signal
import socket
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

import pytest

from netbot.framing import FramingError, MsgType, build_frame, parse_frames

TEST_RELAY_SECRET = "test-relay-secret"

# server/ip_admission.h 의 kMaxHandshakesPerIp / kMaxSessionsPerIp 와 같아야 한다.
# 두 릴레이 바이너리가 이 헤더를 공유하므로 값도 하나뿐이다.
RELAY_MAX_HANDSHAKES_PER_IP = 16
RELAY_MAX_SESSIONS_PER_IP = 64


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
        try:
            frames = parse_frames(buf)
        except FramingError:
            # C++ 대응(parse_frames false → 호출자 close): relay 가 오버사이즈
            # 길이를 선언했다는 뜻. 버퍼는 parse_frames 가 이미 비웠으니
            # 소켓만 닫고 그대로 테스트 실패로 띄운다.
            sock.close()
            raise
        for t, p in frames:
            if t == MsgType.MATCH_FOUND:
                role = p[0]
                seed = struct.unpack_from("<Q", p, 1)[0]
                return role, seed
    raise TimeoutError("no MATCH_FOUND within deadline")


def _recv_frame(sock: socket.socket, want: MsgType, buf: bytearray,
                timeout: float = 5.0) -> bytes:
    """want 타입 프레임이 올 때까지 읽는다. buf 는 호출자가 보관 — 한 read 에
    여러 프레임이 실려 오는 경우 나머지를 잃지 않기 위해서다."""
    sock.settimeout(timeout)
    deadline = time.monotonic() + timeout
    while True:
        for t, p in parse_frames(buf):
            if t == want:
                return p
        if time.monotonic() >= deadline:
            raise TimeoutError(f"no {want!r} within deadline")
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError(f"relay closed before {want!r}")
        buf.extend(chunk)


def _build_room_create(token: str) -> bytes:
    return build_frame(MsgType.ROOM_CREATE,
                       bytes([len(token)]) + token.encode("ascii"))


def _build_room_join(code: str, token: str) -> bytes:
    payload = (bytes([len(code)]) + code.encode("ascii")
               + bytes([len(token)]) + token.encode("ascii"))
    return build_frame(MsgType.ROOM_JOIN, payload)


def _parse_room_info(payload: bytes) -> tuple[str, int, int]:
    code_len = payload[0]
    code = payload[1:1 + code_len].decode("ascii")
    return code, payload[1 + code_len], payload[2 + code_len]


def test_relay_sigterm_drains_active_match() -> None:
    """SIGTERM 중 active forwarder가 server-owned state보다 먼저 종료된다."""
    relay_bin = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    if not relay_bin:
        pytest.skip("tetris_relay binary missing")

    port = _free_port()
    # Windows 의 proc.terminate() 는 TerminateProcess(handle, 1) — 시그널
    # 핸들러가 실행될 기회 자체가 없어 graceful shutdown 경로를 검증하지
    # 못한다. 대신 relay 를 새 프로세스 그룹으로 띄우고 CTRL_BREAK_EVENT
    # (자식 입장에서는 SIGBREAK — 서버가 핸들러를 등록해 둠) 를 보내
    # 핸들러 경유 종료를 유도한다. POSIX 는 기존 SIGTERM(terminate()) 유지.
    creationflags = (
        subprocess.CREATE_NEW_PROCESS_GROUP if sys.platform == "win32" else 0
    )
    proc = subprocess.Popen(
        [str(relay_bin), "--port", str(port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=creationflags,
    )
    if not _wait_listen(port, 5.0):
        proc.kill()
        pytest.fail("tetris_relay failed to listen")

    a = socket.create_connection(("127.0.0.1", port), timeout=1.0)
    b = socket.create_connection(("127.0.0.1", port), timeout=1.0)
    try:
        a.sendall(_build_queue_join(""))
        b.sendall(_build_queue_join(""))
        _recv_match_found(a)
        _recv_match_found(b)
        a.sendall(build_frame(MsgType.READY, b"\x01"))
        b.sendall(build_frame(MsgType.READY, b"\x01"))
        time.sleep(0.1)

        if sys.platform == "win32":
            proc.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            proc.terminate()
        try:
            return_code = proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            pytest.fail("relay did not drain active match within 3 seconds")
        # POSIX 는 SIGTERM 핸들러, Windows 는 SIGBREAK 핸들러를 거쳐 정상
        # 종료하므로 양쪽 다 종료 코드 0 을 기대한다 (0 이 아니면 핸들러를
        # 안 거쳤거나 drain 에 실패한 것).
        assert return_code == 0
    finally:
        a.close()
        b.close()
        if proc.poll() is None:
            proc.kill()


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
        [str(meta_bin), "--db", str(db), "--http", f"127.0.0.1:{meta_port}",
         "--relay-secret", TEST_RELAY_SECRET],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if not _wait_listen(meta_port, 5.0):
        meta_proc.kill()
        pytest.fail("tetris_meta failed to listen")

    relay_proc = subprocess.Popen(
        [str(relay_bin), "--port", str(relay_port),
         "--meta", f"http://127.0.0.1:{meta_port}",
         "--meta-secret", TEST_RELAY_SECRET],
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


def test_same_player_cannot_queue_twice(meta_and_relay):
    base = meta_and_relay["meta_url"]
    rh = meta_and_relay["relay_host"]
    rp = meta_and_relay["relay_port"]
    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    first = socket.create_connection((rh, rp), timeout=2.0)
    duplicate = socket.create_connection((rh, rp), timeout=2.0)
    peer = socket.create_connection((rh, rp), timeout=2.0)
    try:
        # 예전에는 first 송신 후 time.sleep(0.1) 으로 "first 가 먼저 큐에
        # 들어간다" 는 순서를 가정했지만, relay 의 verify 는 접속별 스레드에서
        # 돌기 때문에 어느 쪽이 먼저 처리될지는 보장이 없다 (느린 CI 에서
        # flaky). 순서 가정을 버리고 대칭으로 검증한다: 같은 player 의 두
        # 큐 진입 중 정확히 하나만 relay 가 닫아야 하고, 살아남은 쪽이 누구든
        # peer 와 정상 매칭되면 통과다.
        first.sendall(_build_queue_join(p1["token"]))
        duplicate.sendall(_build_queue_join(p1["token"]))

        rejected = None
        deadline = time.monotonic() + 5.0
        while rejected is None and time.monotonic() < deadline:
            readable, _, _ = select.select([first, duplicate], [], [], 0.1)
            for sock in readable:
                try:
                    data = sock.recv(4096)
                except ConnectionResetError:
                    data = b""
                # 거절된 쪽은 EOF(또는 reset)만 보여야 한다 — 프레임이 오면
                # 같은 player 둘이 서로 매칭됐다는 뜻이므로 즉시 실패.
                assert data == b"", "rejected side must see EOF, not frames"
                rejected = sock
                break
        assert rejected is not None, "relay must close one of the duplicate joins"
        survivor = first if rejected is duplicate else duplicate

        peer.sendall(_build_queue_join(p2["token"]))
        role_survivor, seed_survivor = _recv_match_found(survivor)
        role_peer, seed_peer = _recv_match_found(peer)
        assert seed_survivor == seed_peer
        assert {role_survivor, role_peer} == {1, 2}
    finally:
        first.close(); duplicate.close(); peer.close()


def test_ranked_room_create_join_pairs(meta_and_relay):
    """인증된 연결의 ROOM_CREATE/ROOM_JOIN 이 룸으로 가야 한다.

    룸 스모크는 tok_len=0 (unranked) 으로만 이 경로를 밟기 때문에, 인증을 거친
    뒤 진로를 고르는 분기가 오래 무검증으로 남아 있었다. reactor 릴레이에서
    실제로 그 분기가 빠져 랭크드 룸 요청이 전부 매치메이킹 큐로 끌려갔다.
    """
    base = meta_and_relay["meta_url"]
    rh   = meta_and_relay["relay_host"]
    rp   = meta_and_relay["relay_port"]

    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    a = socket.create_connection((rh, rp), timeout=2.0)
    b = socket.create_connection((rh, rp), timeout=2.0)
    a_buf, b_buf = bytearray(), bytearray()
    try:
        a.sendall(_build_room_create(p1["token"]))
        # 큐로 샜다면 여기서 ROOM_INFO 대신 아무것도 오지 않는다.
        code, status, peers = _parse_room_info(_recv_frame(a, MsgType.ROOM_INFO, a_buf))
        assert len(code) == 5
        assert (status, peers) == (0, 1)

        b.sendall(_build_room_join(code, p2["token"]))
        _, status_b, peers_b = _parse_room_info(_recv_frame(b, MsgType.ROOM_INFO, b_buf))
        assert (status_b, peers_b) == (0, 2)

        a.sendall(build_frame(MsgType.READY, b"\x01"))
        b.sendall(build_frame(MsgType.READY, b"\x01"))

        mf_a = _recv_frame(a, MsgType.MATCH_FOUND, a_buf)
        mf_b = _recv_frame(b, MsgType.MATCH_FOUND, b_buf)
        role_a, seed_a = mf_a[0], struct.unpack_from("<Q", mf_a, 1)[0]
        role_b, seed_b = mf_b[0], struct.unpack_from("<Q", mf_b, 1)[0]
        assert seed_a == seed_b
        assert {role_a, role_b} == {1, 2}
    finally:
        a.close(); b.close()


def test_ranked_auth_releases_handshake_slot(meta_and_relay):
    """per-IP 핸드셰이크 예산은 인증이 끝나는 순간 반납돼야 한다.

    반납하지 않으면 상한(16)이 사실상 '동시 세션 16'이 되어 같은 IP 뒤에 오는
    접속이 굶는다 — NAT 뒤 다수 사용자가 서로를 밀어내고, loopback 테스트도
    상한에 걸린다. 핸드셰이크 상한보다 넉넉히 많되 세션 상한(아래 테스트)에는
    닿지 않는 수를 한 주소에서 붙여 확인한다.

    두 바이너리가 같은 정책을 갖게 된 뒤로는 reactor 전용 skip 가드가 없다.
    """
    base = meta_and_relay["meta_url"]
    rh   = meta_and_relay["relay_host"]
    rp   = meta_and_relay["relay_port"]

    total = RELAY_MAX_HANDSHAKES_PER_IP + 8   # 핸드셰이크 상한보다 확실히 크게
    assert total < RELAY_MAX_SESSIONS_PER_IP, "세션 상한에 닿으면 다른 것을 재게 된다"
    socks: list[socket.socket] = []
    try:
        for i in range(total):
            tok = _post(f"{base}/v1/guest")["token"]
            s = socket.create_connection((rh, rp), timeout=2.0)
            socks.append(s)
            s.sendall(_build_room_create(tok))
            # 슬롯이 안 풀리면 17번째부터 relay 가 EOF 로 끊는다.
            code, _, _ = _parse_room_info(
                _recv_frame(s, MsgType.ROOM_INFO, bytearray(), timeout=5.0))
            assert len(code) == 5, f"connection {i} got a malformed room code"
    finally:
        for s in socks:
            s.close()


def test_per_ip_session_cap_rejects_excess_connections(tmp_path):
    """세션 슬롯은 연결이 죽을 때까지 유지된다 — 한 IP 가 서버를 독식하지 못하게.

    핸드셰이크 슬롯만 두면 인증만 통과시키며 전역 상한(reactor kMaxConns=512,
    스레드 모델 포워딩 워커 512)까지 한 주소가 전부 차지할 수 있다. 상한이
    아니라 속도 제한일 뿐이다.

    한 주소에서 kMaxSessionsPerIp 개를 붙여 전부 살아남는지 (상한이 정상
    사용자 집단을 자르지 않는지) 확인하고, 그 다음 하나가 거절되는지 (상한이
    실제로 존재하는지) 확인한다. 두 방향 모두 필요하다.

    unranked(tok_len=0) 로 돌리므로 meta 는 필요 없다. ROOM_CREATE 를 쓰는
    이유는 연결이 응답(ROOM_INFO)을 받은 뒤에도 계속 살아 있어 세션 슬롯을
    붙들고 있기 때문이다 — 큐 경로는 둘씩 짝지어져 상태가 복잡해진다.
    """
    relay_bin = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    if not relay_bin:
        pytest.skip("tetris_relay binary missing")

    port = _free_port()
    # 로그는 버린다. 릴레이는 연결마다 여러 줄을 찍는데, PIPE 로 받아 놓고 아무도
    # 읽지 않으면 64 KiB 짜리 파이프 버퍼가 차는 순간 릴레이가 write 에서 멈춘다 —
    # 연결 수십 개를 붙이는 이 테스트에서는 그게 "상한에 걸렸다" 와 구분되지 않는
    # 타임아웃으로 나타난다.
    proc = subprocess.Popen(
        [str(relay_bin), "--port", str(port)],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    if not _wait_listen(port, 5.0):
        proc.kill()
        pytest.fail("relay failed to listen")
    # _wait_listen 의 탐침 연결도 슬롯을 하나 썼다. 그 반납이 끝나기 전에 세기
    # 시작하면 마지막 하나가 억울하게 거절된다.
    time.sleep(0.3)

    socks: list[socket.socket] = []
    try:
        for i in range(RELAY_MAX_SESSIONS_PER_IP):
            s = socket.create_connection(("127.0.0.1", port), timeout=2.0)
            socks.append(s)
            s.sendall(build_frame(MsgType.ROOM_CREATE, b"\x00"))
            code, _, _ = _parse_room_info(
                _recv_frame(s, MsgType.ROOM_INFO, bytearray(), timeout=5.0))
            assert len(code) == 5, (
                f"connection {i} was refused below the per-IP session cap "
                f"({RELAY_MAX_SESSIONS_PER_IP}) — 상한이 너무 빡빡하다")

        # 상한을 한 칸 넘긴 연결: relay 가 accept 직후 닫아야 한다.
        extra = socket.create_connection(("127.0.0.1", port), timeout=2.0)
        socks.append(extra)
        extra.sendall(build_frame(MsgType.ROOM_CREATE, b"\x00"))
        extra.settimeout(3.0)
        try:
            data = extra.recv(4096)
        except ConnectionResetError:
            data = b""
        except socket.timeout:
            pytest.fail("상한 초과 연결이 ROOM_INFO 도 못 받고 닫히지도 않았다")
        assert data == b"", (
            "per-IP 세션 상한을 넘긴 연결이 살아남았다 — 한 주소가 전역 상한까지 "
            "연결을 쌓을 수 있다는 뜻")
    finally:
        for s in socks:
            s.close()
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


def test_reactor_loops_two_falls_back_to_single_loop():
    """--loops 2 는 스레드만 하나 더 쓰고 아무것도 사지 못한다.

    앞단 루프는 샤드가 하나라도 생기면 포워딩을 전부 넘기고 자기는 하지 않는다.
    그래서 포워딩 일꾼 수는 loops-1 이고, loops=2 의 일꾼은 1개 — 앞단이 직접
    포워딩하는 loops=1 과 같은 수다. 측정에서도 두 구성의 처리량 차이는 0.14%
    (374,360 vs 373,827 frames/s) 로 오차 범위였고, 늘어나는 것은 스레드 하나와
    매치마다 도는 우편함 인계뿐이다.

    릴레이는 이 값을 거절하는 대신 1 로 낮춘다 (틀린 설정이 아니라 무의미한
    설정이고, 이미 그 인자를 박아 둔 스크립트를 멈춰 세울 이유가 없다). 대신
    조용히 넘어가지 않는다 — 이 테스트가 그 "조용하지 않음"을 못 박는다.

    이 인자는 reactor 바이너리 전용이므로 TETRIS_RELAY_BIN 이 무엇을 가리키든
    reactor 를 이름으로 직접 찾는다 (그래야 스레드 모델 스모크에서도 돈다).
    """
    reactor_bin = _find_bin("tetris_relay_reactor", "TETRIS_RELAY_REACTOR_BIN")
    if not reactor_bin:
        pytest.skip("tetris_relay_reactor binary missing")

    # --help 는 실효 병렬도가 loops-1 이라는 사실을 밝혀야 한다. 이 문장이
    # 사라지면 사용자는 --loops 2 가 왜 손해인지 알 길이 없다.
    help_proc = subprocess.run([str(reactor_bin), "--help"],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=10)
    help_text = help_proc.stdout.decode("utf-8", errors="replace")
    assert "loops-1" in help_text, "--help 가 실효 병렬도(loops-1)를 말하지 않는다"

    port = _free_port()
    # 종료를 핸들러 경유로 유도해야 stdout 버퍼가 flush 된다 (파이프로 받으면
    # 완전 버퍼링이라 강제 종료 시 아무것도 안 남는다).
    creationflags = (
        subprocess.CREATE_NEW_PROCESS_GROUP if sys.platform == "win32" else 0
    )
    proc = subprocess.Popen(
        [str(reactor_bin), "--port", str(port), "--loops", "2"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        creationflags=creationflags,
    )
    try:
        if not _wait_listen(port, 5.0):
            proc.kill()
            pytest.fail("tetris_relay_reactor failed to listen")
        if sys.platform == "win32":
            proc.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            proc.terminate()
        out = proc.communicate(timeout=10)[0].decode("utf-8", errors="replace")
    finally:
        if proc.poll() is None:
            proc.kill()

    assert "--loops 2" in out, (
        "--loops 2 를 조용히 받아들였다 — 사용자는 손해를 본 줄도 모른다:\n" + out)
    assert "forwarding shards" not in out, (
        "--loops 2 가 샤드를 만들었다 — 일꾼 수는 그대로인데 스레드와 인계 "
        "비용만 늘어난 구성이다:\n" + out)


def test_relay_refuses_meta_without_secret(tmp_path):
    relay_bin = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    if not relay_bin:
        pytest.skip("tetris_relay binary missing")

    relay_port = _free_port()
    env = os.environ.copy()
    env.pop("TETRIS_RELAY_SECRET", None)
    proc = subprocess.Popen(
        [str(relay_bin), "--port", str(relay_port),
         "--meta", "http://127.0.0.1:1"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
    )
    try:
        rc = proc.wait(timeout=3)
        stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
        assert rc == 2
        assert "refusing to start" in stderr
    finally:
        if proc.poll() is None:
            proc.kill()
