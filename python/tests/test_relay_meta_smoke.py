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
import re
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
        # 0.0.0.0 으로 잡는다 — 릴레이가 INADDR_ANY 로 bind 하기 때문이다
        # (net/socket.cpp 의 tcp_listen). 127.0.0.1 로만 예약하면 그 주소에서만
        # 비어 있는 번호를 받을 수 있고, 릴레이는 모든 주소에서 그 번호를 잡아야 하니
        # bind 가 실패한다. 코드가 틀려서가 아니라 예약한 범위가 달라서 나는 실패라
        # 무관한 테스트가 빨갛게 된다. (meta 는 --http HOST:PORT 로 127.0.0.1 에만
        # 붙으므로 이 사정이 없지만, 더 강한 예약이 해가 되지 않아 같은 함수를 쓴다.)
        s.bind(("0.0.0.0", 0))
        return s.getsockname()[1]


def _spawn_listening(argv_for_port, tries: int = 4, creationflags: int = 0,
                     stdout_reader=None):
    """포트를 잡아 프로세스를 띄우고, 실제로 듣기 시작하면 (proc, port) 를 준다.

    ``stdout_reader`` 를 주면 기본 비우기 스레드 대신 그것을 부른다 — 릴레이가 찍는
    줄을 직접 읽어야 하는 테스트도 포트 재시도를 함께 쓰기 위해서다. 자식이 실제로
    듣기 시작한 뒤에만 불리므로, 번호를 뺏겨 버려지는 시도의 출력은 지금처럼
    communicate 가 가져간다.

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
            if stdout_reader is not None:
                stdout_reader(proc)
            else:
                drain = threading.Thread(target=lambda: proc.stdout.read(),
                                         daemon=True)
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


def _require_reactor_step() -> Path:
    """reactor 고정 테스트의 공통 관문.

    이 테스트들은 TETRIS_RELAY_BIN 이 무엇을 가리키든 자기 reactor 바이너리를 직접
    띄운다 — 즉 "지금 어떤 릴레이를 겨누고 있는가" 와 무관하게 매번 돈다. 그래서
    스레드 모델 스텝에서도 전부 한 번씩 더 돌아 CI 시간이 그대로 두 배가 됐다.
    중복만 없애고 커버리지는 그대로 두는 것이 여기서 하려는 일이다.

    그래서 건너뛰는 조건을 "reactor 가 아니다" 가 아니라 **"이 실행이 명시적으로
    다른 바이너리를 겨눈다"** 로 잡는다. 둘은 같아 보이지만 로컬에서 갈린다:
    환경변수를 안 주고 README 가 안내하는 대로 pytest 를 돌리면 경로 탐색이
    build/tetris_relay 를 먼저 집으므로, 이름으로만 판정하면 이 테스트들이
    **로컬에서 영영 안 돌고** CI 에서만 도는 테스트가 된다. 실제로 그렇게 됐었다.
    CI 는 스텝마다 TETRIS_RELAY_BIN 을 명시하므로 중복 제거는 그대로 유지된다.
    """
    reactor_bin = _find_bin("tetris_relay_reactor", "TETRIS_RELAY_REACTOR_BIN")
    if not reactor_bin:
        pytest.skip("tetris_relay_reactor binary missing")
    targeted = os.environ.get("TETRIS_RELAY_BIN")
    if targeted and "reactor" not in Path(targeted).name:
        pytest.skip("이 실행은 스레드 모델을 겨눈다 — reactor 스텝에서 돈다")
    return reactor_bin


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
    reactor_bin = _require_reactor_step()

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
    reactor_bin = _require_reactor_step()

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


# 종료 로그 한 줄에서 연결 번호와 사유를 뽑는다. 릴레이가 찍는 형태는
#   [conn 7] close: <사유> player_id=.. match=.. match_uuid=..
# 이고(reactor_relay.cpp 의 close_conn + ident_of), 사유 뒤의 식별자 꼬리가 경계
# 역할을 한다 — 사유 자체에 공백이 있어서(예: "상호 백프레셔 교착 (양쪽
# read_paused)") 토큰 분해로는 못 자른다.
_CLOSE_LINE_RE = re.compile(r"\[conn (\d+)\] close: (.*?) player_id=")
# make_channel 이 찍는 페어링 줄. 앞 번호가 ch->a 다.
_PAIRED_LINE_RE = re.compile(r"paired conn (\d+) x (\d+)")


class _StatsReader:
    """릴레이가 내는 [stats] 줄과 종료 사유·페어링 줄을 실시간으로 읽어 둔다.

    파이프를 읽는 쪽이 없으면 버퍼가 차서 릴레이가 write 에서 멈추므로, 테스트가
    상태를 안 볼 때에도 계속 비워 줘야 한다 — 그래서 전용 스레드다.

    상태 줄 말고 종료 사유와 페어링 줄까지 받아 두는 이유는, 이 릴레이의 내부
    판단을 소켓 밖에서 추측할 수 없기 때문이다. sendall 이 막히는지, recv 가 언제
    끝나는지는 릴레이가 아니라 양쪽 커널 버퍼 크기가 정하는 값이라 플랫폼마다
    반대로 관측된다(6ffe2cd → dffdee9 왕복의 근본 원인). 반면 릴레이는 자기가 왜
    끊었는지와 누가 누구와 붙었는지를 직접 INFO 로 적으므로, 판정은 그 줄에서만
    읽는다. "끊겼는가" 와 "왜 끊겼는가" 는 다른 질문이고, 사유를 안 보면 엉뚱한
    이유로 끊긴 것(레이트 초과·하드 상한·유휴)을 재려던 조건이 성립한 것으로
    오인한다. --log-level error 로 띄우면 이 줄들이 사라져 아무것도 판정할 수 없다.

    구간을 자르는 기준은 전부 "받은 시각" 이다 — 기동 확인용 탐침 연결(_wait_listen)
    도 종료 줄을 남기므로, 재려는 구간이 시작된 뒤에 도착한 줄만 봐야 그 잡음이
    안 섞인다.
    """

    _FIELDS = ("conns", "matches", "tx", "tx_peak")

    def __init__(self, proc):
        self._proc = proc
        self._lock = threading.Lock()
        self._samples = []          # (수신 시각, {필드: 값})
        self._closes = []           # (수신 시각, 연결 번호, 사유)
        self._pairings = []         # (수신 시각, ch->a 의 번호, ch->b 의 번호)
        self.lines = []
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        for raw in self._proc.stdout:
            line = raw.decode("utf-8", errors="replace").rstrip()
            now = time.monotonic()
            with self._lock:
                self.lines.append(line)
            m = _CLOSE_LINE_RE.search(line)
            if m:
                with self._lock:
                    self._closes.append((now, int(m.group(1)), m.group(2)))
                continue
            m = _PAIRED_LINE_RE.search(line)
            if m:
                with self._lock:
                    self._pairings.append((now, int(m.group(1)), int(m.group(2))))
                continue
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

    def samples_after(self, when: float) -> list[dict]:
        """``when`` 이후에 도착한 표본 전부, 도착 순서대로.

        "값이 더 안 변한다" 를 판정하려면 최신값 하나로는 부족하다 — 같은 값이 새로
        온 것인지 예전 것을 다시 보는 것인지 구분할 수 없기 때문이다.
        """
        with self._lock:
            return [dict(f) for ts, f in self._samples if ts > when]

    def closes_after(self, when: float, conns=None) -> list:
        """``when`` 이후에 도착한 종료 줄을 (연결 번호, 사유) 로, 적힌 순서대로.

        ``conns`` 를 주면 그 연결들만 본다. 시각만으로 자르는 것으로는 부족하다 —
        기동 확인용 탐침 연결(_wait_listen)도 종료 줄을 남기는데, 그 줄이 파이프를
        빠져나와 이 스레드에 닿는 시각은 릴레이가 적은 시각이 아니라 버퍼가 넘어가는
        시각이라 얼마든지 뒤로 밀린다(실측: 탐침의 종료 줄이 페어링 줄보다 늦게
        도착). 재려는 연결의 번호는 페어링 줄에서 이미 확정돼 있으므로, 잡음을
        빼는 확실한 기준은 시각이 아니라 그 번호다.
        """
        with self._lock:
            rows = [(cid, why) for ts, cid, why in self._closes if ts > when]
        if conns is None:
            return rows
        return [(cid, why) for cid, why in rows if cid in conns]

    def wait_for_closes_after(self, when: float, count: int,
                              timeout: float, conns=None) -> list:
        """``when`` 이후의 종료 줄이 ``count`` 개가 될 때까지 기다린다.

        모자란 채로 시간이 다 되면 모인 만큼 그대로 돌려준다 — 판정은 호출자가
        사유까지 보고 해야 한다. 여기서 실패로 끝내면 "몇 개가 왔는가" 만 남고
        "무엇이 왔는가" 가 사라진다.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            got = self.closes_after(when, conns)
            if len(got) >= count:
                return got
            time.sleep(0.05)
        return self.closes_after(when, conns)

    def wait_for_close_of(self, when: float, conn_id: int,
                          timeout: float = 5.0):
        """``conn_id`` 가 끊길 때까지 기다렸다 (연결 번호, 사유) 를 준다. 없으면 None.

        다른 연결이 먼저 끊겨도 그건 그대로 두고 기다린다 — 호출자가
        ``closes_after`` 로 전체를 다시 보고 판정한다.
        """
        deadline = time.monotonic() + timeout
        while True:
            for cid, why in self.closes_after(when):
                if cid == conn_id:
                    return (cid, why)
            if time.monotonic() >= deadline:
                return None
            time.sleep(0.02)

    def wait_for_pairing_after(self, when: float, timeout: float = 10.0):
        """페어링 줄에서 (ch->a 의 연결 번호, ch->b 의 연결 번호) 를 읽는다.

        MATCH_FOUND 의 role 바이트가 1 이면 그 소켓이 ch->a, 2 면 ch->b 다
        (reactor_relay.cpp 의 start_match). 그래서 이 두 값과 role 을 맞추면 "내
        소켓이 릴레이의 몇 번 연결인가" 를 접속 순서 같은 추정 없이 확정할 수 있다.
        """
        deadline = time.monotonic() + timeout
        while True:
            with self._lock:
                for ts, id_a, id_b in self._pairings:
                    if ts > when:
                        return id_a, id_b
            if time.monotonic() >= deadline:
                raise AssertionError(
                    "페어링 줄이 안 나왔다 — 두 클라이언트가 매칭되지 않았다:\n"
                    + self.dump())
            time.sleep(0.02)

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
    reactor_bin = _require_reactor_step()

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


# ── reactor 백프레셔·레이트 상한 회귀 테스트의 공용 재료 ─────────────────────
# 아래 테스트 무리는 전부 릴레이가 스스로 적는 줄(close 사유, 페어링 줄, [stats])로만
# 판정한다 — 클라이언트의 sendall 이 막히는가로 릴레이의 내부 상태를 추측하지 않는다.
# 그 값은 릴레이가 아니라 양쪽 커널 버퍼 크기가 정하고, 그래서 같은 계약이 Linux 와
# Windows 에서 정반대로 관측된 전례가 있다(6ffe2cd → dffdee9 왕복의 근본 원인).
#
# 밖에서 못 재는 것이 하나 있다. on_writable 의 ``tx_drained_at`` 갱신은 "빼냈는데도
# tx 가 high-water 위에 남아 재개가 안 되는" 순간에만 밖으로 드러나는데, Linux
# loopback 에서는 그 순간이 만들어지지 않는다. 실측(2026-08-23, 이 호스트):
#   · tx 가 high-water 위로 갈 수 있는 폭은 포워딩 배치 하나뿐이다. tcp_recv_some 은
#     한 번에 4096 바이트를 읽고 rx 에는 잘린 프레임 하나(≤4103)만 남으므로 구조적
#     상한이 ≈8 KiB 고, 실제로 관측된 pause 시점 tx 는 66342~69278 (상한 위로 806~3742).
#   · 반면 한 번의 EPOLLOUT 이 옮기는 양은 커널 송신 버퍼(SO_SNDBUF 64 KiB → 실효
#     131072)의 1/3 이상이다. 수신 창을 최소(SO_RCVBUF 128 → 실효 2304)로 좁혀도
#     16704 바이트였고, 기본값에서는 59392 였다.
# 그래서 배수가 관측되는 순간 tx 는 반드시 high-water 아래로 떨어지고, 송신자가 재개돼
# paused_since 가 새로 찍힌다 — tx_drained_at 이 그보다 앞설 방법이 없다. 실제로 그
# 대입을 지운 바이너리는 이 파일의 reactor 테스트 전부를 통과하며, 16 KiB 씩 5초마다
# 빼내는 상대(16초 동안 49152 바이트를 실제로 빼냈다)에서 원본과 바이트 단위로 같은
# 종료(16000ms 에 "백프레셔 상한 초과")를 냈다. 이 필드가 값을 하는 곳은 커널 송신
# 버퍼가 훨씬 작은 실제 링크와 Windows 쪽이다.

