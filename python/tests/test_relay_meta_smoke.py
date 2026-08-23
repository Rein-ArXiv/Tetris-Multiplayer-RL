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
import threading
import time
import urllib.request
from pathlib import Path

import pytest

from netbot.framing import (
    FramingError,
    MsgType,
    RejectReason,
    build_frame,
    parse_frames,
)

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


def _spawn_listening(argv_for_port, tries: int = 4, creationflags: int = 0):
    """포트를 잡아 프로세스를 띄우고, 실제로 듣기 시작하면 (proc, port) 를 준다.

    _free_port 는 커널이 준 번호를 돌려주고 곧바로 소켓을 닫는다. 그 사이에 다른
    연결이 같은 번호를 가져가면 자식은 bind 에 실패하고, 테스트는 "기동 실패" 로
    끝난다 — 코드가 틀려서가 아니라 번호를 뺏겨서다. 스위트가 프로세스를 수십 번
    띄우는 동안 이 경합은 드물지 않게 터지고, 그때마다 무관한 테스트가 빨갛게
    된다. 번호를 뺏긴 것뿐이면 새 번호로 다시 시도하는 것이 맞다.

    마지막 시도까지 실패하면 자식이 남긴 출력을 함께 올려 준다. 진짜 기동 실패
    (인자 오류 등)와 번호 경합을 로그 없이 구분할 수 없기 때문이다.
    """
    last_output = ""
    for attempt in range(tries):
        port = _free_port()
        proc = subprocess.Popen(argv_for_port(port),
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                creationflags=creationflags)
        if _wait_listen(port, 5.0):
            # 파이프를 아무도 안 읽으면 버퍼가 차는 순간 자식이 write 에서 멈춘다.
            # 실패 진단용으로 PIPE 로 띄웠으니, 뜨고 나면 계속 비워 준다.
            # (상태 줄을 직접 읽어야 하는 테스트는 _StatsReader 로 따로 띄운다.)
            drain = threading.Thread(target=lambda: proc.stdout.read(), daemon=True)
            drain.start()
            return proc, port
        proc.kill()
        try:
            last_output = proc.communicate(timeout=2)[0].decode("utf-8", "replace")
        except Exception:
            last_output = "(자식 출력을 읽지 못함)"
        # bind 실패가 아니라 진짜 기동 실패라면 재시도해도 같은 출력이 반복된다.
    raise AssertionError(
        f"프로세스가 {tries}회 시도에도 듣지 않았다. 마지막 자식 출력:\n{last_output}")


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

    # Windows 의 proc.terminate() 는 TerminateProcess(handle, 1) — 시그널
    # 핸들러가 실행될 기회 자체가 없어 graceful shutdown 경로를 검증하지
    # 못한다. 대신 relay 를 새 프로세스 그룹으로 띄우고 CTRL_BREAK_EVENT
    # (자식 입장에서는 SIGBREAK — 서버가 핸들러를 등록해 둠) 를 보내
    # 핸들러 경유 종료를 유도한다. POSIX 는 기존 SIGTERM(terminate()) 유지.
    creationflags = (
        subprocess.CREATE_NEW_PROCESS_GROUP if sys.platform == "win32" else 0
    )
    proc, port = _spawn_listening(
        lambda p: [str(relay_bin), "--port", str(p)],
        creationflags=creationflags)

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

    db = tmp_path / "test.db"
    meta_proc, meta_port = _spawn_listening(lambda port: [
        str(meta_bin), "--db", str(db), "--http", f"127.0.0.1:{port}",
        "--relay-secret", TEST_RELAY_SECRET])

    try:
        relay_proc, relay_port = _spawn_listening(lambda port: [
            str(relay_bin), "--port", str(port),
            "--meta", f"http://127.0.0.1:{meta_port}",
            "--meta-secret", TEST_RELAY_SECRET])
    except AssertionError:
        meta_proc.kill()
        raise

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

    핸드셰이크 슬롯만 두면 인증만 통과시키며 전역 상한(reactor --max-conns,
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
        #
        # "아무것도 안 오면 통과" 로는 더 이상 판정하지 않는다. 루프 릴레이는 닫기
        # 전에 사유(SERVER_REJECT)를 한 프레임 내려보내므로, 조용한 끊김만 정답으로
        # 두면 사유를 밝히는 쪽이 실패한다. 정작 지켜야 할 성질은 그게 아니라
        # "입장하지 못하고(ROOM_INFO 없음) 곧 닫힌다(EOF)" 이다. 스레드 모델은
        # 여전히 아무 말 없이 닫으므로 두 동작을 모두 받아들인다.
        extra = socket.create_connection(("127.0.0.1", port), timeout=2.0)
        socks.append(extra)
        extra.sendall(build_frame(MsgType.ROOM_CREATE, b"\x00"))
        extra.settimeout(3.0)
        received = bytearray()
        closed = False
        try:
            while True:
                data = extra.recv(4096)
                if not data:
                    closed = True
                    break
                received += data
        except socket.timeout:
            pytest.fail("상한 초과 연결이 ROOM_INFO 도 못 받고 닫히지도 않았다")
        except ConnectionError:
            # 끊김이 깔끔한 EOF 로 오는지 RST/abort 로 오는지는 OS 와 타이밍이
            # 정한다 (Windows 는 10053/10054 를 모두 낸다). 어느 쪽이든 "닫혔다"
            # 는 같은 사실이므로 ConnectionError 계열을 통째로 받는다 — 하나만
            # 잡으면 같은 동작이 플랫폼에 따라 실패로 뒤집힌다.
            closed = True

        frames = parse_frames(bytearray(received))
        types = [t for t, _ in frames]
        assert MsgType.ROOM_INFO not in types, (
            "per-IP 세션 상한을 넘긴 연결이 방을 받았다 — 한 주소가 전역 상한까지 "
            f"연결을 쌓을 수 있다는 뜻 (frames={types})")
        assert closed, (
            "per-IP 세션 상한을 넘긴 연결이 살아남았다 — 상한이 실효가 없다")
        for msg_type, payload in frames:
            assert msg_type == MsgType.SERVER_REJECT, (
                f"거절 직전에 예상 밖의 프레임을 보냈다: {msg_type}")
            assert payload and payload[0] == RejectReason.IP_SESSION_LIMIT, (
                "거절 사유가 per-IP 세션 상한이 아니다: "
                f"{payload[0] if payload else None}")
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


def test_reactor_bounds_the_pending_auth_queue():
    """인증 대기 줄에 상한이 있고, 그 예산이 돌아와야 한다.

    루프 릴레이는 meta 검증을 오프로드 워커 풀로 뺀다. 죽은 연결의 작업은 이제
    취소되지만(그게 굶김의 주된 원인이었다) 취소만으로는 절반이다 — 살아 있는
    연결이 몰려오면 줄은 여전히 길어지고, 뒤에 선 사람은 자기 클라이언트가
    포기할 때까지 기다린다. 그래서 대기 줄에 상한을 두고, 넘기면 세우는 대신
    사유를 밝히고 거절한다 (스레드 모델이 워커가 다 찼을 때 하는 것과 같다).

    여기서 정말 무서운 실패는 상한이 없는 것이 아니라 **예산이 안 돌아오는
    것**이다. 새면 서버가 멀쩡한데도 상한만큼 로그인이 지나간 뒤로는 아무도
    인증하지 못하고, 밖에서 보이는 증상은 "어느 순간부터 로그인이 안 된다" 뿐
    이다. 그래서 두 방향을 함께 잰다: 상한이 실제로 걸리는가, 그리고 걸린 뒤
    풀리는가.

    meta 는 붙기만 하고 대답하지 않는 소켓으로 세운다 — 인증을 확실히 "대기
    중" 상태로 붙들어 두는 가장 단순한 방법이고, 지연 주입 장치가 필요 없다.

    ``--max-pending-auth`` 는 reactor 바이너리 전용이므로 TETRIS_RELAY_BIN 이
    무엇을 가리키든 reactor 를 이름으로 직접 찾는다.
    """
    reactor_bin = _find_bin("tetris_relay_reactor", "TETRIS_RELAY_REACTOR_BIN")
    if not reactor_bin:
        pytest.skip("tetris_relay_reactor binary missing")

    # 대답하지 않는 meta. accept 만 하고 붙들고 있으면 릴레이의 verify_token 은
    # 자기 read timeout(3초)까지 그 워커에 매달린다.
    silent = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    silent.bind(("127.0.0.1", 0))
    silent.listen(16)
    meta_port = silent.getsockname()[1]
    held: list[socket.socket] = []

    def _accept_and_hold():
        while True:
            try:
                conn, _ = silent.accept()
            except OSError:
                return
            held.append(conn)      # 응답하지 않는다

    threading.Thread(target=_accept_and_hold, daemon=True).start()

    port = _free_port()
    proc = subprocess.Popen(
        [str(reactor_bin), "--port", str(port),
         "--meta", f"http://127.0.0.1:{meta_port}",
         "--meta-secret", TEST_RELAY_SECRET,
         "--max-pending-auth", "1"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    socks: list[socket.socket] = []

    def _join(tok: str) -> socket.socket:
        s = socket.create_connection(("127.0.0.1", port), timeout=3.0)
        socks.append(s)
        s.sendall(_build_room_create(tok))
        return s

    try:
        if not _wait_listen(port, 5.0):
            pytest.fail("tetris_relay_reactor failed to listen")

        # 1) 한 명이 줄을 차지한다 (meta 가 대답하지 않으므로 대기 상태로 남는다).
        first = _join("a" * 32)
        time.sleep(0.3)

        # 2) 상한을 넘긴 다음 사람은 기다리는 대신 사유를 받고 끊겨야 한다.
        payload = _recv_frame(second := _join("b" * 32),
                              MsgType.SERVER_REJECT, bytearray(), timeout=5.0)
        assert payload and payload[0] == RejectReason.AUTH_BACKLOG, (
            "인증 대기 상한을 넘긴 연결이 다른 사유로 거절됐다 — 사유 코드 "
            f"{payload[0] if payload else '(없음)'}")
        second.settimeout(5.0)
        assert second.recv(4096) == b"", "거절한 연결을 닫지 않았다"

        # 3) 앞사람의 인증이 실패로 끝나면 예산이 돌아와야 한다. 새면 이 뒤로는
        #    아무도 인증 줄에 들어가지 못한다.
        first.settimeout(15.0)
        deadline = time.monotonic() + 15.0
        while time.monotonic() < deadline:
            if first.recv(4096) == b"":
                break
        else:
            pytest.fail("meta 무응답인데 앞사람의 연결이 정리되지 않았다")

        third = _join("c" * 32)
        third.settimeout(6.0)
        buf = bytearray()
        deadline = time.monotonic() + 6.0
        while time.monotonic() < deadline:
            try:
                chunk = third.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            buf.extend(chunk)
        # 이 연결도 결국 거절된다(meta 가 대답하지 않으므로). 하지만 그 사유가
        # "인증 대기 상한" 이면 앞사람이 비운 자리가 돌아오지 않았다는 뜻이다.
        for msg_type, payload in parse_frames(buf):
            assert not (msg_type == MsgType.SERVER_REJECT
                        and payload and payload[0] == RejectReason.AUTH_BACKLOG), (
                "앞사람이 떠났는데도 인증 대기 상한으로 거절했다 — 예산이 "
                "돌아오지 않는다")
    finally:
        for s in socks:
            s.close()
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
        silent.close()
        for c in held:
            c.close()


def test_queued_client_cannot_grow_the_relay_buffer(meta_and_relay):
    """큐 대기 중 흘려보낸 바이트가 무한히 쌓이면 안 된다.

    큐 단계는 잔여 바이트를 뒤 단계로 넘겨야 해서 rx 를 소비하지 않고 쌓아 둔다.
    그런데 바이트 레이트 상한이 Forward 단계에만 걸려 있었고 버퍼 상한도 없어,
    큐에 들어간 클라이언트 하나가 메모리를 무한히 먹을 수 있었다. 재파싱이 매
    읽기마다 돌기 때문에 비용이 O(n^2) 로 자라 단일 루프 스레드를 통째로 잡는다 —
    진행 중인 모든 매치가 함께 멈춘다.

    QUEUE_CANCEL 이 아닌 유효 프레임(CHAT)을 상한 위로 부어 릴레이가 끊는지 본다.
    """
    base = meta_and_relay["meta_url"]
    rh   = meta_and_relay["relay_host"]
    rp   = meta_and_relay["relay_port"]

    tok = _post(f"{base}/v1/guest")["token"]
    s = socket.create_connection((rh, rp), timeout=2.0)
    try:
        s.sendall(_build_queue_join(tok))
        # 상대가 없으니 큐에 남는다. 여기에 CHAT 을 들이붓는다.
        chat = build_frame(MsgType.CHAT, b"x" * 512)
        s.settimeout(5.0)
        closed = False
        sent = 0
        # 버퍼 상한(64 KiB)과 레이트 상한(64 KiB/s) 중 어느 쪽에 먼저 걸리든
        # 결과는 같아야 한다 — 릴레이가 이 연결을 끊는다.
        for _ in range(2000):          # 최대 1 MiB
            try:
                s.sendall(chat)
                sent += len(chat)
            except (BrokenPipeError, ConnectionResetError, OSError):
                closed = True
                break
        if not closed:
            try:
                closed = s.recv(4096) == b""
            except (socket.timeout, ConnectionResetError):
                closed = True
        assert closed, (
            f"큐 대기 중 {sent} 바이트를 부었는데도 릴레이가 연결을 유지한다 "
            "— 상한이 없다")
    finally:
        s.close()


def test_ranked_room_join_ready_coalesced_with_host_already_ready(meta_and_relay):
    """게스트의 ROOM_JOIN 과 READY 가 한 세그먼트에 실려 오고 호스트가 이미 READY 인 경우.

    room_join 은 on_room(c) 를 부른 뒤 호스트의 잔여 바이트도 처리하려고 Room 을
    다시 만졌는데, 그 사이 on_room 이 양쪽 READY 를 관측하면 start_room_match 가
    Room 을 파괴한다 — use-after-free 다.

    주의: 이 테스트는 회귀 테스트가 아니라 **가드**다. 해제된 메모리가 아직 멀쩡해
    보이면 수정 전에도 통과한다. 값은 경로를 밟아 둔다는 데 있고, 진짜 검출은
    새니타이저를 켠 빌드에서 이 경로를 돌릴 때 나온다.
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
        code, _, _ = _parse_room_info(_recv_frame(a, MsgType.ROOM_INFO, a_buf))
        # 호스트가 먼저 READY 를 확정해 둔다.
        a.sendall(build_frame(MsgType.READY, b"\x01"))
        time.sleep(0.2)
        # 게스트는 JOIN 과 READY 를 한 번에 — 릴레이가 같은 recv 로 받게 된다.
        b.sendall(_build_room_join(code, p2["token"]) + build_frame(MsgType.READY, b"\x01"))

        mf_a = _recv_frame(a, MsgType.MATCH_FOUND, a_buf)
        mf_b = _recv_frame(b, MsgType.MATCH_FOUND, b_buf)
        assert struct.unpack_from("<Q", mf_a, 1)[0] == struct.unpack_from("<Q", mf_b, 1)[0]
        assert {mf_a[0], mf_b[0]} == {1, 2}
    finally:
        a.close(); b.close()


def test_backpressure_does_not_disconnect_the_sender(meta_and_relay):
    """느리게 읽는 상대 때문에 정상 송신자가 끊기면 안 된다.

    상대의 tx 가 high-water 를 넘으면 릴레이는 흘려보내는 쪽의 읽기를 멈춘다. 멈춘
    연결은 읽기 이벤트가 나지 않으므로 유휴 데드라인이 갱신되지 않는데, 유휴 판정이
    그걸 그대로 보면 "안 읽는 쪽" 이 아니라 "보내는 쪽" 이 idle 타임아웃으로 끊긴다.
    피해자와 가해자가 뒤바뀐다.

    A 가 꾸준히 보내고 B 는 전혀 읽지 않는 상태를 유휴 타임아웃(15초)보다 오래 유지한
    뒤, A 가 살아 있는지 본다. 느린 테스트지만 이 경로를 재는 방법이 이것뿐이다.

    reactor 전용이다. 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다 — 방향별 스레드가
    tcp_send_all 안에서 최대 5초 잠들며 버티다 실패하면 매치를 접는다. 안 읽는 상대를
    다루는 방식 자체가 달라서 같은 계약을 겨눌 수 없다.
    """
    relay_bin = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")
    if relay_bin is None or "reactor" not in relay_bin.name:
        pytest.skip("스레드 모델은 백프레셔 대신 블로킹 send 재시도 — 계약이 다름")
    if sys.platform.startswith("linux"):
        # 이 테스트는 Linux 에서 계약을 재지 못한다. 릴레이가 틀려서가 아니라
        # 재는 방법이 성립하지 않아서다. 배포 대상에서 계측해 확인한 것은 이렇다.
        #
        # 릴레이의 송신 큐가 B 를 향해 가득 차고 나면 B 가 보내는 keepalive 가
        # 릴레이에 도달하지 않는다 — ss 로 보면 B 쪽 Send-Q 는 쌓이는데 릴레이
        # 쪽 Recv-Q 는 0 이고, B 가 자기 수신 버퍼를 전부 비워도 풀리지 않는다.
        # 릴레이는 그동안 B 를 읽기 관심에 정상 등록해 두고 있고(interest=3),
        # epoll 이 아무것도 보고하지 않는 이유는 도착한 것이 없기 때문이다.
        # 그래서 릴레이는 15초 동안 조용한 B 를 유휴로 걷어가고, 그 여파로 A 까지
        # 닫혀 정작 재려던 조건을 관측할 수 없다.
        #
        # 계약 자체는 Linux 에서도 지켜진다. 같은 시나리오를 계측해 보면 백프레셔로
        # 멈춰 세운 A 는 유휴로 닫히지 않고, 끊기는 것은 언제나 안 읽는 B 쪽이다.
        # A 는 상대가 사라진 뒤에야 "상대 이탈" 로 닫힌다.
        #
        # 테스트가 B 를 "전혀 안 읽는" 대신 "느리게 읽는" 상대로 바꾸면 세 단계가
        # 모두 Linux 에서 돈다. 다만 그 형태는 tx 가 high-water 와 하드 상한 사이에
        # 머물러야 성립해서 커널 버퍼 크기에 민감하고, Windows 에서는 tx 가 하드
        # 상한을 넘겨 릴레이가 연결을 끊는다. 한쪽을 살리면 다른 쪽이 죽는 형태라
        # 지금은 원래 형태를 유지하고 Linux 에서만 건너뛴다.
        pytest.skip(
            "Linux 에서는 안 읽는 피어의 keepalive 가 전송 계층에서 막혀 "
            "이 계약을 관측할 수 없다 (릴레이 동작은 계측으로 확인함)")

    base = meta_and_relay["meta_url"]
    rh   = meta_and_relay["relay_host"]
    rp   = meta_and_relay["relay_port"]

    p1 = _post(f"{base}/v1/guest")
    p2 = _post(f"{base}/v1/guest")

    a = socket.create_connection((rh, rp), timeout=2.0)
    b = socket.create_connection((rh, rp), timeout=2.0)
    # B 의 수신 버퍼를 좁혀 릴레이의 tx 가 빨리 차게 한다.
    b.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    try:
        a.sendall(_build_queue_join(p1["token"]))
        b.sendall(_build_queue_join(p2["token"]))
        _recv_match_found(a)
        _recv_match_found(b)
        a.sendall(build_frame(MsgType.READY, b"\x01"))
        b.sendall(build_frame(MsgType.READY, b"\x01"))
        time.sleep(0.3)

        # B 는 이 아래로 단 한 바이트도 읽지 않는다. 다만 자기 유휴 데드라인은
        # 살려 둬야 한다 — B 가 스스로 idle 로 끊기면 그 여파로 A 도 닫혀서, 정작
        # 재려던 조건(백프레셔에 멈춘 A 가 끊기는가)을 못 본다.
        keepalive = build_frame(MsgType.INPUT, struct.pack("<IH", 0, 1) + b"\x00")
        # A 는 상대의 tx 를 high-water(256 KiB) 위로 밀어 올릴 만큼 보낸다. 바이트
        # 레이트 상한(64 KiB/s) 아래를 유지해야 그쪽에 먼저 걸리지 않는다.
        chunk = build_frame(MsgType.CHAT, b"x" * 400)          # ≈ 411 B
        # 아래 루프는 50 ms 마다 돈다. 5 × 411 B × 20 회/초 ≈ 41 KiB/s 로,
        # 바이트 레이트 상한(64 KiB/s) 아래다 — 그쪽에 먼저 걸리면 재려던 조건이
        # 아니라 레이트 초과로 끊긴다.
        per_tick = 5
        a.settimeout(2.0)
        b.settimeout(2.0)

        def _fail(exc, phase):
            pytest.fail(f"[{phase}] 릴레이가 연결을 끊었다: {exc!r} — 백프레셔로 "
                        "멈춰 세운 송신자를 유휴로 오인하고 있다")

        # 1단계 — A 를 밀어 올려 릴레이가 A 의 읽기를 멈추게 한다.
        # 멈추면 릴레이가 A 소켓에서 더 이상 읽지 않으므로 A 의 커널 송신 버퍼가
        # 차고 sendall 이 블록된다. 그 timeout 이 "멈춰 세워졌다" 는 신호다 —
        # 끊긴 것과 구분해야 한다(끊기면 reset/abort 가 온다).
        paused = False
        t0 = time.monotonic()
        while time.monotonic() - t0 < 20.0:
            try:
                for _ in range(per_tick):
                    a.sendall(chunk)
                b.sendall(keepalive)
            except socket.timeout:
                paused = True
                break
            except (BrokenPipeError, ConnectionResetError, OSError) as exc:
                _fail(exc, "1단계: 밀어 올리는 중")
            time.sleep(0.05)
        assert paused, "high-water 를 넘겼는데도 릴레이가 읽기를 멈추지 않았다"

        # 2단계 — 멈춘 채로 유휴 타임아웃(15초)보다 오래 버틴다. B 는 여전히 안 읽고,
        # 자기 데드라인만 keepalive 로 살려 둔다. 수정 전에는 여기서 A 가 끊겼다.
        hold_until = time.monotonic() + 18.0
        while time.monotonic() < hold_until:
            try:
                b.sendall(keepalive)
            except (BrokenPipeError, ConnectionResetError, OSError) as exc:
                _fail(exc, "2단계: 멈춘 채로 대기")
            time.sleep(0.5)

        # 3단계 — B 가 읽기 시작하면 릴레이의 보류 송신이 빠지고 A 의 읽기가 재개돼야
        # 한다. 재개 경로에는 유휴 데드라인 재무장이 붙어 있다(멈춘 동안 굳어 있었다).
        # 빼내는 동안에도 양쪽 데드라인은 살려 둬야 한다 — 여기서 아무도 안 보내면
        # 재개와 무관하게 그냥 유휴로 끊긴다(재려던 조건이 아니다).
        drained = 0
        b.settimeout(0.5)
        t1 = time.monotonic()
        last_keepalive = t1
        while time.monotonic() - t1 < 8.0:
            try:
                got = b.recv(65536)
                if not got:
                    break
                drained += len(got)
            except socket.timeout:
                pass
            # 데이터가 몰려 오면 이 루프는 아주 빨리 돈다. 송신을 매 회전마다 하면
            # 다시 바이트 레이트 상한을 넘어 "레이트 초과" 로 끊긴다 — 시간 기준으로
            # 묶어 유휴 데드라인만 유지한다.
            now = time.monotonic()
            if now - last_keepalive >= 0.4:
                last_keepalive = now
                try:
                    b.sendall(keepalive)
                    a.sendall(chunk)
                except (BrokenPipeError, ConnectionResetError, OSError) as exc:
                    _fail(exc, "3단계: 빼내는 중")
            if drained > 512 * 1024:
                break
        assert drained > 0, "B 가 읽기 시작했는데 보류 송신이 하나도 안 빠졌다"

        # A 가 살아 있고, 재개된 뒤 다시 보낼 수 있어야 한다.
        a.settimeout(5.0)
        try:
            a.sendall(chunk)
        except (BrokenPipeError, ConnectionResetError, OSError) as exc:
            _fail(exc, "3단계: 재개 후")
    finally:
        a.close(); b.close()


class _StatsReader:
    """릴레이가 주기적으로 내는 [stats] 줄을 실시간으로 읽어 둔다.

    파이프를 읽는 쪽이 없으면 버퍼가 차서 릴레이가 write 에서 멈추므로, 테스트가
    상태를 안 볼 때에도 계속 비워 줘야 한다 — 그래서 전용 스레드다.
    """

    _FIELDS = ("conns", "matches", "tx", "tx_peak")

    def __init__(self, proc):
        self._proc = proc
        self._lock = threading.Lock()
        self._samples = []          # (수신 시각, {필드: 값})
        self.lines = []
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        for raw in self._proc.stdout:
            line = raw.decode("utf-8", errors="replace").rstrip()
            with self._lock:
                self.lines.append(line)
            if "[stats]" not in line:
                continue
            fields = {}
            for tok in line.split():
                if "=" not in tok:
                    continue
                key, _, val = tok.partition("=")
                if key not in self._FIELDS:
                    continue
                # conns/tx 는 used/limit 형태다 — 앞쪽(사용량)만 쓴다.
                head = val.split("/")[0]
                try:
                    fields[key] = int(head)
                except ValueError:
                    pass
            if fields:
                with self._lock:
                    self._samples.append((time.monotonic(), fields))

    def wait_for_sample_after(self, when: float, timeout: float = 15.0):
        """``when`` 이후에 새로 도착한 상태 줄을 기다렸다 돌려준다.

        경계에 걸친 줄을 그대로 읽으면 아직 정리 전 상태를 보게 되므로, 반드시
        그 시각 이후에 **도착한** 표본만 인정한다.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self._lock:
                for ts, fields in reversed(self._samples):
                    if ts > when:
                        return fields
            time.sleep(0.05)
        with self._lock:
            tail = "\n".join(self.lines[-30:])
        raise AssertionError(
            "상태 줄이 오지 않았다 — 릴레이가 상태를 못 내고 있다:\n" + tail)

    def latest(self) -> dict:
        with self._lock:
            return dict(self._samples[-1][1]) if self._samples else {}

    def latest_after(self, when: float) -> dict:
        """``when`` 이후에 도착한 표본 중 가장 최근 것. 없으면 빈 dict."""
        with self._lock:
            if self._samples and self._samples[-1][0] > when:
                return dict(self._samples[-1][1])
        return {}

    def dump(self) -> str:
        with self._lock:
            return "\n".join(self.lines[-40:])


def test_tx_budget_is_returned_when_connections_die():
    """전역 tx 예산이 연결이 죽을 때 정확히 돌아와야 한다.

    연결당 상한만으로는 메모리를 보장하지 못한다 — 연결당 상한 × --max-conns 로
    천장이 곱해지기 때문이다. 전역 예산이 그 곱셈을 끊는다. 다만 예산은 수동
    회계라, 진짜 위험은 상한이 안 걸리는 것이 아니라 **반납이 새는 것**이다.
    새면 서버가 멀쩡한데도 시간이 갈수록 조용히 굶다가 아무도 못 붙는다.

    예전 버전은 바깥에서 "접속이 되는가" 만 봤고, 그래서 반납 회계를 통째로
    제거한 바이너리가 그대로 통과했다 — 예산이 바닥나도 accept 는 계속 되고
    막히는 것은 버퍼링뿐이라 증상이 밖으로 안 나왔기 때문이다. 이제 릴레이가
    주기적으로 tx 사용량을 찍으므로 카운터 자체를 본다.

    한 라운드는 이렇게 돈다.
      1) 안 읽는 상대를 만들어 릴레이의 보류 송신을 실제로 쌓는다.
      2) 전부 끊는다.
      3) 끊은 뒤에 도착한 상태 줄에서 tx 사용량이 정확히 0 이어야 한다.

    3) 이 이 테스트의 전부다. 반납이 한 경로라도 빠지면 tx 는 0 으로 돌아오지
    않고, 라운드를 거듭할수록 커진다. 1) 은 그 판정이 공허하지 않다는 증거다 —
    tx_peak 이 0 이면 애초에 아무것도 안 쌓인 것이므로 테스트가 실패한다.
    """
    reactor_bin = _find_bin("tetris_relay_reactor", "TETRIS_RELAY_REACTOR_BIN")
    if not reactor_bin:
        pytest.skip("tetris_relay_reactor binary missing")

    port = _free_port()
    # 상태 주기를 1초로 줄인다 (운영 기본값 10초는 테스트 한 라운드보다 길다).
    proc = subprocess.Popen([str(reactor_bin), "--port", str(port),
                             "--max-tx-mib", "1",
                             "--stats-interval-sec", "1"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stats = _StatsReader(proc)
    try:
        assert _wait_listen(port, 5.0), "relay 기동 실패"
        # 커널 송신 버퍼를 넘겨 릴레이의 보류 송신까지 밀어내야 하므로 큰 프레임을
        # 쓴다. 작은 프레임으로는 커널이 다 삼켜 tx 가 0 인 채로 끝난다.
        chunk = build_frame(MsgType.CHAT, b"x" * 4000)

        # 판정 기준을 최고 수위(tx_peak)가 아니라 그 순간의 사용량(tx)으로 잡는다.
        # 최고 수위는 프로세스 수명 동안 단조 증가하는 값이라, 라운드마다 새 최고를
        # 요구하면 "앞 라운드보다 덜 쌓인" 정상적인 라운드가 실패로 뒤집힌다
        # (4쌍으로 세운 기록을 뒤의 1쌍이 넘을 수는 없다). 사용량은 라운드마다
        # 0 에서 다시 시작하므로 그런 이력에 얽히지 않는다.
        def flood_until_pending(senders, since: float, budget_s: float = 20.0) -> bool:
            """보류 송신이 실제로 쌓인 것을 상태 줄로 확인할 때까지 붓는다.

            속도가 관건이다 — 릴레이의 초당 상한(64 KiB/s)을 넘기면 보류 송신이
            쌓이기 전에 릴레이가 붓는 쪽을 먼저 끊어 버려, 정작 재려던 것을 하나도
            못 잰다. 그래서 그 아래(약 40 KiB/s)로 페이스를 맞춘다.
            """
            deadline = time.monotonic() + budget_s
            while time.monotonic() < deadline:
                if stats.latest_after(since).get("tx", 0) > 0:
                    return True
                for sock in senders:
                    for _ in range(2):
                        try:
                            sock.sendall(chunk)
                        except OSError:
                            pass
                time.sleep(0.2)
            return stats.latest_after(since).get("tx", 0) > 0

        for round_no in range(3):
            round_start = time.monotonic()
            socks = []
            try:
                # unranked 로 4쌍을 붙이고, 각 쌍의 한쪽은 아무것도 읽지 않는다.
                for _ in range(4):
                    a = socket.create_connection(("127.0.0.1", port), timeout=3.0)
                    b = socket.create_connection(("127.0.0.1", port), timeout=3.0)
                    b.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                    socks += [a, b]
                    a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
                    b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
                    _recv_match_found(a)
                    _recv_match_found(b)
                    a.sendall(build_frame(MsgType.READY, b"\x01"))
                    b.sendall(build_frame(MsgType.READY, b"\x01"))
                time.sleep(0.3)

                senders = socks[0::2]
                for a in senders:
                    a.settimeout(0.2)
                # 쌓인 적이 없으면 아래 판정은 아무것도 증명하지 않는다.
                assert flood_until_pending(senders, round_start), (
                    f"라운드 {round_no}: 보류 송신이 안 쌓였다 — 이 테스트는 지금 "
                    "아무것도 재고 있지 않다:\n" + stats.dump())
            finally:
                for s in socks:
                    s.close()

            closed_at = time.monotonic()
            assert proc.poll() is None, f"라운드 {round_no} 후 릴레이가 죽었다"

            sample = stats.wait_for_sample_after(closed_at)
            assert sample.get("tx") == 0, (
                f"라운드 {round_no}: 연결이 전부 끊겼는데 tx 예산이 돌아오지 않았다 "
                f"(tx={sample.get('tx')}). 반납 회계가 새고 있다:\n" + stats.dump())

            probe = socket.create_connection(("127.0.0.1", port), timeout=3.0)
            probe.close()

        # 위 라운드들은 "쌓인 채로 죽는" 경로만 잰다. 반납 경로는 둘인데, 다른
        # 하나는 살아 있는 연결이 적체를 빼내는 쪽이다 — 그쪽 반납만 빠져도
        # 위 판정은 전부 통과한다(죽을 때 남은 것이 0 이므로). 그래서 밀렸다가
        # 다시 빠져나가는 경로를 따로 재고, 연결을 끊지 않은 채로 확인한다.
        phase2_start = time.monotonic()
        a = socket.create_connection(("127.0.0.1", port), timeout=3.0)
        b = socket.create_connection(("127.0.0.1", port), timeout=3.0)
        try:
            b.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
            b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
            _recv_match_found(a)
            _recv_match_found(b)
            a.sendall(build_frame(MsgType.READY, b"\x01"))
            b.sendall(build_frame(MsgType.READY, b"\x01"))
            time.sleep(0.3)

            a.settimeout(0.2)
            assert flood_until_pending([a], phase2_start), (
                "2단계에서 보류 송신이 안 쌓였다 — 배수 경로를 재지 못한다:\n"
                + stats.dump())

            # 이제 b 가 읽어 릴레이의 보류 송신을 비운다. "조용해지면 끝" 으로는
            # 판정할 수 없다 — 우리가 멈춰 세웠던 a 의 커널 백로그가 재개와 함께
            # 뒤늦게 흘러 들어오므로, 잠깐의 정적은 배수 완료가 아니다. 릴레이가
            # 스스로 tx=0 을 말할 때까지 읽는다.
            drain_start = time.monotonic()
            b.settimeout(0.2)
            reached_zero = False
            drain_until = drain_start + 25.0
            while time.monotonic() < drain_until:
                try:
                    b.recv(65536)
                except socket.timeout:
                    pass
                except OSError:
                    break
                if stats.latest_after(drain_start).get("tx") == 0:
                    reached_zero = True
                    break
            assert reached_zero, (
                "상대가 계속 읽는데도 tx 예산이 0 으로 돌아오지 않았다 — 배수 "
                "경로의 반납 회계가 새고 있다:\n" + stats.dump())
        finally:
            a.close()
            b.close()

        # 연결 수 카운터도 같은 성질을 갖는다 — 전부 끊긴 뒤에는 0 이어야 한다.
        final = stats.wait_for_sample_after(time.monotonic())
        assert final.get("conns") == 0, (
            "모든 연결이 끊겼는데 동시 연결 수가 안 돌아왔다 "
            f"(conns={final.get('conns')}):\n" + stats.dump())
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()


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


def test_reactor_rate_cap_survives_a_backpressure_pause():
    """백프레셔로 멈췄다 풀린 연결이 레이트 상한을 무한히 우회하면 안 된다.

    멈춰 있는 동안 상대의 버퍼에 쌓인 적체는 우리가 안 읽어서 생긴 것이지 상대가
    규정을 넘긴 게 아니다 — 그래서 재개 직후의 버스트를 그대로 "레이트 초과" 로
    청구하면 우리가 막아 놓고 대가를 상대에게 물리는 셈이 된다. 처음 그 문제를
    "재개 후 일정 시간 면제" 로 풀었는데, **면제를 재개 시각에 묶은 것이 틀렸다.**
    재개는 tx 가 high-water 아래로 떨어질 때마다 일어나므로, pause 와 resume 을
    반복시키면 이전 면제가 만료되기 전에 새 면제가 걸려 상한이 영영 적용되지 않는다.

    수정 전 실측(Windows): pause 를 한 번 성립시킨 뒤 상대가 빠르게 빼내게 하자
    3.7초 동안 42.2 MB — 초당 11 MiB 로 상한(64 KiB/s)의 약 172배가 차단 없이
    통과했다. 지금은 토큰 버킷이라 멈춘 동안 쌓인 토큰만큼만 봐주고 그 뒤로는
    평소 속도로만 찬다.

    회귀 테스트다. 위 크기 차이가 워낙 커서 문턱을 넉넉히 잡아도 수정 전은 반드시
    걸린다 — 통과 조건을 8 MiB 로 두면 42 MB 는 다섯 배 넘게 초과한다.

    reactor 전용이다. 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없어 이 경로 자체가
    존재하지 않는다.
    """
    reactor_bin = _find_bin("tetris_relay_reactor", "TETRIS_RELAY_REACTOR_BIN")
    if not reactor_bin:
        pytest.skip("tetris_relay_reactor binary missing")

    ALLOWED = 8 * 1024 * 1024          # 버스트 허용치(≈1 MiB)보다 넉넉히 위
    proc, port = _spawn_listening(
        lambda p: [str(reactor_bin), "--port", str(p), "--log-level", "error"])
    a = socket.create_connection(("127.0.0.1", port), timeout=3.0)
    b = socket.create_connection(("127.0.0.1", port), timeout=3.0)
    b.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    try:
        a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        _recv_match_found(a)
        _recv_match_found(b)
        a.sendall(build_frame(MsgType.READY, b"\x01"))
        b.sendall(build_frame(MsgType.READY, b"\x01"))
        time.sleep(0.3)

        # 1단계 — 상한 아래로 밀어(≈41 KiB/s) B 의 tx 를 high-water 위로 올린다.
        # A 의 sendall 이 막히는 것이 "릴레이가 A 를 멈춰 세웠다" 는 신호다.
        small = build_frame(MsgType.CHAT, b"x" * 400)
        a.settimeout(2.0)
        paused = False
        t0 = time.monotonic()
        while time.monotonic() - t0 < 25.0:
            try:
                for _ in range(5):
                    a.sendall(small)
            except socket.timeout:
                paused = True
                break
            except OSError as exc:
                pytest.fail(f"1단계에서 끊겼다: {exc!r}")
            time.sleep(0.05)
        assert paused, "high-water 를 넘겼는데 릴레이가 읽기를 멈추지 않았다"

        # 2단계 — B 가 빠르게 빼내면 resume 이 반복된다. 예전에는 그때마다 면제가
        # 갱신돼 상한이 사라졌다. 지금은 쌓인 토큰을 다 쓰면 차단돼야 한다.
        drained = [0]
        alive = [True]

        def drain():
            b.settimeout(0.5)
            while alive[0]:
                try:
                    got = b.recv(1 << 18)
                    if not got:
                        break
                    drained[0] += len(got)
                except socket.timeout:
                    pass
                except OSError:
                    break

        th = threading.Thread(target=drain, daemon=True)
        th.start()
        try:
            chunk = build_frame(MsgType.CHAT, b"x" * 3900)
            accepted = 0
            cut = False
            t1 = time.monotonic()
            while time.monotonic() - t1 < 10.0:
                try:
                    a.sendall(chunk)
                    accepted += len(chunk)
                except socket.timeout:
                    pass
                except OSError:
                    cut = True
                    break
                if accepted > ALLOWED:
                    break
        finally:
            alive[0] = False
            th.join(timeout=2)

        assert cut, (
            f"레이트 상한이 걸리지 않았다 — {accepted} 바이트를 차단 없이 통과시켰다. "
            "면제가 재개마다 갱신되고 있다")
        assert accepted <= ALLOWED, (
            f"차단되기까지 {accepted} 바이트가 통과했다 (허용 {ALLOWED}). "
            "멈춘 동안 쌓인 토큰보다 훨씬 많다면 면제가 갱신되고 있다는 뜻이다")
    finally:
        a.close()
        b.close()
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