# server/reactor_relay.cpp 의 상수. 기대는 값마다 출처를 남긴다.
_RELAY_MAX_BYTES_PER_SECOND = 64 * 1024                       # kMaxBytesPerSecond
_RELAY_RATE_BURST_BYTES = 16 * _RELAY_MAX_BYTES_PER_SECOND    # kRateBurstBytes ≈1 MiB
_RELAY_SEND_HIGH_WATER = 64 * 1024                            # kSendHighWater
# 백프레셔로 한 연결을 멈춰 세워 둘 수 있는 최대 시간, 초. 릴레이가 그러듯
# 버스트 한도에서 유도한다(kMaxPauseDuration = kRateBurstBytes / kMaxBytesPerSecond) —
# 상수를 따로 적어 두면 한쪽만 움직였을 때 테스트가 조용히 다른 것을 재게 된다.
_RELAY_MAX_PAUSE_SEC = _RELAY_RATE_BURST_BYTES // _RELAY_MAX_BYTES_PER_SECOND   # 16
# 릴레이가 적는 종료 사유. 문자열이 달라지면 이 테스트들이 겨누는 갈래도 달라진 것이라
# 그대로 일치해야 한다.
_RATE_REASON = "byte rate 초과"
_DEADLOCK_REASON = "상호 백프레셔 교착 (양쪽 read_paused)"

# 백프레셔 테스트가 릴레이에 물리는 유휴 만기(--idle-timeout-sec). 운영 기본값은
# 15초지만, 만기가 실제로 지나가는 것을 여러 번 봐야 하는 테스트에서 15초는 한
# 계약당 벽시계 30초가 넘는다. 3초로 줄이면 같은 갈래를 같은 순서로 지나면서
# 한 자릿수 초에 끝난다. 위로는 pause 상한(kMaxPauseDuration, 16초)이 천장이다 —
# 일방 백프레셔 테스트가 그 상한에 닿으면 재려던 것과 다른 갈래를 재게 된다.
_BP_IDLE_SEC = 3
# server/reactor_relay.cpp 의 kMaxPauseDuration 과 같아야 한다
# (kRateBurstBytes / kMaxBytesPerSecond 에서 유도되는 값).
#
# 교착 회수가 "만기 몇 번" 이 아니라 이 상한에 매여 있기 때문에 필요하다. 릴레이는
# 상대가 아직 배출 중일 수 있다고 보이면 만기에서 곧장 끊지 않고 pause 시작점
# +kMaxPauseDuration 으로 재무장한다. 커널이 송신 버퍼를 조금 비워 read_paused 가
# 한순간 풀리면 정확히 그 갈래를 타므로, 만기의 배수로 잡은 대기 예산(3*3+3=12초)은
# 16초 천장보다 짧아 릴레이가 규정대로 동작해도 테스트가 진다. 실제로 이 불일치가
# CI 를 리눅스·윈도우 양쪽에서 간헐적으로 빨갛게 만들고 있었다.
_BP_MAX_PAUSE_SEC = 16
# 위 pause 예산 테스트가 2단계에서 쌓아야 하는 최소 적체. 이보다 적게 쌓이면
# "예산 없이도 덮이는" 구간에 들어가 판정이 성립하지 않는다. 값은 그 테스트의
# tokens_left(≈394 KiB) + 멈추지 않은 구간의 충전을 넘겨야 한다는 데서 온다.
_BP_MIN_STAGED_BYTES = 900 * 1024
# 커널이 한 번에 다 삼키지 않을 크기의 프레임.
_BP_CHUNK = build_frame(MsgType.CHAT, b"x" * 4000)
# 로비에서 토큰만 태우는 길이 0 프레임(길이 2바이트 + 체크섬 4바이트).
_LOBBY_BURN_FRAME = b"\x00\x00" + b"\x00\x00\x00\x00"
# 룸 단계에서 토큰만 태우는 프레임. on_room 은 READY/CHAT/ROOM_LEAVE 만 처리하고
# 나머지는 무시하므로, 이 프레임은 상대의 tx 를 건드리지 않고 버킷만 줄인다.
_ROOM_BURN_FRAME = build_frame(MsgType.INPUT, b"\x00" * 4000)
# 한 방향으로 밀어 넣을 총량의 상한. 레이트 버스트(≈1 MiB) 아래로 잡아야 백프레셔를
# 재는 테스트가 레이트 상한에 먼저 걸리지 않는다.
_BP_FLOOD_CAP = 768 * 1024


class _Feeder:
    """소켓 하나에 프레임 경계를 깨지 않고 밀어 넣는다.

    sendall 을 쓰지 않는다. 타임아웃이 나면 몇 바이트가 나갔는지 알려 주지 않아
    프레임이 중간에서 잘리는데, 백프레셔로 멈춰 세워진 소켓에는 그 일이 늘 일어난다.
    잘린 스트림을 릴레이가 "과대 프레임" 으로 판정하면(forward_screened) 남은 rx 를
    통째로 버려서, 재려던 것과 무관한 이유로 흐름이 달라진다. 보낼 것을 버퍼에 모아
    두고 send 가 받아 준 만큼만 지우면 그 경계가 안 깨진다.

    소켓의 블로킹 모드는 건드리지 않는다 — 호출자가 정한다. 타임아웃을 건 소켓이면
    pump 가 커널이 받아 줄 때까지 그만큼 잠들어 그 자체가 페이스 조절이 되고,
    논블로킹이면 곧바로 돌아온다. 두 테스트 무리가 각각 그 둘을 필요로 한다.

    여기서 나오는 수(pushed/backlog)는 페이스 조절과 진단용이지 판정 근거가 아니다 —
    커널이 얼마를 받아 줬는지는 릴레이의 상태가 아니라 버퍼 크기의 함수다.
    """

    def __init__(self, sock: socket.socket):
        self._sock = sock
        self._buf = bytearray()
        self.pushed = 0
        self.alive = True

    def offer(self, data: bytes) -> None:
        self._buf += data

    def backlog(self) -> int:
        return len(self._buf)

    def offered_total(self) -> int:
        return self.pushed + len(self._buf)

    def pump(self, limit: int | None = None) -> int:
        """커널이 받아 주는 만큼(최대 ``limit``) 밀어 넣고 보낸 양을 준다."""
        sent = 0
        view = memoryview(self._buf)
        try:
            while self._buf and (limit is None or sent < limit):
                end = len(self._buf)
                if limit is not None:
                    end = min(end, limit - sent)
                try:
                    n = self._sock.send(view[:end])
                except (BlockingIOError, socket.timeout):
                    break
                except OSError:
                    self.alive = False
                    break
                if n <= 0:
                    break
                view.release()
                del self._buf[:n]
                view = memoryview(self._buf)
                self.pushed += n
                sent += n
        finally:
            view.release()
        return sent


def _drain_nonblocking(sock: socket.socket) -> int:
    """읽을 수 있는 만큼 읽고 버린다. 정상적으로 읽는 쪽을 흉내낸다.

    받은 양을 돌려준다 — 릴레이가 실제로 흘려보낸 바이트는 "릴레이가 상대에게서
    그만큼을 읽었다(= 그만큼을 청구했다)" 의 하한이라, 청구액을 밖에서 잴 수 있는
    유일한 수다.
    """
    got = 0
    while True:
        try:
            chunk = sock.recv(65536)
        except (BlockingIOError, OSError):
            return got
        if not chunk:
            return got
        got += len(chunk)


def _pace(feeder: "_Feeder", chunk: bytes, rate: int, since: float,
          floor: int = 0) -> None:
    """``since`` 부터 초당 ``rate`` 바이트가 되도록 버퍼를 채우고 민다.

    페이스를 정하는 것이 "커널이 받아 준 양" 이 아니라 "우리가 내놓은 양" 이다.
    백프레셔로 멈춰 세워진 소켓에서는 그 둘이 갈라지는데, 청구 대상은 결국 전자가
    아니라 후자(언젠가 릴레이가 읽어 갈 바이트)이기 때문이다.
    """
    target = floor + rate * (time.monotonic() - since)
    while feeder.offered_total() < target:
        feeder.offer(chunk)
    feeder.pump()


def _keepalive_frame() -> bytes:
    """유휴 데드라인만 갱신하는 최소 프레임."""
    return build_frame(MsgType.INPUT, struct.pack("<IH", 0, 1) + b"\x00")


def _spawn_relay_with_stats(argv_for_port, tries: int = 4):
    """포트를 잡아 릴레이를 띄우고 (proc, port, _StatsReader) 를 준다.

    ``_spawn_listening`` 의 기본 동작은 자식 stdout 을 전용 스레드로 버리는 것인데,
    아래 테스트들의 판정 근거([stats] 줄·close 사유·페어링 줄)가 바로 그 stdout
    이다. 버리는 대신 읽는 쪽을 끼우는 자리가 ``stdout_reader`` 이므로 포트 경합
    재시도(110def7)를 다시 쓰지 않고 그대로 얹는다.
    """
    box = {}
    proc, port = _spawn_listening(
        argv_for_port, tries=tries,
        stdout_reader=lambda pr: box.__setitem__("s", _StatsReader(pr)))
    return proc, port, box["s"]


def _forwarding_pair(stats, port, lobby_burn: int = 0):
    """두 클라이언트를 붙여 포워딩 단계까지 올리고 연결 번호까지 확정해 준다.

    반환은 ``(a, b, conn_a, conn_b)``. 연결 번호는 릴레이의 페어링 줄
    ("paired conn X x Y" — X 가 ch->a) 과 MATCH_FOUND 의 role 바이트(1 이면 ch->a,
    2 면 ch->b, start_match 참고)를 맞춰 정한다. 접속 순서 같은 클라이언트 쪽 추정이
    아니라 릴레이의 진술이라, 나중에 close 줄을 볼 때 "그게 누구였나" 로 다툴 여지가
    없다.

    소켓 버퍼 크기는 건드리지 않는다. SO_RCVBUF 를 명시하면 커널의 수신 창 자동
    조정이 꺼져서, 같은 크기를 지정해도 실효 처리량이 1/3 로 떨어진다(실측:
    기본값 1004 KiB/s → SO_RCVBUF=64 KiB 지정 324 KiB/s). 더 좁히면 창이 상시 닫혀
    zero-window persist 로 들어가 29 KiB/s 까지 주저앉는데, 그러면 상한(64 KiB/s)과
    자릿수가 같아져 토큰이 마르지 않는다 — 재려던 것을 아예 못 재게 된다. 좁혀야
    하는 테스트는 받은 뒤에 자기가 좁힌다.

    ``lobby_burn`` 을 주면 READY 직전에 A 가 그만큼의 길이 0 프레임을 흘려 레이트
    토큰을 미리 태운다. 로비 단계는 길이 0 프레임을 그 자리에서 소비하므로(on_lobby)
    수신 버퍼 상한에 걸리지 않고 토큰만 정확히 그만큼 줄일 수 있다 — 포워딩에
    들어설 때의 버킷 잔량을 테스트가 아는 값으로 만드는 유일한 방법이다. 단계 전이는
    버킷을 되돌리지 않으므로(test_reactor_forwarding_start_does_not_refill_the_burst
    가 못 박은 계약) 태운 만큼이 그대로 남는다.
    """
    assert lobby_burn % len(_LOBBY_BURN_FRAME) == 0, (
        f"lobby_burn 은 {len(_LOBBY_BURN_FRAME)} 의 배수여야 태운 양이 정확해진다")
    when = time.monotonic()
    a = socket.create_connection(("127.0.0.1", port), timeout=3.0)
    b = socket.create_connection(("127.0.0.1", port), timeout=3.0)
    try:
        a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        role_a, _ = _recv_match_found(a)
        role_b, _ = _recv_match_found(b)
        if lobby_burn:
            block = _LOBBY_BURN_FRAME * 1000
            a.settimeout(5.0)
            left = lobby_burn
            while left:
                n = min(left, len(block))
                a.sendall(block[:n])
                left -= n
        a.sendall(build_frame(MsgType.READY, b"\x01"))
        b.sendall(build_frame(MsgType.READY, b"\x01"))
        id_a, id_b = stats.wait_for_pairing_after(when)
    except BaseException:
        a.close()
        b.close()
        raise
    conn_a = id_a if role_a == 1 else id_b
    conn_b = id_a if role_b == 1 else id_b
    assert conn_a != conn_b, (
        f"role 이 겹친다 (A={role_a}, B={role_b}) — 페어링 줄과 맞출 수 없다:\n"
        + stats.dump())
    time.sleep(0.3)     # READY 왕복이 포워딩으로 넘어갈 틈
    return a, b, conn_a, conn_b


def _shutdown(proc, socks=()) -> None:
    for s in socks:
        if s is not None:
            try:
                s.close()
            except OSError:
                pass
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()


# ── 백프레셔로 멈춰 세운 연결의 회계와 상한 ──────────────────────────────────
# 한쪽만 멈춘 백프레셔에는 릴레이가 지켜야 할 계약이 둘 있고, 둘은 서로 반대
# 방향으로 틀린다.
#   1) 멈춰 있던 구간의 적체를 그 연결에게 청구하면 안 된다 (pause_credit).
#   2) 그렇다고 무한정 멈춰 둘 수도 없다 (kMaxPauseDuration).
# 한 테스트로 둘을 잴 수 없다. 1) 은 상한이 오기 전에 풀어 줘야 관측되고, 2) 는
# 풀지 않아야 관측되기 때문이다.

def test_reactor_does_not_bill_the_sender_for_backpressure_it_imposed():
    """백프레셔로 멈춰 세운 송신자에게 자기가 만든 정체를 청구하면 안 된다.

    멈춰 있는 동안 상대의 커널 버퍼와 우리 수신 버퍼에 쌓인 적체는 우리가 안 읽어서
    생긴 것이지 상대가 규정을 넘긴 게 아니다. 재개하는 순간 그것이 한꺼번에 읽히는데,
    그대로 "byte rate 초과" 로 청구하면 릴레이가 **자기가 만든 정체를 피해자에게**
    무는 셈이 된다. 2026-08-23 Linux 실측(수정 전 바이너리): 상한의 98% 로 보내던
    A 가 30초 멈춰 있다가 재개 50 ms 만에 절단됐다. 지금은 멈춰 있던 구간의 몫이
    재개할 때 pause 예산으로 따로 지급된다.

    이 계약을 재려면 **버킷이 비어 있어야** 한다. 버스트 허용치(kRateBurstBytes)가
    곧 16초분이고 pause 자체가 16초(kMaxPauseDuration)로 묶여 있으므로, 가득 찬
    버킷으로 시작하면 예산이 없어도 버스트만으로 다 덮여 아무것도 안 드러난다.
    실제로 그랬다 — 이 테스트의 옛 형태는 pause_credit 지급을 통째로 제거한
    바이너리를 그대로 통과시켰다. 그래서 로비에서 토큰을 먼저 태워 잔량을 아는
    값으로 만든 뒤에 시작한다.

    판정은 릴레이가 적은 줄과, 릴레이를 실제로 통과한 바이트로만 한다.
      · A 는 끊기지 않는다 — 특히 레이트 사유로는.
      · 그런데 통과한 양은 "예산이 없었다면 허용됐을 최대치"(태우고 남은 토큰 +
        멈추지 않은 시간 × 상한)를 넘는다. 그 둘이 동시에 참이면 그 초과분을
        지불한 것은 pause 예산뿐이다. 뒤엣것이 없으면 이 테스트는 아무것도 재지
        않는다 — 그게 옛 형태가 빠져 있던 함정이라 단언으로 못 박는다.

    reactor 전용이다. 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다.
    """
    reactor_bin = _require_reactor_step()

    # 로비에서 미리 태울 양. 남기는 잔량(≈394 KiB)은 1단계가 파이프를 채우며 쓰는
    # 몫(≈210 KiB)보다 넉넉히 크되, 2단계가 지급 없이는 못 덮을 만큼 작아야 한다.
    BURN = 630 * 1024
    # 1단계 페이스. 상한보다 빠르게 밀어 파이프를 빨리 채운다 — 태우고 남은 토큰이
    # 이 구간의 초과분(≈36 KiB/s)을 덮는다.
    RAMP_RATE = 100 * 1024
    # 2단계: 멈춘 채로 이만큼, 이 페이스로 민다. 페이스는 상한(64 KiB/s) 아래여야
    # 한다 — 넘겨 밀면 지급이 있어도 정당하게 끊기고, 그건 이 계약이 아니다.
    HOLD_SEC = 10.0
    HOLD_RATE = 60 * 1024
    # A 의 커널 송신 버퍼. 이 값을 안 건드리면 플랫폼 기본값(대개 이 시험의 목표보다
    # 작다)에 막혀 2단계가 목표한 만큼(HOLD_RATE*HOLD_SEC ≈586 KiB)을 커널에 못
    # 넘긴다 — 실측: Windows 기본값에서 목표의 절반가량만 send() 가 받아 주고
    # 나머지는 파이썬 쪽 버퍼에 갇혔다(3단계는 sender.pump 를 다시 안 부르므로
    # 그 갇힌 몫은 영영 안 나간다). 그러면 got_b 가 재려던 "pause 예산이 덮은 양"이
    # 아니라 "A 의 송신 버퍼가 우연히 얼마나 컸나" 를 재게 되어 플랫폼마다 결과가
    # 갈린다. 수신 창과는 무관한 지렛대다 — 로컬 송신 버퍼는 상대가 안 읽어 ACK가
    # 안 와도 그 자체 용량만큼은 쌓인다(재전송을 위해 커널이 들고 있어야 하는 몫이라
    # 원격 수신 여부와 별개). 1·2단계 목표 합(≈900 KiB)보다 넉넉히 큰 값으로 고정해
    # 그 편차를 없앤다. 수신 버퍼(SO_RCVBUF)는 여전히 건드리지 않는다 — 그건 다른
    # 계약(위 주석 참고)이다.
    SEND_BUF_BYTES = 4 * 1024 * 1024

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p),
                   "--log-level", "info", "--stats-interval-sec", "1"])
    a = b = None
    try:
        when = time.monotonic()
        # 버킷 잔량이 확정되는 시각은 이 뒤(로비에서 태우는 순간)지만, 기준을 앞으로
        # 잡아 둔다. 아래 "예산 없이 허용됐을 최대치" 는 이 시각부터의 충전을 전부
        # 세므로, 앞으로 잡을수록 그 최대치가 커지고 판정은 그만큼 보수적이 된다.
        t0 = time.monotonic()
        a, b, conn_a, conn_b = _forwarding_pair(stats, port, lobby_burn=BURN)
        a.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, SEND_BUF_BYTES)
        # 요청한 값이 그대로 잡혔는지 반드시 되읽는다. 리눅스는 SO_SNDBUF 를
        # net.core.wmem_max(기본 212992)로 잘라 버리고 그 두 배를 저장하므로,
        # 4 MiB 를 요청해도 실제로는 400 KiB 대만 잡힐 수 있다. 그 상태로 그냥
        # 진행하면 아래 마지막 단언이 "예산이 지급되지 않았다" 처럼 실패하는데,
        # 진짜 원인은 릴레이가 아니라 이 호스트가 적체를 충분히 못 쌓은 것이다.
        # 원인이 다른 두 실패를 같은 모양으로 만들면 CI 에서 구분할 수 없으므로,
        # 전제 조건은 전제 조건으로서 실패시킨다.
        eff_sndbuf = a.getsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF)
        if eff_sndbuf < _BP_MIN_STAGED_BYTES:
            pytest.fail(
                f"A 의 송신 버퍼가 {eff_sndbuf} 바이트뿐이라(요청 {SEND_BUF_BYTES}) "
                f"2단계에서 {_BP_MIN_STAGED_BYTES} 바이트 이상을 쌓을 수 없다. "
                "이 호스트에서는 이 테스트가 재려는 것을 잴 수 없다 — 리눅스라면 "
                "net.core.wmem_max 가 상한이다 "
                "(sysctl -w net.core.wmem_max=4194304). 릴레이의 결함이 아니다.")
        tokens_left = _RELAY_RATE_BURST_BYTES - BURN

        # 수신 버퍼는 어느 쪽도 건드리지 않는다. B 쪽을 좁히면 3단계에서 B 의
        # 수신 창이 상시 닫혀 배수가 ≈31 KiB/s 로 주저앉는데(실측), 그러면 적체가
        # "한꺼번에" 가 아니라 상한과 비슷한 속도로 야금야금 읽혀서 예산이 없어도
        # 충전만으로 덮인다 — 재려던 것을 아예 못 재게 된다.
        a.setblocking(False)
        b.setblocking(False)
        sender, quiet = _Feeder(a), _Feeder(b)
        keepalive = _keepalive_frame()
        last_keepalive = [0.0]
        got_b = [0]

        def _tick_quiet_side() -> None:
            # B 는 한 바이트도 빼내지 않지만 자기 유휴 데드라인은 살려 둔다. 안
            # 그러면 B 가 먼저 유휴로 걷히고 A 가 "상대 이탈" 로 따라 죽어, 재려던
            # 조건에 닿지도 못한 채 "끊겼다" 만 남는다.
            now = time.monotonic()
            if now - last_keepalive[0] >= 3.0:
                last_keepalive[0] = now
                quiet.offer(keepalive)
            quiet.pump()

        # ── 1단계: 파이프를 채워 릴레이가 A 의 읽기를 멈추게 한다 ─────────────
        deadline = time.monotonic() + 25.0
        established = False
        while time.monotonic() < deadline:
            _pace(sender, _BP_CHUNK, RAMP_RATE, t0)
            _tick_quiet_side()
            _drain_nonblocking(a)
            if stats.latest().get("tx_peak", 0) > _RELAY_SEND_HIGH_WATER:
                established = True
                break
            time.sleep(0.01)
        ramp_s = time.monotonic() - t0

        early = stats.closes_after(when, (conn_a, conn_b))
        assert not early, (
            f"1단계에서 벌써 끊겼다: {early} (A=conn {conn_a}, "
            f"B=conn {conn_b}). 태우고 남은 토큰({tokens_left})으로 파이프를 채우지 "
            "못했다는 뜻이라, 태우는 양을 줄여야 한다:\n" + stats.dump())
        assert established, (
            f"{ramp_s:.1f}초를 밀었는데 보류 송신이 high-water"
            f"({_RELAY_SEND_HIGH_WATER}) 를 넘지 않았다 "
            f"(tx_peak={stats.latest().get('tx_peak', 0)}) — 릴레이가 A 의 읽기를 "
            "멈춘 적이 없으므로 이 테스트는 아무것도 재고 있지 않다:\n" + stats.dump())

        # ── 2단계: 멈춘 채로 상한 아래 페이스로 계속 민다 ────────────────────
        # 이 바이트들은 릴레이가 안 읽는 동안 커널에 쌓인다 — 재개하는 순간
        # 한꺼번에 읽히는 그 적체가 이 테스트의 대상이다.
        hold_started = time.monotonic()
        hold_floor = sender.offered_total()
        while time.monotonic() - hold_started < HOLD_SEC:
            _pace(sender, _BP_CHUNK, HOLD_RATE, hold_started, floor=hold_floor)
            _tick_quiet_side()
            _drain_nonblocking(a)
            time.sleep(0.02)

        # ── 3단계: B 가 빼내기 시작한다. A 가 재개되고 적체가 한꺼번에 읽힌다 ──
        # A 는 여기서부터 아무것도 더 보내지 않는다 — 청구되는 것이 오롯이 "멈춰
        # 있는 동안 쌓인 것" 이어야 한다.
        drain_started = time.monotonic()
        last_progress = drain_started
        while time.monotonic() - drain_started < 15.0:
            n = _drain_nonblocking(b)
            if n:
                got_b[0] += n
                last_progress = time.monotonic()
            _tick_quiet_side()
            _drain_nonblocking(a)
            if time.monotonic() - last_progress > 1.5:
                break
            time.sleep(0.005)
        drain_s = last_progress - drain_started

        closes = stats.closes_after(when, (conn_a, conn_b))
        assert not closes, (
            f"백프레셔로 멈춰 세운 뒤 재개한 연결이 끊겼다: {closes} "
            f"(A=conn {conn_a}, B=conn {conn_b}). 사유가 레이트 초과면 릴레이가 자기가 "
            "만든 정체를 피해자에게 청구한 것이다:\n" + stats.dump())

        # 지급이 없었다면 허용됐을 최대치. 멈춰 있는 동안은 버킷 시계도 서므로
        # (refill_tokens 의 read_paused 갈래) 충전은 멈추지 않은 구간에서만 는다.
        # 재개 이후의 충전까지 넉넉히 얹어 보수적으로 잡는다.
        without_credit = tokens_left + int(
            _RELAY_MAX_BYTES_PER_SECOND * (ramp_s + drain_s + 1.0))
        assert got_b[0] > without_credit, (
            f"릴레이를 통과한 양이 {got_b[0]} 바이트뿐이라 지급 없이도 덮인다 "
            f"(태우고 남은 토큰 {tokens_left} + 멈추지 않은 "
            f"{ramp_s + drain_s + 1.0:.1f}초치 충전 = {without_credit}). "
            "A 가 살아남은 것이 pause 예산 덕인지 아닌지 이 실행은 구분하지 못한다 — "
            "재는 쪽(태우는 양·멈춰 두는 시간)을 고쳐야 한다:\n" + stats.dump())
    finally:
        _shutdown(proc, (a, b))


def test_reactor_closes_the_peer_that_never_drains():
    """멈춰 세워 둔 채로 상한을 넘기면, 닫히는 것은 정체를 만든 쪽이어야 한다.

    위 계약(멈춘 연결을 유휴로 오인하지 않는다)만 지키면 반대쪽 구멍이 열린다:
    아무도 회수하지 않는 매치가 fd 2개와 per-IP 세션 슬롯 2개를 프로세스 재시작까지
    붙든다(실측: 45초 유지, close 0건). 그래서 pause 는 kMaxPauseDuration(16초) 안에
    묶이고, 넘기면 **자기 tx 에서 그동안 한 바이트도 빼내지 않은 쪽** 이 닫힌다.

    지목이 이 계약의 전부다. 멈춰 세워진 쪽을 대신 닫으면 "매치가 회수됐다" 는
    그대로 참이지만, 릴레이는 자기가 입을 막아 둔 정상 플레이어를 걷어낸 것이 된다.
    그래서 close 줄을 연결 번호까지 맞춰 본다 — 안 빼내는 쪽(B)이 상한 사유로,
    멈춰 세워졌던 쪽(A)이 그 여파("상대 이탈")로.

    B 는 여기서도 자기 유휴 데드라인을 살려 둔다. 안 그러면 만기(15초)가 상한(16초)
    보다 먼저 와서 B 가 그냥 유휴로 걷히고, 이 테스트는 상한을 하나도 재지 못한 채
    통과한다 — 1초 차이로 갈리는 함정이라 반드시 살려 둬야 한다.

    reactor 전용이다. 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다.
    """
    reactor_bin = _require_reactor_step()

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p),
                   "--log-level", "info", "--stats-interval-sec", "1"])
    a = b = None
    try:
        when = time.monotonic()
        a, b, conn_a, conn_b = _forwarding_pair(stats, port)
        b.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        a.setblocking(False)
        b.setblocking(False)
        sender, quiet = _Feeder(a), _Feeder(b)
        keepalive = _keepalive_frame()
        last_keepalive = [0.0]

        def _tick_quiet_side() -> None:
            now = time.monotonic()
            if now - last_keepalive[0] >= 3.0:
                last_keepalive[0] = now
                quiet.offer(keepalive)
            quiet.pump()

        # 1단계 — A 를 밀어 릴레이가 A 의 읽기를 멈추게 한다. 전속력으로 밀되 총량은
        # 버스트 허용치 아래로 묶는다 — 여기서 레이트로 끊기면 상한을 재지 못한다.
        started = time.monotonic()
        established = False
        while time.monotonic() - started < 25.0:
            while (sender.backlog() < 4 * len(_BP_CHUNK)
                   and sender.offered_total() < _BP_FLOOD_CAP):
                sender.offer(_BP_CHUNK)
            sender.pump()
            _tick_quiet_side()
            _drain_nonblocking(a)
            if stats.latest().get("tx_peak", 0) > _RELAY_SEND_HIGH_WATER:
                established = True
                break
            time.sleep(0.01)
        assert established, (
            f"보류 송신이 high-water({_RELAY_SEND_HIGH_WATER}) 를 넘지 않았다 "
            f"(tx_peak={stats.latest().get('tx_peak', 0)}) — 릴레이가 A(conn {conn_a})"
            "의 읽기를 멈춘 적이 없으므로 상한을 잴 수 없다:\n" + stats.dump())
        paused_at = time.monotonic()

        # 2단계 — A 는 더 보내지 않는다(보낼 수도 없다). B 는 계속 안 읽되 자기
        # 데드라인만 살려 둔다. 상한이 지나면 릴레이가 스스로 움직여야 한다.
        while time.monotonic() - paused_at < 30.0:
            _tick_quiet_side()
            _drain_nonblocking(a)
            if len(stats.closes_after(when, (conn_a, conn_b))) >= 2:
                break
            time.sleep(0.02)
        closes = stats.wait_for_closes_after(when, 2, 5.0, (conn_a, conn_b))
        held_s = time.monotonic() - paused_at

        by_conn = dict(closes)
        assert len(closes) == 2, (
            f"{held_s:.1f}초를 멈춰 뒀는데 페어가 회수되지 않았다 "
            f"(상한 16초, 종료 줄 {closes}) — 아무도 걷지 않는 매치가 fd 와 세션 "
            "슬롯을 붙들고 남는다:\n" + stats.dump())
        assert conn_b in by_conn and by_conn[conn_b].startswith("백프레셔 상한 초과"), (
            f"상한에 걸려 닫힌 것이 정체를 만든 쪽(B=conn {conn_b})이 아니다: "
            f"{closes}. 멈춰 세워진 쪽을 닫으면 릴레이가 자기가 입을 막아 둔 정상 "
            "플레이어를 걷어낸 것이다:\n" + stats.dump())
        assert by_conn.get(conn_a) == "상대 이탈", (
            f"멈춰 세워졌던 쪽(A=conn {conn_a})이 그 여파로 닫힌 것이 아니다: "
            f"{closes}. 지목이 뒤집혔거나 다른 기제가 먼저 걷어갔다:\n" + stats.dump())
    finally:
        _shutdown(proc, (a, b))


def test_reactor_forwarding_start_does_not_refill_the_burst():
    """포워딩 시작이 레이트 버킷을 만충으로 되돌리면 안 된다.

    단계가 바뀐다고 그 연결이 다른 연결이 되는 것은 아니다. 예전에는 begin_forwarding
    이 토큰을 무조건 kRateBurstBytes 로 리셋해서, 로비에서 버스트를 다 태운 연결이
    포워딩 시작과 동시에 가득 찬 버킷을 한 번 더 받았다 — 연결당 실효 버스트가 사실상
    두 배다(2026-08-23 Linux 실측: 합계가 한도의 1.88배).

    로비 단계는 길이 0 프레임을 그 자리에서 소비하므로(on_lobby) 수신 버퍼 상한에
    걸리지 않고 토큰만 태울 수 있다. 그렇게 태운 뒤 READY 로 넘어가 전속력으로 부으면,
    리셋이 남아 있는 한 한도를 훌쩍 넘겨 통과한다.

    reactor 전용이다 — 스레드 모델에는 이 단계 전이가 없다.
    """
    reactor_bin = _require_reactor_step()

    BURST = 16 * 64 * 1024              # reactor_relay.cpp 의 kRateBurstBytes
    BURN  = 900 * 1024                  # 로비에서 미리 태울 양
    proc, port = _spawn_listening(
        lambda p: [str(reactor_bin), "--port", str(p), "--log-level", "error"])
    a = socket.create_connection(("127.0.0.1", port), timeout=3.0)
    b = socket.create_connection(("127.0.0.1", port), timeout=3.0)
    try:
        a.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        b.sendall(build_frame(MsgType.QUEUE_JOIN, b"\x00"))
        _recv_match_found(a)
        _recv_match_found(b)

        # 로비 단계 — 길이 0 프레임(2바이트 길이 + 4바이트 체크섬)으로 토큰만 태운다.
        block = (b"\x00\x00" + b"\x00\x00\x00\x00") * 1000
        burned = 0
        a.settimeout(5.0)
        try:
            while burned < BURN:
                a.sendall(block)
                burned += len(block)
        except OSError as exc:
            pytest.fail(f"로비에서 태우는 중 끊겼다: {exc!r}")

        a.sendall(build_frame(MsgType.READY, b"\x01"))
        b.sendall(build_frame(MsgType.READY, b"\x01"))
        time.sleep(0.3)

        # 상대는 계속 빼내 준다 — 백프레셔가 아니라 레이트 상한을 재는 자리다.
        # 세는 것은 **B 가 실제로 받은 양** 이다. A 쪽 send 가 받아 준 바이트를 세면
        # A 의 커널 송신 버퍼에 얼마나 고였는지가 섞여 들어와(머신이 바쁘면 끊김
        # 통지가 늦게 와서 그만큼 더 고인다) 릴레이가 통과시킨 양과 무관하게 커진다.
        stop = threading.Event()
        relayed = [0]

        def _drain():
            b.settimeout(0.3)
            while not stop.is_set():
                try:
                    got = b.recv(1 << 18)
                except socket.timeout:
                    continue
                except OSError:
                    return
                if not got:
                    return
                relayed[0] += len(got)

        th = threading.Thread(target=_drain, daemon=True)
        th.start()
        try:
            chunk = build_frame(MsgType.CHAT, b"x" * 3900)
            a.setblocking(False)
            pend = b""
            t0 = time.monotonic()
            while time.monotonic() - t0 < 10.0:
                if not pend:
                    pend = chunk
                try:
                    n = a.send(pend)
                except BlockingIOError:
                    time.sleep(0.002)
                    continue
                except OSError:
                    break               # 끊겼다 — 상한이 살아 있다는 뜻
                pend = pend[n:]
                if burned + relayed[0] > 3 * BURST:
                    break               # 상한이 사라졌다 — 아래 단언이 잡는다
            time.sleep(0.3)             # 마지막으로 흘려보낸 것까지 받게
        finally:
            stop.set()
            th.join(timeout=2)

        total = burned + relayed[0]
        assert total < BURST * 3 // 2, (
            f"로비에서 {burned} 바이트를 태우고도 포워딩에서 {relayed[0]} 바이트가 "
            f"더 통과했다 (합계 {total}, 버스트 한도 {BURST}). 단계 전이가 버킷을 "
            "만충으로 되돌리고 있다 — 연결당 실효 버스트가 두 배다")
    finally:
        a.close()
        b.close()
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()


# ── 레이트 상한 ──────────────────────────────────────────────────────────────
# 한 계약을 네 테스트가 나눠 잰다. 나누는 축은 "pause 를 몇 개, 얼마나 길게 끼우는가"
# 다 — 회계가 틀리는 방식이 그 축을 따라 갈리기 때문이다.
#   1) pause 없이: 상한이 실제로 강제되는가 — 버스트를 다 쓰면 끊고 사유를 그렇게 적는가
#   2) 밀리초 pause 여럿: 재개마다 갱신되는 면제가 상한을 무력화하지 않는가
#   3) 긴 pause 하나(큐 경로): 재개할 때 버킷 시계를 다시 켜 그 구간을 한 번만 세는가
#   4) 긴 pause 하나(룸 경로): 멈춰 세워진 채 포워딩에 들어선 연결의 버킷을 안 채우는가
# 넷이 서로를 대신하지 못한다. 옛 구멍("재개 후 3초 면제")은 1) 로는 발동조차 하지
# 않아 안 잡히고, 이중 계산은 2) 의 밀리초 pause 에서는 반올림돼 사라지며, 3) 과 4) 는
# 애초에 다른 줄을 겨눈다(실측으로 확인했다 — 각 docstring 참고).

def test_reactor_rate_cap_cuts_a_flood_and_says_why():
    """상한을 넘겨 부으면 릴레이가 끊고, 사유를 "byte rate 초과" 로 적어야 한다.

    계약의 절반 — 상한이 실제로 강제되는가. 토큰 버킷은 kRateBurstBytes(≈1 MiB)
    까지만 미리 봐주고 그 뒤로는 kMaxBytesPerSecond(64 KiB/s) 로만 채워지므로,
    전속력으로 붓는 쪽은 1 MiB 남짓에서 끊겨야 한다. 여기서는 백프레셔를 일으키지
    않는다 — B 가 전속력으로 빼내므로 릴레이의 보류 송신이 쌓이지 않는다. 상한
    하나만 겨누는 형태라 판정이 다른 기제와 섞이지 않고, 1초 안에 끝난다.

    문턱은 8 MiB 다. 정상 동작은 그 8분의 1 에서 끊기고, 상한이 없는 바이너리는
    문턱까지 아무 저항 없이 통과한다 — 사이에 애매한 구간이 없다.

    끊겼다는 사실만으로는 부족해서 사유까지 본다. 상한을 지웠는데 다른 기제가
    (송신 버퍼 하드 상한이든 유휴 만기든) 대신 끊으면 "끊겼다" 는 참이지만 이
    테스트가 겨눈 것은 하나도 안 지켜진 상태다.

    reactor 전용이다 — 스레드 모델에는 이 토큰 버킷이 없다.
    """
    reactor_bin = _require_reactor_step()

    LIMIT = 8 * 1024 * 1024
    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p),
                   "--log-level", "info", "--stats-interval-sec", "1"])
    a = b = None
    try:
        when = time.monotonic()
        a, b, conn_a, conn_b = _forwarding_pair(stats, port)

        # B 는 전속력으로 빼낸다. 여기서 밀리면 백프레셔가 끼어들어, 이 테스트가
        # 겨누는 것(상한 그 자체)이 아니라 다른 경로를 재게 된다.
        alive = [True]
        drained = [0]

        def drain():
            b.settimeout(0.2)
            while alive[0]:
                try:
                    got = b.recv(1 << 18)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not got:
                    break
                drained[0] += len(got)

        th = threading.Thread(target=drain, daemon=True)
        th.start()
        try:
            blob = build_frame(MsgType.CHAT, b"x" * 4000) * 16
            a.settimeout(1.0)
            feeder = _Feeder(a)
            closed = None
            deadline = time.monotonic() + 30.0
            while time.monotonic() < deadline:
                if feeder.backlog() < (1 << 18):
                    feeder.offer(blob)
                feeder.pump(1 << 18)
                closed = stats.wait_for_close_of(when, conn_a, timeout=0.0)
                if closed or feeder.pushed > LIMIT or not feeder.alive:
                    break
            sent = feeder.pushed
        finally:
            alive[0] = False
            th.join(timeout=2)

        closed = closed or stats.wait_for_close_of(when, conn_a, timeout=3.0)
        assert closed is not None, (
            f"레이트 상한이 안 걸렸다 — A(conn {conn_a}) 로 {sent} 바이트를 부었는데 "
            f"릴레이가 끊지 않았다 (버스트 허용치 {_RELAY_RATE_BURST_BYTES}). "
            "릴레이가 적은 줄:\n" + stats.dump())
        assert closed[1] == _RATE_REASON, (
            f"A(conn {conn_a}) 가 끊기긴 했는데 사유가 레이트 초과가 아니다: "
            f"{closed[1]!r}. 상한이 아니라 다른 기제가 걷어간 것이므로 이 테스트는 "
            "아무것도 보장하지 않는다:\n" + stats.dump())
        assert sent <= LIMIT, (
            f"끊기기까지 {sent} 바이트가 통과했다 (문턱 {LIMIT}). 상한이 있긴 한데 "
            f"실효 한도가 버스트 허용치({_RELAY_RATE_BURST_BYTES})보다 훨씬 크다:\n"
            + stats.dump())
    finally:
        _shutdown(proc, (a, b))


def test_reactor_rate_cap_holds_across_backpressure_pauses():
    """백프레셔로 멈췄다 풀리기를 반복해도 레이트 상한이 유지돼야 한다.

    계약의 나머지 절반. 멈춰 있는 동안 상대의 커널 버퍼에 쌓인 적체는 우리가 안
    읽어서 생긴 것이라, 그대로 "레이트 초과" 로 청구하면 우리가 만든 정체를 피해자에게
    무는 셈이 된다. 처음 그 문제를 "재개 후 3초 면제" 로 풀었는데 **면제를 재개 시각에
    묶은 것이 틀렸다** — 재개는 tx 가 high-water 아래로 떨어질 때마다 일어나므로,
    pause 와 resume 을 반복시키면 이전 면제가 만료되기 전에 새 면제가 걸려 상한이
    영영 적용되지 않는다(수정 전 Windows 실측: 3.7초에 42.2 MB, 상한의 172배).
    토큰 버킷에는 그 이음매가 없다.

    두 단계다.

    1단계 — **상한 아래로** 밀어(≈49 KiB/s) 백프레셔를 성립시킨다. 상한을 넘겨 밀면
    옛 코드는 면제가 걸리기도 전에 그냥 레이트 초과로 끊어서, 정작 재려던 구멍에
    도달하지 못한다. B 는 한 바이트도 읽지 않고 자기 유휴 데드라인만 살려 둔다.
    성립 판정은 릴레이가 찍는 [stats] 의 tx_peak 이다 — 보류 송신이 high-water 를
    넘어야만 그 값이 그 위로 올라가고, queue_send 는 peak 를 갱신한 바로 그 자리에서
    pause 를 건다. 클라이언트 sendall 이 막히는지는 보지 않는다.

    2단계 — B 가 ≈1 MiB/s 로 빼내기 시작하면 보류 송신이 high-water 를 오르내리며
    pause/resume 이 반복된다. 그 상태에서 A 가 전속력으로 부어도, 쌓아 둔 토큰(≈1 MiB)
    만큼 쓰고 나면 끊겨야 한다. 면제를 재개마다 갱신하는 바이너리는 여기서 영영
    안 끊긴다 — 문턱(3 MiB)까지 아무 저항 없이 통과한다.
    """
    reactor_bin = _require_reactor_step()

    # 2단계에서 릴레이를 통과한 바이트의 문턱. 정상 동작은 버스트 허용치(≈1 MiB)에
    # 2단계가 도는 1~2초치 충전(64 KiB/s)을 더한 값 근처에서 끊긴다(실측 1.16 MiB).
    # 3 MiB 면 그 위로 두 배 넘는 여유가 있다.
    LIMIT = 3 * 1024 * 1024
    # 1단계 페이스. 상한의 75% 다 — 루프 지터로 어느 1초 창이 부풀어도 상한을 넘지
    # 않을 만큼 낮고, 백프레셔를 세우는 데는 충분하다. 상한 아래로 보내는 송신자가
    # 이 단계에서 끊기면 그 자체가 결함이므로 아래에서 따로 확인한다.
    RAMP_TICK = 0.05
    ramp_chunk = build_frame(MsgType.CHAT, b"x" * 400) * 6      # ≈49 KiB/s
    RAMP_PER_TICK = len(ramp_chunk)
    # 2단계 배수 페이스(≈1 MiB/s). A 는 전속력으로 부으므로 릴레이가 A 에서 읽어낼
    # 수 있는 속도는 곧 B 가 빼내는 속도다 — 여기서 정해진다. 상한의 열여섯 배라
    # 토큰이 확실히 마르고(순감 ≈960 KiB/s), 매 틱 tx 가 high-water 아래로 떨어져
    # resume 이 반복된다. 잘게 자주 빼내는 것이 중요하다 — 한 번에 많이·드물게
    # 빼내면 B 의 수신 창이 매 틱 닫혀 커널이 zero-window persist 로 물러나고,
    # 배수가 상한과 자릿수가 같아져 토큰이 마르지 않는다(실측 29~103 KiB/s).
    DRAIN_PER_TICK = 16 * 1024
    DRAIN_TICK = 0.016

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p),
                   "--log-level", "info", "--stats-interval-sec", "1"])
    a = b = None
    try:
        when = time.monotonic()
        a, b, conn_a, conn_b = _forwarding_pair(stats, port)
        keepalive = _keepalive_frame()

        # ── 1단계: 상한 아래로 밀어 백프레셔를 성립시킨다 ────────────────────
        a.settimeout(0.5)
        b.settimeout(2.0)
        feeder = _Feeder(a)
        established = False
        ramp_started = time.monotonic()
        last_keepalive = ramp_started
        while time.monotonic() - ramp_started < 25.0:
            tick = time.monotonic()
            if feeder.backlog() < RAMP_PER_TICK:
                feeder.offer(ramp_chunk)
            feeder.pump(RAMP_PER_TICK)
            if not feeder.alive:
                break
            now = time.monotonic()
            if now - last_keepalive >= 0.4:
                last_keepalive = now
                # B 는 읽지 않지만 자기 유휴 데드라인은 살려 둬야 한다. 안 그러면
                # B 가 먼저 유휴로 걷히고 A 가 "상대 이탈" 로 따라 죽어, 재려던
                # 조건에 도달하지 못한다(34b5b11 이후 이 경로의 단골 함정).
                try:
                    b.sendall(keepalive)
                except OSError:
                    break
            if stats.latest().get("tx_peak", 0) > _RELAY_SEND_HIGH_WATER:
                established = True
                break
            rest = RAMP_TICK - (time.monotonic() - tick)
            if rest > 0:
                time.sleep(rest)
        ramp_s = time.monotonic() - ramp_started

        early = stats.closes_after(when, (conn_a, conn_b))
        assert not early, (
            f"1단계에서 벌써 끊겼다 {early} — 상한(64 KiB/s)의 75% 로 보내는 정상 "
            "송신자를, 혹은 그 상대를 걷어가고 있다:\n" + stats.dump())
        assert established, (
            f"{ramp_s:.1f}초를 밀었는데 보류 송신이 high-water"
            f"({_RELAY_SEND_HIGH_WATER}) 를 넘지 않았다 "
            f"(tx_peak={stats.latest().get('tx_peak', 0)}) — 백프레셔가 안 걸렸으니 "
            "2단계는 pause/resume 을 하나도 재지 못한다:\n" + stats.dump())

        # ── 2단계: B 가 빠르게 빼내는 동안 A 가 전속력으로 붓는다 ─────────────
        alive = [True]
        drained = [0]

        def drain():
            b.settimeout(0.02)
            last = time.monotonic()
            while alive[0]:
                tick = time.monotonic()
                taken = 0
                while taken < DRAIN_PER_TICK:
                    try:
                        got = b.recv(DRAIN_PER_TICK - taken)
                    except socket.timeout:
                        break
                    except OSError:
                        alive[0] = False
                        return
                    if not got:
                        alive[0] = False
                        return
                    taken += len(got)
                drained[0] += taken
                now = time.monotonic()
                if now - last >= 0.4:
                    last = now
                    try:
                        b.sendall(keepalive)
                    except OSError:
                        alive[0] = False
                        return
                rest = DRAIN_TICK - (time.monotonic() - tick)
                if rest > 0:
                    time.sleep(rest)

        th = threading.Thread(target=drain, daemon=True)
        th.start()
        try:
            blob = build_frame(MsgType.CHAT, b"x" * 4000) * 16
            closed = None
            flood_started = time.monotonic()
            deadline = flood_started + 40.0
            while time.monotonic() < deadline:
                if feeder.backlog() < (1 << 18):
                    feeder.offer(blob)
                feeder.pump(1 << 18)
                closed = stats.wait_for_close_of(when, conn_a, timeout=0.0)
                if closed or drained[0] > LIMIT or not alive[0] or not feeder.alive:
                    break
            passed = drained[0]
            flood_s = max(time.monotonic() - flood_started, 1e-6)
        finally:
            alive[0] = False
            th.join(timeout=2)

        # 배수가 상한과 자릿수가 같으면 토큰이 마를 수가 없어 아래 판정이 공허해진다.
        # 릴레이를 실제로 통과한 바이트로 확인한다.
        rate = passed / flood_s
        assert rate > 4 * _RELAY_MAX_BYTES_PER_SECOND, (
            f"2단계에서 릴레이를 통과한 속도가 {rate/1024:.0f} KiB/s 뿐이다 — 상한"
            f"({_RELAY_MAX_BYTES_PER_SECOND // 1024} KiB/s)과 자릿수가 같아 토큰이 마를 "
            "수 없다. 릴레이가 아니라 이 테스트의 배수 페이스가 문제다:\n"
            + stats.dump())

        closed = closed or stats.wait_for_close_of(when, conn_a, timeout=3.0)
        assert closed is not None, (
            f"pause/resume 이 반복되는 동안 A(conn {conn_a}) 가 끊기지 않았다 — "
            f"{passed} 바이트가 통과했다 (버스트 허용치 {_RELAY_RATE_BURST_BYTES}). "
            "재개마다 면제가 갱신되고 있다:\n" + stats.dump())
        assert closed[1] == _RATE_REASON, (
            f"A(conn {conn_a}) 가 끊긴 사유가 레이트 초과가 아니다: {closed[1]!r}. "
            "백프레셔로 멈춰 세운 송신자를 유휴로 오인했거나(34b5b11 이 못 박은 계약), "
            "다른 기제가 대신 걷어간 것이다:\n" + stats.dump())
        assert passed <= LIMIT, (
            f"끊기기까지 {passed} 바이트가 통과했다 (문턱 {LIMIT}). 상한이 있긴 한데 "
            "pause 를 거치면 실효 한도가 크게 늘어난다:\n" + stats.dump())
        # 한 번 멈춘 뒤 릴레이가 A 에서 더 읽을 수 있는 양은 4 KiB 남짓이다
        # (tcp_recv_some 은 한 번에 4096). 그보다 훨씬 많이 통과했다는 것은 그 사이에
        # resume 이 여러 번 있었다는 뜻 — 옛 코드가 면제를 갱신하던 그 지점을 실제로
        # 여러 번 지났다는 증거다.
        assert passed > 4 * _RELAY_SEND_HIGH_WATER, (
            f"끊기기 전에 통과한 양이 너무 적다 ({passed} 바이트) — 첫 pause 언저리에서 "
            "끝났다는 뜻이라 resume 반복을 재지 못했다:\n" + stats.dump())
    finally:
        _shutdown(proc, (a, b))


def test_reactor_bills_a_long_pause_once_not_twice():
    """긴 pause 하나를 사이에 두고도 레이트 상한이 유지돼야 한다.

    바로 위 테스트가 재는 pause 는 밀리초 단위다 — B 가 빠르게 빼내는 동안 tx 가
    high-water 를 오르내리는 형태라, 회계가 한 pause 를 두 번 세도 늘어나는 몫이
    문턱(3 MiB) 앞에서 반올림돼 사라진다. 이중 계산은 **긴** pause 에서만 금액이
    된다: 9초를 멈춰 세우면 그 구간의 몫이 576 KiB 다.

    이 형태가 못 박는 것은 재개할 때 버킷 시계를 다시 켜는 줄(pause_peer_read 의
    ``rate_refilled_at = now``)이다. 그 줄이 없으면 멈춰 있던 구간이 pause 예산과
    버킷 충전 양쪽에 들어간다(실측: 이 테스트가 그 바이너리를 잡는다). 같은 이중
    계산을 막는 refill_tokens 의 read_paused 갈래는 큐 경로에서는 실행되지 않는다 —
    멈춰 세워진 연결에는 읽기 이벤트가 없기 때문이다. 그쪽은 아래
    test_reactor_does_not_refill_a_paused_connections_bucket 이 룸 경로로 겨눈다.

    그래서 여기서는 상한을 시간의 함수로 걸고 잰다. A 가 내놓는 **총량**을 정직한
    허용치 — 버스트 한도 + 상한 × 흐른 시간 — 보다 정확히 ``OVERDRAFT`` 만큼 많게
    유지하면, 판정이 벽시계에 좌우되지 않는다. 어느 시점에 재도 초과분은 그대로
    ``OVERDRAFT`` 이므로 상한을 지키는 릴레이는 반드시 A 를 끊고, 멈춘 구간을 두 번
    센 릴레이는 그 두 배 가까운 여유(768 KiB) 안에 있어 끊지 않는다. 두 값의 간격이
    판정의 여유다.

    버킷을 미리 태우는 것이 이 형태의 전제다. 멈추기 시작할 때 토큰이 천장 근처면
    이중 계산분이 천장에 잘려(min(상한 × pause, 버스트 - 잔량)) 두 바이너리가 같아진다.
    로비에서 태우고 파이프를 채우고 나면 잔량은 200 KiB 대로 내려온다.

    reactor 전용이다. 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다.
    """
    reactor_bin = _require_reactor_step()

    # 로비에서 미리 태울 양. 남기는 잔량(≈394 KiB)은 파이프를 채우는 몫(≈210 KiB)보다
    # 넉넉히 크되, 멈출 때의 잔량이 버스트 천장에서 멀도록 충분히 커야 한다.
    BURN = 630 * 1024
    # 멈춰 두는 시간. pause 상한(kMaxPauseDuration, 16초) 아래여야 한다 — 넘기면
    # 릴레이가 안 빼내는 B 를 걷어가서 재려던 것과 다른 갈래를 재게 된다.
    HOLD_SEC = 9.0
    # A 가 정직한 허용치보다 더 내놓는 양. 이중 계산이 만드는 여유(상한 × HOLD_SEC =
    # 576 KiB)보다 확실히 작고, 페이스 지터(실측 ±2 KiB)보다는 확실히 커야 한다.
    OVERDRAFT = 280 * 1024

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p),
                   "--log-level", "info", "--stats-interval-sec", "1"])
    a = b = None
    try:
        when = time.monotonic()
        # 허용치를 세는 기준 시각. 버킷은 연결이 생기는 순간 만충이므로 여기서 잡는다.
        # 실제 accept 는 이 뒤라 우리가 계산하는 허용치는 릴레이의 것보다 아주 조금
        # 크다 — 그만큼 판정은 "끊겨야 한다" 쪽으로 보수적이다.
        t0 = time.monotonic()
        a, b, conn_a, conn_b = _forwarding_pair(stats, port, lobby_burn=BURN)
        # 로비에서 태운 몫은 아래 페이스에 이미 포함돼 있으므로 floor 에서 뺀다.
        floor = _RELAY_RATE_BURST_BYTES + OVERDRAFT - BURN

        a.setblocking(False)
        b.setblocking(False)
        sender, quiet = _Feeder(a), _Feeder(b)
        keepalive = _keepalive_frame()
        last_keepalive = [0.0]

        def _tick_quiet_side() -> None:
            # B 는 한 바이트도 빼내지 않지만 자기 유휴 데드라인은 살려 둔다. 안
            # 그러면 B 가 먼저 유휴로 걷히고 A 가 "상대 이탈" 로 따라 죽는다.
            now = time.monotonic()
            if now - last_keepalive[0] >= 3.0:
                last_keepalive[0] = now
                quiet.offer(keepalive)
            quiet.pump()

        # ── 1단계: 파이프를 채워 릴레이가 A 의 읽기를 멈추게 한다 ─────────────
        established = False
        deadline = time.monotonic() + 25.0
        while time.monotonic() < deadline:
            _pace(sender, _BP_CHUNK, _RELAY_MAX_BYTES_PER_SECOND, t0, floor=floor)
            _tick_quiet_side()
            _drain_nonblocking(a)
            if stats.latest().get("tx_peak", 0) > _RELAY_SEND_HIGH_WATER:
                established = True
                break
            time.sleep(0.01)
        ramp_s = time.monotonic() - t0

        early = stats.closes_after(when, (conn_a, conn_b))
        assert not early, (
            f"파이프를 채우는 중에 벌써 끊겼다: {early} (A=conn {conn_a}, "
            f"B=conn {conn_b}). 태우고 남은 토큰({_RELAY_RATE_BURST_BYTES - BURN})이 "
            "이 구간을 못 덮는다는 뜻이라, 태우는 양을 줄여야 한다:\n" + stats.dump())
        assert established, (
            f"{ramp_s:.1f}초를 밀었는데 보류 송신이 high-water"
            f"({_RELAY_SEND_HIGH_WATER}) 를 넘지 않았다 "
            f"(tx_peak={stats.latest().get('tx_peak', 0)}) — 릴레이가 A 의 읽기를 "
            "멈춘 적이 없으므로 잴 pause 자체가 없다:\n" + stats.dump())

        # ── 2단계: 멈춰 세워 둔 채로 오래 버틴다 ────────────────────────────
        # A 는 계속 내놓지만 릴레이는 읽지 않는다 — 이 구간의 몫이 재개할 때
        # 예산으로 한 번만 지급돼야 하는 그 몫이다.
        paused_at = time.monotonic()
        while time.monotonic() - paused_at < HOLD_SEC:
            _pace(sender, _BP_CHUNK, _RELAY_MAX_BYTES_PER_SECOND, t0, floor=floor)
            _tick_quiet_side()
            _drain_nonblocking(a)
            time.sleep(0.02)
        held_s = time.monotonic() - paused_at

        during = stats.samples_after(paused_at)
        assert not stats.closes_after(when, (conn_a, conn_b)), (
            f"멈춰 두는 사이에 끊겼다: {stats.closes_after(when, (conn_a, conn_b))} "
            f"(A=conn {conn_a}, B=conn {conn_b}):\n" + stats.dump())
        assert len(during) >= 3 and all(s.get("tx", 0) > _RELAY_SEND_HIGH_WATER
                                        for s in during), (
            f"{held_s:.1f}초 동안 보류 송신이 high-water 위에 머물지 않았다 "
            f"(표본 {[s.get('tx') for s in during]}) — 중간에 백프레셔가 풀렸다면 "
            "이 구간은 하나의 긴 pause 가 아니라 짧은 pause 여럿이고, 그러면 이중 "
            "계산분이 다시 반올림돼 사라진다:\n" + stats.dump())

        # ── 3단계: B 가 빼내기 시작한다. 멈춰 있던 구간의 적체가 한꺼번에 읽힌다 ──
        # 예산은 그 구간을 한 번만 지불한다. A 는 그동안에도 계속 "허용치 +
        # OVERDRAFT" 를 유지하므로, 상한을 지키는 릴레이라면 반드시 끊는다.
        drained = 0
        closed = None
        release_started = time.monotonic()
        while time.monotonic() - release_started < 15.0:
            _pace(sender, _BP_CHUNK, _RELAY_MAX_BYTES_PER_SECOND, t0, floor=floor)
            drained += _drain_nonblocking(b)
            _tick_quiet_side()
            _drain_nonblocking(a)
            closed = stats.wait_for_close_of(when, conn_a, timeout=0.0)
            if closed:
                break
            time.sleep(0.005)
        release_s = time.monotonic() - release_started
        offered = BURN + sender.offered_total()
        allowed = _RELAY_RATE_BURST_BYTES + int(
            _RELAY_MAX_BYTES_PER_SECOND * (time.monotonic() - t0))

        assert drained > 4 * _RELAY_SEND_HIGH_WATER, (
            f"풀어 준 뒤 B 가 받은 양이 {drained} 바이트뿐이다 — 릴레이가 멈춰 있던 "
            "동안의 적체를 실제로 읽지 못했다는 뜻이라, 이 실행은 상한을 재지 "
            "않았다:\n" + stats.dump())
        assert closed is not None, (
            f"A(conn {conn_a}) 가 끊기지 않았다. {held_s:.1f}초를 멈춰 세운 뒤 "
            f"{release_s:.1f}초를 풀어 뒀고, A 가 내놓은 총량 {offered} 는 정직한 "
            f"허용치 {allowed}(버스트 {_RELAY_RATE_BURST_BYTES} + 상한 × 경과)를 "
            f"{offered - allowed} 바이트 넘는다. 멈춘 구간을 버킷 충전과 pause "
            "예산에서 두 번 세면 딱 이만큼 헐거워진다:\n" + stats.dump())
        assert closed[1] == _RATE_REASON, (
            f"A(conn {conn_a}) 가 끊긴 사유가 레이트 초과가 아니다: {closed[1]!r} — "
            "다른 기제가 대신 걷어갔다면 이 실행은 상한을 재지 못했다:\n"
            + stats.dump())
    finally:
        _shutdown(proc, (a, b))


def test_reactor_does_not_refill_a_paused_connections_bucket():
    """멈춰 세워 둔 연결의 버킷을 그 구간만큼 채우면 안 된다.

    멈춘 구간의 몫은 재개할 때 grant_pause_credit 이 천장 없이 한 번 지급한다.
    refill_tokens 가 같은 구간을 버킷에도 채우면 한 시간이 두 번 계산돼 실효 상한이
    최대 2배가 된다 — 그래서 그 자리에 read_paused 갈래가 있다.

    그 갈래가 실제로 실행되는 경로는 하나뿐이다. 멈춰 세워진 연결에는 읽기 이벤트가
    없으므로 on_readable 의 refill_tokens 는 애초에 안 불리고, 멈추는 순간의
    refill_tokens 는 read_paused 를 켜기 **전에** 부른다. 남는 자리는 begin_forwarding
    이다 — 룸 단계에서 백프레셔로 멈춰 세워진 채 포워딩으로 넘어오는 연결이 거기서
    refill_tokens 를 만난다(같은 자리의 tx_drained_at 주석이 그 상황을 명시한다).
    큐 경로에서는 로비가 게임 프레임을 흘려보내지 않아 아무도 멈춰 있지 않으므로,
    이 갈래를 겨누려면 룸 경로여야 한다. 실측(2026-08-23 Linux): 큐 경로로 12초를
    멈춰 세운 형태는 가드를 지운 바이너리를 그대로 통과시켰고, 아래 형태는 잡았다.

    구성:
      · A 가 룸 단계에서 무시되는 프레임(INPUT)으로 토큰을 먼저 태운다. 그 프레임은
        상대에게 전달되지 않으므로(on_room 은 READY/CHAT/ROOM_LEAVE 만 본다) tx 를
        건드리지 않고 버킷만 정확히 줄인다. 태워 두지 않으면 이중 계산분이 버스트
        천장에 잘려(min(상한 × pause, 버스트 - 잔량)) 두 바이너리가 같아진다.
      · A 가 CHAT 으로 B 의 파이프를 채워 릴레이가 A 의 읽기를 멈추게 한다. B 는
        READY 를 아직 안 보냈으므로 매치는 시작되지 않는다.
      · 그 상태로 ROOM_HOLD_SEC 를 버틴 뒤 B 가 READY 를 보낸다. 멈춘 채로
        begin_forwarding 을 지나는 것이 이 테스트가 만드는 조건이다.
      · 풀어 주고, A 가 내놓은 총량이 정직한 허용치(버스트 + 상한 × 흐른 시간)를
        OVERDRAFT 만큼 넘는지로 판정한다. 상한을 지키는 릴레이는 반드시 끊고,
        멈춘 구간을 두 번 센 릴레이는 그 두 배 가까운 여유 안에 있어 끊지 않는다.

    reactor 전용이다. 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다.
    """
    reactor_bin = _require_reactor_step()

    # 룸 단계에서 태울 양. 남기는 잔량이 파이프를 채우는 몫보다 넉넉히 크되(안 그러면
    # 백프레셔를 세우기 전에 A 가 정당하게 끊긴다), 멈출 때의 잔량이 버스트 천장에서
    # 충분히 멀어야 한다(안 그러면 이중 계산분이 천장에 잘린다).
    BURN = 384 * 1024
    # 룸 단계에서 멈춰 두는 시간. 이 길이가 곧 이중 계산분(상한 × 이 값)이다.
    ROOM_HOLD_SEC = 9.0
    # A 가 정직한 허용치보다 더 내놓는 양. 이중 계산분(576 KiB)보다 확실히 작고,
    # 페이스 지터(실측 ±1 KiB)보다는 확실히 커야 한다.
    OVERDRAFT = 280 * 1024

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p),
                   "--log-level", "info", "--stats-interval-sec", "1"])
    a = b = None
    try:
        when = time.monotonic()
        # 허용치를 세는 기준. 버킷은 연결이 생기는 순간 만충이므로 여기서 잡는다.
        t0 = time.monotonic()
        a = socket.create_connection(("127.0.0.1", port), timeout=3.0)
        b = socket.create_connection(("127.0.0.1", port), timeout=3.0)
        # B 의 수신 창을 고정한다. 자동 조정에 맡기면 파이프 용량이 실행마다 달라져
        # "백프레셔를 세우는 데 드는 토큰" 이 흔들리고, 그 값이 곧 멈출 때의 잔량이라
        # 이중 계산분이 천장에 잘리는지가 운에 좌우된다.
        b.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 32768)
        a_buf, b_buf = bytearray(), bytearray()
        a.sendall(_build_room_create(""))
        code, _, _ = _parse_room_info(_recv_frame(a, MsgType.ROOM_INFO, a_buf))
        b.sendall(_build_room_join(code, ""))
        _recv_frame(b, MsgType.ROOM_INFO, b_buf)
        # A 는 먼저 READY 를 보내 둔다 — 멈춰 세워진 뒤에는 릴레이가 A 를 안 읽으므로
        # 그때 보내면 영영 도착하지 않는다. B 의 READY 하나만 남겨 두면 매치가
        # 시작되는 시점을 테스트가 정확히 고를 수 있다.
        a.sendall(build_frame(MsgType.READY, b"\x01"))
        a.setblocking(False)
        b.setblocking(False)
        sender, quiet = _Feeder(a), _Feeder(b)
        floor = _RELAY_RATE_BURST_BYTES + OVERDRAFT
        keepalive = _keepalive_frame()
        last_keepalive = [0.0]

        def _tick_quiet_side() -> None:
            # B 는 한 바이트도 빼내지 않지만 포워딩에 들어선 뒤로는 자기 유휴
            # 데드라인을 살려 둬야 한다. 안 그러면 B 가 먼저 걷히고 A 가 "상대 이탈"
            # 로 따라 죽어, 재려던 조건에 닿지 못한 채 "끊겼다" 만 남는다.
            now = time.monotonic()
            if now - last_keepalive[0] >= 3.0:
                last_keepalive[0] = now
                quiet.offer(keepalive)
            quiet.pump()

        # ── 1단계: 룸 단계에서 토큰만 태운다 ─────────────────────────────────
        while sender.offered_total() < BURN:
            sender.offer(_ROOM_BURN_FRAME)
        deadline = time.monotonic() + 10.0
        while sender.backlog() and time.monotonic() < deadline:
            sender.pump()
            _drain_nonblocking(a)
            time.sleep(0.002)
        assert not sender.backlog(), (
            f"룸 단계에서 태우려던 {BURN} 바이트가 다 안 나갔다 "
            f"(남은 {sender.backlog()}) — 태운 양을 모르면 잔량도 모른다:\n"
            + stats.dump())

        # ── 2단계: CHAT 으로 파이프를 채워 릴레이가 A 의 읽기를 멈추게 한다 ───
        established = False
        deadline = time.monotonic() + 25.0
        while time.monotonic() < deadline:
            _pace(sender, _BP_CHUNK, _RELAY_MAX_BYTES_PER_SECOND, t0, floor=floor)
            _drain_nonblocking(a)
            if stats.latest().get("tx_peak", 0) > _RELAY_SEND_HIGH_WATER:
                established = True
                break
            time.sleep(0.01)
        paused_at = time.monotonic()

        # 아직 페어링 줄이 안 나와 우리 연결 번호를 모르므로 사유로 거른다. 기동
        # 확인용 탐침(_wait_listen)도 종료 줄("peer 종료")을 남기고 그 줄이 도착하는
        # 시각은 얼마든지 뒤로 밀리므로, 번호도 시각도 여기서는 기준이 못 된다.
        # 이 단계에서 뜻이 있는 죽음은 하나뿐이다 — 태우는 양이 과해서 A 가 레이트로
        # 끊기는 것. 그 밖의 죽음은 아래 페어링 대기가 전문(dump)과 함께 잡는다.
        early = [row for row in stats.closes_after(when) if row[1] == _RATE_REASON]
        assert not early, (
            f"룸 단계에서 벌써 레이트로 끊겼다: {early}. 태우고 남은 토큰"
            f"({_RELAY_RATE_BURST_BYTES - BURN})으로 파이프를 못 채운다는 뜻이라, "
            "태우는 양을 줄여야 한다:\n" + stats.dump())
        assert established, (
            f"{paused_at - t0:.1f}초를 밀었는데 보류 송신이 high-water"
            f"({_RELAY_SEND_HIGH_WATER}) 를 넘지 않았다 "
            f"(tx_peak={stats.latest().get('tx_peak', 0)}) — 릴레이가 A 의 읽기를 "
            "멈춘 적이 없으므로 잴 pause 자체가 없다:\n" + stats.dump())

        # ── 3단계: 멈춘 채로 버틴다. 매치는 아직 시작되지 않았다 ─────────────
        while time.monotonic() - paused_at < ROOM_HOLD_SEC:
            _pace(sender, _BP_CHUNK, _RELAY_MAX_BYTES_PER_SECOND, t0, floor=floor)
            _drain_nonblocking(a)
            time.sleep(0.02)
        held_s = time.monotonic() - paused_at

        # ── 4단계: B 의 READY. A 는 멈춘 채로 begin_forwarding 을 지난다 ─────
        quiet.offer(build_frame(MsgType.READY, b"\x01"))
        deadline = time.monotonic() + 5.0
        while quiet.backlog() and time.monotonic() < deadline:
            quiet.pump()
            time.sleep(0.002)
        conn_a, conn_b = stats.wait_for_pairing_after(when)   # make_channel(host, guest)

        during = stats.samples_after(paused_at)
        assert len(during) >= 3 and all(s.get("tx", 0) > _RELAY_SEND_HIGH_WATER
                                        for s in during), (
            f"{held_s:.1f}초 동안 보류 송신이 high-water 위에 머물지 않았다 "
            f"(표본 {[s.get('tx') for s in during]}) — 중간에 백프레셔가 풀렸다면 A 는 "
            "멈춘 채로 포워딩에 들어선 것이 아니고, 그러면 이 테스트가 겨누는 갈래는 "
            "한 번도 실행되지 않는다:\n" + stats.dump())

        # ── 5단계: 풀어 준다. 멈춰 있던 구간의 적체가 한꺼번에 읽힌다 ────────
        drained = 0
        closed = None
        release_started = time.monotonic()
        while time.monotonic() - release_started < 15.0:
            _pace(sender, _BP_CHUNK, _RELAY_MAX_BYTES_PER_SECOND, t0, floor=floor)
            drained += _drain_nonblocking(b)
            _tick_quiet_side()
            _drain_nonblocking(a)
            closed = stats.wait_for_close_of(when, conn_a, timeout=0.0)
            if closed:
                break
            time.sleep(0.005)
        release_s = time.monotonic() - release_started
        offered = sender.offered_total()
        allowed = _RELAY_RATE_BURST_BYTES + int(
            _RELAY_MAX_BYTES_PER_SECOND * (time.monotonic() - t0))

        assert drained > 4 * _RELAY_SEND_HIGH_WATER, (
            f"풀어 준 뒤 B 가 받은 양이 {drained} 바이트뿐이다 — 릴레이가 멈춰 있던 "
            "동안의 적체를 실제로 읽지 못했다는 뜻이라, 이 실행은 상한을 재지 "
            "않았다:\n" + stats.dump())
        assert closed is not None, (
            f"A(conn {conn_a}) 가 끊기지 않았다. 룸 단계에서 {held_s:.1f}초를 멈춰 "
            f"세운 채 포워딩으로 넘긴 뒤 {release_s:.1f}초를 풀어 뒀고, A 가 내놓은 "
            f"총량 {offered} 는 정직한 허용치 {allowed}(버스트 "
            f"{_RELAY_RATE_BURST_BYTES} + 상한 × 경과)를 {offered - allowed} 바이트 "
            "넘는다. 멈춰 있던 연결의 버킷을 그 구간만큼 채우면 딱 이만큼 "
            "헐거워진다:\n" + stats.dump())
        assert closed[1] == _RATE_REASON, (
            f"A(conn {conn_a}) 가 끊긴 사유가 레이트 초과가 아니다: {closed[1]!r} — "
            "다른 기제가 대신 걷어갔다면 이 실행은 상한을 재지 못했다:\n"
            + stats.dump())
    finally:
        _shutdown(proc, (a, b))


# ── 상호 백프레셔 교착과 그 반대편 ───────────────────────────────────────────
# 아래 셋은 on_timeout 의 같은 분기를 세 방향에서 누른다. 하나만 고정하면 나머지가
# 자유롭게 망가진다 — 회수 갈래는 재무장 갈래를 통째로 잘라 내는 것으로도 "통과" 하고,
# 재무장 갈래는 무조건 재무장으로 되돌리는 것으로도 "통과" 한다. 세 번째는 상한이
# **누구의 시계로** 재는가다: 배수마다 pause 시계가 다시 시작되지 않으면, 느리지만
# 실제로 빼내는 상대와 붙은 정상 송신자가 16초마다 걷힌다.

def test_reactor_reclaims_a_mutually_backpressured_pair():
    """서로를 멈춰 세운 두 연결은 만기에 회수돼야 한다.

    A 와 B 가 서로에게 붓기만 하고 아무도 읽지 않으면, 릴레이는 양쪽 tx 가
    high-water 를 넘는 순간 **양쪽의 읽기를 모두** 멈춘다. 그때부터 두 연결은 읽기
    이벤트가 영영 안 나고(멈춰 뒀으니), 쓰기도 진행되지 않으며(상대가 안 읽으니),
    tx 도 더 안 자란다(멈춰 뒀으니) — 송신 버퍼 하드 상한도 영영 안 온다. 유휴
    만기가 무조건 재무장하면 이 페어는 프로세스 재시작까지 fd 2개, per-IP 세션 슬롯
    2개, 매치 1개를 물고 있는다. 주소를 갈아 가며 반복하면 연결 풀이 통째로
    고정된다 — MEMORY 에 HIGH DoS 로 적힌 그 결함이다(34b5b11 이 고쳤다).

    판정은 세 가지를 순서대로 본다.
      1) 양쪽이 정말로 멈췄다: 붓기를 계속하는데도 [stats] 의 tx 가 방향당
         high-water 두 몫을 넘긴 채 더 안 자란다. tx 절대값만으로는 "두 방향이 각각
         넘었다" 와 "한 방향이 두 배로 물고 있다" 를 구분하지 못하지만(한 번의 read
         가 4 KiB 를 얹는다), 안 멈춘 쪽이 하나라도 있으면 릴레이가 거기서 계속 읽어
         tx 가 계속 자라므로 **붓는 중의 정지** 가 곧 "양쪽 다 멈춤" 이다.
      2) 페어가 닫힌다 — 그것도 **교착 사유로**. 사유를 안 보면 유휴 만기나 레이트
         초과로 끊긴 것을 회수로 오인한다. 회수가 없던 시절에도 다른 이유로는
         끊길 수 있었다.
      3) 자원이 실제로 돌아온다: 상태 줄이 conns=0 matches=0 이 된다.

    reactor 전용이다 — 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다.
    """
    reactor_bin = _require_reactor_step()

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p), "--log-level", "info",
                   "--stats-interval-sec", "1",
                   "--idle-timeout-sec", str(_BP_IDLE_SEC)])
    a = b = None
    try:
        a, b, conn_a, conn_b = _forwarding_pair(stats, port)
        # 양쪽 수신 버퍼를 좁혀 두 방향이 비슷한 시점에 high-water 를 넘게 한다.
        for s in (a, b):
            s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            s.setblocking(False)
        time.sleep(0.3)

        started = time.monotonic()
        feeders = [_Feeder(a), _Feeder(b)]

        # 1단계 — 서로에게 붓는다. 아무도 읽지 않는다. 정지에 빨리 닿을수록 좋다:
        # 양쪽이 멈춘 뒤 만기까지가 관측 창인데, 거기서 [stats] 표본 두 개를 봐야 한다.
        stalled = {}
        deadline = started + 25.0
        while time.monotonic() < deadline:
            seen = stats.samples_after(started)
            if (len(seen) >= 2 and seen[-1].get("tx", 0) >= 2 * _RELAY_SEND_HIGH_WATER
                    and seen[-1]["tx"] == seen[-2]["tx"]):
                stalled = seen[-1]
                break
            for f in feeders:
                while f.backlog() < 4 * len(_BP_CHUNK) and f.offered_total() < _BP_FLOOD_CAP:
                    f.offer(_BP_CHUNK)
                f.pump()
            time.sleep(0.005)
        assert stalled, (
            "붓는 동안 tx 가 양방향 high-water 를 넘긴 채 멎지 않았다 — 양쪽이 다 "
            "멈춘 상태에 도달하지 못했으므로 이 테스트는 아무것도 재고 있지 않다:\n"
            + stats.dump())

        paused_at = time.monotonic()
        # 2단계 — 여기서부터 아무도 아무것도 안 보낸다. 릴레이만 스스로 움직인다.
        # 만기 두 번 반의 여유를 준다: 양쪽이 정확히 같은 순간에 멈추지는 않으므로
        # 먼저 멈춘 쪽은 첫 만기에서 (상대가 아직 멀쩡해) 한 번 재무장할 수 있다.
        # 천장(kMaxPauseDuration)을 넘겨 기다린다 — 만기의 배수로 잡으면 릴레이가
        # 재무장할 권리를 가진 구간 안에서 테스트가 먼저 포기한다.
        closes = stats.wait_for_closes_after(
            started, 2, _BP_MAX_PAUSE_SEC + _BP_IDLE_SEC + 6, (conn_a, conn_b))
        reasons = [why for _, why in closes]
        assert _DEADLOCK_REASON in reasons, (
            f"교착된 페어가 회수되지 않았다 (만기 {_BP_IDLE_SEC}초, "
            f"{time.monotonic() - paused_at:.1f}초 관측). 이 구간의 종료 줄: "
            f"{reasons or '없음'} — 하나도 없으면 페어가 불멸이고, 사유가 다르면 "
            "교착 회수가 아니라 다른 이유로 끊긴 것이다:\n" + stats.dump())
        assert len(closes) == 2, (
            f"교착 회수는 페어 전체를 걷어야 한다. 종료된 연결: {closes} "
            f"(A=conn {conn_a}, B=conn {conn_b}):\n" + stats.dump())

        # 3단계 — 자원이 정말 돌아왔는지는 릴레이 자신에게 묻는다.
        final = stats.wait_for_sample_after(time.monotonic())
        assert final.get("conns") == 0 and final.get("matches") == 0, (
            f"페어를 닫았는데 자원이 안 돌아왔다 (conns={final.get('conns')} "
            f"matches={final.get('matches')}):\n" + stats.dump())
    finally:
        _shutdown(proc, (a, b))


def test_reactor_keeps_a_one_sided_backpressured_sender():
    """한쪽만 멈춘 정상 백프레셔에서는 멈춰 세워진 송신자를 끊으면 안 된다.

    위 테스트가 못 박는 회수 갈래는 재무장 갈래를 잘라 내는 방식으로도 만들 수
    있다 — 그리고 그러면 릴레이가 자기가 입을 막아 둔 정상 플레이어를 유휴로
    오인해 끊는다. 안 읽는 쪽은 멀쩡히 남고 보내던 쪽이 나간다. 회수와 이 계약은
    같은 분기의 양면이라 반드시 함께 고정해야 한다.

    구성은 교착과 딱 하나가 다르다. B 는 여전히 한 바이트도 안 읽지만 자기 유휴
    데드라인은 살려 둔다(작은 프레임을 주기적으로 보낸다). 그래서 B 는 유휴가
    아니고, 멈춘 것은 A 뿐이다. A 는 상대가 보낸 것을 정상적으로 읽는다.

    A 가 멈춰 있는 동안 A 의 데드라인은 갱신되지 않는다 — 멈춘 연결에는 읽기
    이벤트가 없기 때문이다. 그래서 만기는 반드시 A 를 찾아온다. 그때 재무장 갈래가
    없으면 A 는 첫 만기에서 곧바로 "idle 타임아웃" 으로 끊긴다. 만기 세 번 분량을
    지나도록 A 가 살아 있다는 것이 그 갈래가 실제로 실행됐다는 뜻이다. (재무장은
    다음 확인을 pause 상한 시점에 걸므로 실행 횟수는 한 번이면 족하다. 관측 창을
    그보다 길게 잡는 것은 "우연히 아직 안 왔다" 를 배제하기 위해서다.)

    B 의 데드라인을 살려 두는 것이 이 구성의 핵심이다. 안 그러면 B 가 먼저 유휴로
    걷히고, 그 여파로 A 까지 "상대 이탈" 로 닫혀 재려던 조건에 닿지도 못한 채
    "끊겼다" 만 남는다.

    reactor 전용이다 — 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다.
    """
    reactor_bin = _require_reactor_step()

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p), "--log-level", "info",
                   "--stats-interval-sec", "1",
                   "--idle-timeout-sec", str(_BP_IDLE_SEC)])
    a = b = None
    try:
        a, b, conn_a, conn_b = _forwarding_pair(stats, port)
        # B 의 수신 버퍼만 좁힌다 — 밀리는 방향은 A→B 하나뿐이다.
        b.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        a.setblocking(False)
        b.setblocking(False)
        time.sleep(0.3)

        started = time.monotonic()
        sender, quiet = _Feeder(a), _Feeder(b)
        keepalive = _keepalive_frame()
        # B 는 안 읽되 자기 데드라인은 살려 둔다. 간격은 만기의 1/4 로 잡는다 —
        # 만기에 가까우면 B 가 유휴로 걷히는지가 타이밍에 따라 갈리고, 그러면 A 가
        # "상대 이탈" 로 닫힌 것을 재무장 실패로 오인하게 된다.
        keepalive_every = _BP_IDLE_SEC / 4.0
        last_keepalive = [0.0]

        def _tick_quiet_side() -> None:
            now = time.monotonic()
            if now - last_keepalive[0] >= keepalive_every:
                last_keepalive[0] = now
                quiet.offer(keepalive)
            quiet.pump()

        # 1단계 — A 를 밀어 올려 릴레이가 A 의 읽기를 멈추게 한다. 멈췄다는 것은
        # 릴레이가 말해 준다: B 를 향한 보류 송신이 high-water 를 넘었다는 뜻이고,
        # 그 소켓으로 흘려보내는 쪽(A)의 읽기를 멈추는 것이 곧 그 조건이다.
        pending = 0
        deadline = started + 25.0
        while time.monotonic() < deadline:
            pending = stats.latest_after(started).get("tx", 0)
            if pending > _RELAY_SEND_HIGH_WATER:
                break
            while (sender.backlog() < 4 * len(_BP_CHUNK)
                   and sender.offered_total() < _BP_FLOOD_CAP):
                sender.offer(_BP_CHUNK)
            sender.pump()
            _tick_quiet_side()
            _drain_nonblocking(a)
            time.sleep(0.01)
        assert pending > _RELAY_SEND_HIGH_WATER, (
            f"보류 송신이 high-water 를 못 넘었다 (tx={pending}) — 릴레이가 A(conn "
            f"{conn_a})의 읽기를 멈춘 적이 없으므로 이 테스트는 아무것도 재고 있지 "
            "않다:\n" + stats.dump())

        # 2단계 — 멈춘 채로 만기를 세 번 넘긴다. A 는 아무것도 안 보낸다(보낼 수도
        # 없다 — 릴레이가 안 읽는다). 그동안 릴레이가 A 를 걷어가면 안 된다.
        paused_at = time.monotonic()
        hold_until = paused_at + _BP_IDLE_SEC * 3
        while time.monotonic() < hold_until:
            _tick_quiet_side()
            _drain_nonblocking(a)
            time.sleep(0.02)

        closes = stats.closes_after(started, (conn_a, conn_b))
        assert not closes, (
            f"만기 {_BP_IDLE_SEC}초를 {time.monotonic() - paused_at:.1f}초 동안 "
            f"넘기는 사이 연결이 끊겼다: {closes} (A=conn {conn_a}, B=conn {conn_b}). "
            "'idle 타임아웃' 이면 백프레셔로 멈춰 세운 송신자를 유휴로 오인한 것이고, "
            "교착 사유면 한쪽만 멈춘 정상 백프레셔를 교착으로 오판한 것이다:\n"
            + stats.dump())

        # 그리고 그 생존이 "아직도 멈춰 있는 채" 여야 한다. 사이에 백프레셔가
        # 풀렸다면 만기를 맞은 것은 멈춘 연결이 아니라 평범한 연결이고, 재무장
        # 갈래는 한 번도 실행되지 않은 것이다.
        final = stats.wait_for_sample_after(time.monotonic())
        assert final.get("conns") == 2 and final.get("matches") == 1, (
            f"페어가 온전히 남지 않았다 (conns={final.get('conns')} "
            f"matches={final.get('matches')}):\n" + stats.dump())
        assert final.get("tx", 0) > _RELAY_SEND_HIGH_WATER, (
            f"관측 구간 끝에서 보류 송신이 high-water 아래로 내려갔다 "
            f"(tx={final.get('tx')}) — 그사이 백프레셔가 풀렸다는 뜻이라, 이 "
            "구간의 만기는 멈춘 연결의 만기가 아니다:\n" + stats.dump())
    finally:
        _shutdown(proc, (a, b))


def test_reactor_keeps_a_pair_whose_reader_only_keeps_up_slowly():
    """따라오지 못하는 상대 때문에 정상 송신자가 끊기면 안 된다.

    위 테스트의 B 는 한 바이트도 안 빼내는 고장 난 상대이고, 그런 상대는 pause
    상한(kMaxPauseDuration, 16초)에 걸려 회수되는 것이 맞다. 여기서 재는 것은 그
    반대쪽 — **느리지만 실제로 빼내는** 상대다. 그런 상대와 붙은 매치는 상한 시간을
    몇 번 지나도 살아 있어야 한다. 빼낼 때마다 보류 송신이 high-water 아래로 내려가
    백프레셔가 풀리고, 풀리는 순간 pause 시계가 처음부터 다시 시작되기 때문이다.
    이 계약이 깨지면 릴레이는 "상대가 느리다" 는 이유만으로 규정을 지킨 송신자를
    걷어간다.

    이 테스트는 원래 스레드 모델 시절의 test_backpressure_does_not_disconnect_the_sender
    였다. 그 형태는 판정을 블로킹 sendall 이 막히는지로 했고 — 그 값은 릴레이가 아니라
    양쪽 커널 버퍼가 정한다 — 배포 대상인 Linux 에서는 skip 으로 남아 한 번도 돌지
    않았다. 판정을 릴레이가 스스로 적는 줄로 옮기고 송신을 논블로킹 버퍼 송신(_Feeder)
    으로 바꾸면 같은 계약이 Linux 에서 그대로 돈다.

    관측 창은 pause 상한(16초)보다 길게 잡는다. 유휴 만기(--idle-timeout-sec 3)만
    넘겨서는 부족하다 — 만기 갈래는 위 테스트가 이미 못 박았고, 상한 갈래는 16초를
    지나야만 판정에 들어오기 때문이다. 실측(2026-08-23 Linux): 이 배수 페이스에서 한
    pause 는 4~8초에 풀렸고, 관측 창 안의 close 는 0건이었다.

    reactor 전용이다. 스레드 모델에는 "읽기를 멈춘다" 는 기제가 없다 — 방향별 스레드가
    tcp_send_all 안에서 최대 5초 잠들며 버티다 실패하면 매치를 접는다.
    """
    reactor_bin = _require_reactor_step()

    # 관측 창. pause 상한(16초)을 확실히 넘겨야 상한 갈래가 판정에 들어온다.
    WATCH_SEC = 18.5
    # A 의 페이스. 상한(64 KiB/s)의 75% 다 — 루프 지터로 어느 1초 창이 부풀어도
    # 상한을 넘지 않을 만큼 낮다. 상한 아래로 보내는 송신자가 끊기면 그 자체가 결함이다.
    SEND_RATE = 48 * 1024
    # B 의 배수 페이스. A 보다 확실히 느려야 백프레셔가 관측 창 내내 유지되고,
    # 그러면서도 pause 를 상한(16초)보다 훨씬 빨리 푸는 크기여야 한다.
    BITE = 24 * 1024
    BITE_EVERY = 1.0

    proc, port, stats = _spawn_relay_with_stats(
        lambda p: [str(reactor_bin), "--port", str(p), "--log-level", "info",
                   "--stats-interval-sec", "1",
                   "--idle-timeout-sec", str(_BP_IDLE_SEC)])
    a = b = None
    try:
        when = time.monotonic()
        a, b, conn_a, conn_b = _forwarding_pair(stats, port)
        a.setblocking(False)
        b.setblocking(False)
        sender, slow = _Feeder(a), _Feeder(b)
        keepalive = _keepalive_frame()
        last_keepalive = [0.0]

        def _tick_slow_side() -> None:
            # B 도 자기 유휴 데드라인은 살려 둔다. 안 그러면 B 가 먼저 걷히고 A 가
            # "상대 이탈" 로 따라 죽어, 재려던 조건에 닿지 못한다.
            now = time.monotonic()
            if now - last_keepalive[0] >= _BP_IDLE_SEC / 4.0:
                last_keepalive[0] = now
                slow.offer(keepalive)
            slow.pump()

        # 1단계 — 전속력으로 밀어 백프레셔를 세운다. 총량은 버스트 허용치 아래로
        # 묶는다(_BP_FLOOD_CAP) — 여기서 레이트로 끊기면 재려던 것을 못 잰다.
        established = False
        started = time.monotonic()
        while time.monotonic() - started < 25.0:
            while (sender.backlog() < 4 * len(_BP_CHUNK)
                   and sender.offered_total() < _BP_FLOOD_CAP):
                sender.offer(_BP_CHUNK)
            sender.pump()
            _tick_slow_side()
            _drain_nonblocking(a)
            if stats.latest().get("tx_peak", 0) > _RELAY_SEND_HIGH_WATER:
                established = True
                break
            time.sleep(0.01)
        assert established, (
            f"{time.monotonic() - started:.1f}초를 밀었는데 보류 송신이 high-water"
            f"({_RELAY_SEND_HIGH_WATER}) 를 넘지 않았다 "
            f"(tx_peak={stats.latest().get('tx_peak', 0)}) — 릴레이가 A(conn "
            f"{conn_a})의 읽기를 멈춘 적이 없으므로 이 테스트는 아무것도 재고 있지 "
            "않다:\n" + stats.dump())

        # 2단계 — A 는 상한 아래 페이스로 계속 보내고, B 는 그보다 느리게 빼낸다.
        # 백프레셔는 관측 창 내내 걸렸다 풀렸다 하고, 그동안 아무도 끊기면 안 된다.
        watch_started = time.monotonic()
        floor = sender.offered_total()
        drained = 0
        last_bite = watch_started
        above_hw = 0
        while time.monotonic() - watch_started < WATCH_SEC:
            _pace(sender, _BP_CHUNK, SEND_RATE, watch_started, floor=floor)
            _tick_slow_side()
            _drain_nonblocking(a)
            now = time.monotonic()
            if now - last_bite >= BITE_EVERY:
                last_bite = now
                got = 0
                while got < BITE:
                    try:
                        chunk = b.recv(min(65536, BITE - got))
                    except (BlockingIOError, OSError):
                        break
                    if not chunk:
                        break
                    got += len(chunk)
                drained += got
            if stats.closes_after(when, (conn_a, conn_b)):
                break
            time.sleep(0.01)
        watched_s = time.monotonic() - watch_started
        samples = stats.samples_after(watch_started)
        above_hw = sum(1 for s in samples
                       if s.get("tx", 0) > _RELAY_SEND_HIGH_WATER)

        closes = stats.closes_after(when, (conn_a, conn_b))
        assert not closes, (
            f"느리게 빼내는 상대와 붙은 매치가 {watched_s:.1f}초 만에 끊겼다: "
            f"{closes} (A=conn {conn_a}, B=conn {conn_b}). 상한 사유면 pause 시계가 "
            "배수 때마다 다시 시작되지 않는다는 뜻이고, 'idle 타임아웃' 이면 백프레셔로 "
            "멈춰 세운 송신자를 유휴로 오인한 것이다:\n" + stats.dump())
        assert watched_s > _RELAY_MAX_PAUSE_SEC, (
            f"관측 창이 {watched_s:.1f}초뿐이라 pause 상한"
            f"({_RELAY_MAX_PAUSE_SEC}초)을 넘지 못했다 — 상한 갈래를 하나도 재지 "
            "않았다:\n" + stats.dump())
        assert drained > 4 * _RELAY_SEND_HIGH_WATER, (
            f"B 가 받은 양이 {drained} 바이트뿐이다 — 이 상대는 '느리게 빼내는' 이 "
            "아니라 '안 빼내는' 이라, 재려던 계약이 아니라 회수 계약을 재고 있다:\n"
            + stats.dump())
        assert above_hw >= len(samples) // 2 and above_hw >= 3, (
            f"관측 창 {len(samples)}개 표본 중 보류 송신이 high-water 위였던 것이 "
            f"{above_hw}개뿐이다 (tx={[s.get('tx') for s in samples]}) — 백프레셔가 "
            "걸려 있지 않았다면 이 구간은 평범한 포워딩이고, 그러면 pause 상한도 "
            "재무장 갈래도 지나지 않는다:\n" + stats.dump())
    finally:
        _shutdown(proc, (a, b))
