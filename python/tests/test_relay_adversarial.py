"""릴레이 적대적 입력 스위트 — 사용자 행동과 어뷰징만으로 나는 장애를 겨눈다.

이 파일이 다루는 것은 "잘못된 코드" 가 아니라 **잘못된 사용자** 다. 공개 배포된
멀티플레이 서버에는 프로토콜을 지키지 않는 클라이언트, 아무 때나 끊는 회선,
남을 굶기려는 사람이 반드시 온다. 그래서 모든 케이스가 두 가지를 함께 묻는다:

  1. 릴레이 프로세스가 살아남는가
  2. **다른 사용자가 그 대가를 치르지 않는가**

두 번째가 이 파일의 존재 이유다. 한 연결이 죽는 것은 정상 실패지만, 한 사람의
행동 때문에 무관한 사람이 굶거나 끊기거나 기다리게 되면 그것은 결함이다.

바이너리가 둘이다 — 스레드 모델(``tetris_relay``)과 이벤트 루프
(``tetris_relay_reactor``, 배포 대상). ``TETRIS_RELAY_BIN`` 이 어느 쪽을 가리키든
같은 파일이 그대로 돈다. 여기 담긴 계약은 전부 두 모델에 공통이라 바이너리별
스킵 가드가 하나도 없다 — 구현이 갈리는 지점은 스킵이 아니라 **아래 xfail 로**
드러난다. 한쪽만 못 지키는 계약은 "재지 않을 것" 이 아니라 "고칠 것" 이기 때문이다.
(계약 자체가 다른 항목, 예컨대 reactor 전용 인자나 백프레셔 기제 자체를 겨누는
테스트는 기존 ``test_relay_meta_smoke.py`` 가 ``_find_bin`` 이름으로 가드한다.)

**결함 표시 규약**: 지금 릴레이가 계약을 못 지키는 지점에서는
``pytest.xfail(...)`` 로 이유를 남긴다. 배포 파이프라인을 빨갛게 만들지 않으면서,
고쳐지는 순간 XPASS 로 드러나 이 표시를 지울 때가 됐음을 알려 준다. 각 테스트
docstring 에 무엇을 왜 재는지와, **가드**(계약을 못 박아 회귀를 막는 것)인지
**회귀 테스트**(고쳐진 결함이 되살아나는지 보는 것)인지를 밝힌다.

예외가 하나 있다. **릴레이가 죽는 것은 xfail 로 덮지 않는다** — ``relays``
픽스처가 매 테스트 끝에 생존을 확인하고, 죽었으면 그대로 실패시킨다. 프로세스가
사라지는 것은 "아직 못 지킨 계약" 이 아니라 서비스가 없어진 것이고, 그 상태에서
초록을 보고하는 스위트는 있으나 마나다. 지금 이 확인은 실제로 가끔 걸린다 —
자세한 것은 ``test_connect_and_close_churn_does_not_kill_the_relay`` 참고.

실행::

    uv run python -m pytest python/tests/test_relay_adversarial.py -v
    TETRIS_RELAY_BIN=build/Release/tetris_relay_reactor.exe uv run python -m pytest ...
"""

from __future__ import annotations

import json
import os
import signal
import socket
import struct
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import pytest

from netbot.framing import FramingError, MsgType, build_frame, parse_frames

# ── C++ 쪽과 맞춰야 하는 값 ──────────────────────────────────────────────────
# net/framing.h 의 net::kMaxPayloadBytes.
MAX_PAYLOAD_BYTES = 4096
# server/ip_admission.h 의 kMaxHandshakesPerIp.
RELAY_MAX_HANDSHAKES_PER_IP = 16
# 첫 프레임 대기 — 양 바이너리 공통 5초 (server/player_conn.cpp 의 kJoinTimeout,
# server/reactor_relay.cpp 의 kFirstFrameTimeout).
RELAY_FIRST_FRAME_TIMEOUT = 5.0

TEST_RELAY_SECRET = "test-relay-secret"


# ── 바이너리 찾기 (test_relay_meta_smoke.py 의 선례를 그대로 복사) ───────────
def _find_bin(name: str, env_var: str) -> Path | None:
    env = os.environ.get(env_var)
    if env:
        p = Path(env)
        return p if p.exists() else None
    repo = Path(__file__).resolve().parents[2]
    suffix = ".exe" if os.name == "nt" else ""
    base = name + suffix
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


_RELAY_BIN = _find_bin("tetris_relay", "TETRIS_RELAY_BIN")

pytestmark = pytest.mark.skipif(
    _RELAY_BIN is None, reason="tetris_relay binary missing")


# ── 소켓 유틸 ────────────────────────────────────────────────────────────────
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


def _wait_listen(port: int, timeout_s: float = 5.0,
                 proc: subprocess.Popen | None = None) -> bool:
    """포트가 열릴 때까지 기다린다.

    proc 를 주면 그 프로세스가 죽는 즉시 포기한다 — bind 에 실패해 이미 끝난
    프로세스를 타임아웃 끝까지 기다리는 것은 낭비다 (relay_shard_bench.py 의
    wait_listen 과 같은 이유).
    """
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return True
        except OSError:
            if proc is not None and proc.poll() is not None:
                return False
            time.sleep(0.05)
    return False


def _connect(port: int, source_ip: str | None = None,
             timeout: float = 3.0) -> socket.socket:
    """릴레이에 붙는다. source_ip 를 주면 그 주소에서 나가는 연결을 만든다.

    127.0.0.0/8 은 전부 loopback 이므로 출발지를 흩어 per-IP 버킷을 나눌 수 있다
    (python/tools/relay_shard_bench.py 에 같은 선례가 있다). per-IP 상한을
    우회하려는 것이 아니라, **상한이 IP 단위로 격리되는지** 를 재기 위해서다.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if source_ip:
        s.bind((source_ip, 0))
    s.settimeout(timeout)
    s.connect(("127.0.0.1", port))
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return s


def _abort(sock: socket.socket) -> None:
    """RST 로 끊는다 — 정상 종료(FIN)가 아니라 회선이 끊긴 것처럼 보인다."""
    try:
        fmt = "HH" if sys.platform == "win32" else "ii"
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack(fmt, 1, 0))
    except OSError:
        pass
    try:
        sock.close()
    except OSError:
        pass


def _collect(sock: socket.socket, buf: bytearray, seconds: float,
             stop_on: MsgType | None = None) -> tuple[list, bool]:
    """seconds 동안 읽어 (프레임 목록, 닫혔는가) 를 돌려준다.

    끊김을 예외가 아니라 반환값으로 준다 — 적대적 케이스에서는 "닫혔다" 가
    실패가 아니라 관측 대상이기 때문이다.
    """
    frames: list[tuple[MsgType, bytes]] = []
    deadline = time.monotonic() + seconds
    closed = False
    while time.monotonic() < deadline:
        if stop_on is not None and any(t == stop_on for t, _ in frames):
            break
        try:
            sock.settimeout(max(0.05, min(0.25, deadline - time.monotonic())))
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        except OSError:
            closed = True
            break
        if not chunk:
            closed = True
            break
        buf.extend(chunk)
        try:
            frames.extend(parse_frames(buf))
        except FramingError as exc:
            # 릴레이가 상한을 넘는 길이를 선언했다 — 그 자체가 관측 대상이다.
            frames.extend(exc.frames)
            break
    return frames, closed


def _recv_frame(sock: socket.socket, want: MsgType, buf: bytearray,
                timeout: float = 5.0) -> bytes:
    """want 타입이 올 때까지 읽는다. buf 는 호출자가 보관(한 read 에 여러 프레임)."""
    deadline = time.monotonic() + timeout
    while True:
        for t, p in parse_frames(buf):
            if t == want:
                return p
        if time.monotonic() >= deadline:
            raise TimeoutError(f"no {want!r} within deadline")
        sock.settimeout(max(0.05, deadline - time.monotonic()))
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            raise RuntimeError(f"relay closed before {want!r}")
        buf.extend(chunk)


def _try_recv_frame(sock: socket.socket, want: MsgType, buf: bytearray,
                    timeout: float) -> bytes | None:
    try:
        return _recv_frame(sock, want, buf, timeout)
    except (TimeoutError, RuntimeError, OSError):
        return None


def _wait_closed(sock: socket.socket, timeout: float) -> bool:
    _, closed = _collect(sock, bytearray(), timeout)
    return closed


# ── 프레임 빌더 ──────────────────────────────────────────────────────────────
def _queue_join(token: str = "") -> bytes:
    payload = b"\x00" if not token else bytes([len(token)]) + token.encode("ascii")
    return build_frame(MsgType.QUEUE_JOIN, payload)


def _room_create(token: str = "") -> bytes:
    payload = b"\x00" if not token else bytes([len(token)]) + token.encode("ascii")
    return build_frame(MsgType.ROOM_CREATE, payload)


def _room_join(code: bytes | str, token: str = "") -> bytes:
    raw = code.encode("ascii") if isinstance(code, str) else code
    tail = b"\x00" if not token else bytes([len(token)]) + token.encode("ascii")
    return build_frame(MsgType.ROOM_JOIN, bytes([len(raw)]) + raw + tail)


def _ready(v: int = 1) -> bytes:
    return build_frame(MsgType.READY, bytes([v]))


def _input_frame(tick: int = 0) -> bytes:
    return build_frame(MsgType.INPUT, struct.pack("<IH", tick, 1) + b"\x00")


def _summary(won: int, my_score: int, my_lines: int,
             opp_score: int, opp_lines: int, duration: int = 1) -> bytes:
    payload = bytes([won]) + struct.pack(
        "<IIIII", my_score, my_lines, opp_score, opp_lines, duration)
    assert len(payload) == 21
    return build_frame(MsgType.MATCH_SUMMARY, payload)


def _raw_frame(type_byte: int, payload: bytes, *,
               declared_len: int | None = None,
               checksum: int | None = None) -> bytes:
    """프레임 규칙을 일부러 어긴 바이트열을 만든다.

    build_frame 은 정상 프레임만 만들 수 있으므로(그게 그 함수의 계약이다),
    잘림/과대 길이/체크섬 위조는 여기서 손으로 짠다.
    """
    from netbot.framing import fnv1a32
    body = bytes([type_byte]) + payload
    length = len(body) if declared_len is None else declared_len
    chk = (fnv1a32(payload) if payload else 0) if checksum is None else checksum
    return struct.pack("<H", length & 0xFFFF) + body + struct.pack("<I", chk & 0xFFFFFFFF)


def _parse_room_info(payload: bytes) -> tuple[bytes, int, int]:
    n = payload[0]
    return payload[1:1 + n], payload[1 + n], payload[2 + n]


def _parse_match_found(payload: bytes) -> tuple[int, int]:
    return payload[0], struct.unpack_from("<Q", payload, 1)[0]


# ── 릴레이 기동 ──────────────────────────────────────────────────────────────
def _spawn_relay(log_path: Path, extra: list[str] | None = None, *,
                 new_group: bool = False) -> tuple[subprocess.Popen, int]:
    """릴레이를 빈 포트에 띄우고 로그를 파일로 받는다.

    PIPE 가 아니라 파일인 것이 중요하다. 릴레이는 연결마다 여러 줄을 찍는데
    PIPE 로 받아 놓고 아무도 읽지 않으면 파이프 버퍼가 차는 순간 릴레이가
    write 에서 멈춘다 — 연결을 수십 개 붙이는 이 스위트에서는 그게 "상한에
    걸렸다" 와 구분되지 않는 타임아웃으로 나타난다 (기존 스위트가 같은 함정을
    주석으로 남겨 뒀다). 파일에는 그 상한이 없다.

    그렇다고 DEVNULL 로 버리지도 않는다. 릴레이가 죽는 것은 이 스위트가 잡을 수
    있는 가장 심각한 실패인데, 마지막 로그가 없으면 "죽었다" 까지만 알고 왜인지는
    알 수 없다. 죽었을 때 그 꼬리를 실패 메시지에 붙이는 것이 이 파일의 목적이다.

    ``TETRIS_RELAY_EXTRA_ARGS`` 로 공통 인자를 덧붙일 수 있다. 이 스위트를
    ``--loops 4`` 같은 구성에도 그대로 겨누기 위한 것이다 — 포워딩 인계는 연결
    수명이 루프 사이를 건너가는 유일한 지점이라, 적대적 단절을 그 구성에서도
    밟아 봐야 한다. CI 가 스모크를 두 loops 값으로 돌리는 것과 같은 이유다.
    """
    shared = os.environ.get("TETRIS_RELAY_EXTRA_ARGS", "").split()
    # 빈 포트를 잡는 순간과 릴레이가 bind 하는 순간 사이의 틈은 없앨 수 없다.
    # 그 사이에 임시 포트 풀에서 같은 번호가 나가면 릴레이는 bind 에 실패하고
    # 즉시 죽는다 — 이 스위트는 연결을 수백 개 여닫으므로 그 풀을 활발히 쓴다.
    # relay_shard_bench.py 가 실제로 이 충돌을 겪고 재시도로 해결했다.
    last_port = 0
    for _ in range(5):
        port = _free_port()
        last_port = port
        with open(log_path, "ab") as log:
            proc = subprocess.Popen(
                [str(_RELAY_BIN), "--port", str(port)] + list(extra or []) + shared,
                stdout=log, stderr=subprocess.STDOUT,
                creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP
                               if (new_group and sys.platform == "win32") else 0),
            )
        if _wait_listen(port, 8.0, proc):
            return proc, port
        if proc.poll() is None:
            proc.kill()
        proc.wait(timeout=5)
    pytest.fail(f"relay failed to listen (last port {last_port}):\n"
                + _log_tail(log_path))


def _log_tail(path: Path, lines: int = 30) -> str:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return "(로그 없음)"
    return "\n".join(text.splitlines()[-lines:])


def _stop(proc: subprocess.Popen) -> None:
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)


@pytest.fixture
def relays(tmp_path):
    """릴레이를 띄우고, **테스트 도중 죽지 않았는지 확인한 뒤** 정리하는 팩토리.

    죽었는지 확인하는 것이 핵심이다. 릴레이가 중간에 죽으면 이 파일의 여러
    판정이 조용히 통과해 버린다 — "상대가 끊긴 걸 알려 줬다"(사실은 서버가 죽어
    EOF 가 온 것), "연결이 닫혔다"(사실은 서버가 사라진 것) 처럼, 관측 결과가
    합격 조건과 우연히 같은 모양이 되기 때문이다. 적대적 스위트에서 이건
    치명적이다: 제일 심각한 실패(크래시)가 제일 조용한 통과로 둔갑한다.

    죽었을 때는 마지막 로그를 함께 띄운다 — "죽었다" 만으로는 고칠 수 없다.

    일부러 종료시키는 테스트는 ``allow_exit(proc)`` 로 면제를 신청한다.
    """
    entries: list[tuple[subprocess.Popen, Path]] = []
    allowed: set[int] = set()

    def _make(extra: list[str] | None = None, *,
              new_group: bool = False) -> tuple[subprocess.Popen, int]:
        log_path = tmp_path / f"relay-{len(entries)}.log"
        proc, port = _spawn_relay(log_path, extra, new_group=new_group)
        entries.append((proc, log_path))
        return proc, port

    def _allow_exit(proc: subprocess.Popen) -> None:
        allowed.add(id(proc))

    _make.allow_exit = _allow_exit
    yield _make

    died = [(p, lp) for p, lp in entries
            if id(p) not in allowed and p.poll() is not None]
    for p, _ in entries:
        _stop(p)
    if died:
        report = "\n\n".join(
            f"종료 코드 {p.returncode}, 마지막 로그:\n{_log_tail(lp)}"
            for p, lp in died)
        pytest.fail(
            "릴레이가 테스트 도중 죽었다 — 위 판정 중 통과한 것들은 신뢰할 수 "
            "없다.\n\n"
            "종료 코드가 4294967295(0xFFFFFFFF)이고 마지막 로그에 'shutting down' "
            "이 없다면, 이건 테스트의 불안정이 아니라 알려진 릴레이 결함이다 — "
            "test_connect_and_close_churn_does_not_kill_the_relay 의 docstring 을 "
            "보라. 재현은 확률적이라 어느 테스트에서 터지는지는 매번 다르다.\n\n"
            + report)


@pytest.fixture
def unranked(relays):
    """meta 없는 릴레이 하나 — tok_len=0 경로. 대부분의 케이스는 인증이 필요 없다."""
    proc, port = relays()
    return proc, port


@pytest.fixture
def socks():
    """테스트가 연 소켓을 끝에서 반드시 닫아 준다 (상한 누수 방지)."""
    opened: list[socket.socket] = []

    def _track(s: socket.socket) -> socket.socket:
        opened.append(s)
        return s

    yield _track
    for s in opened:
        try:
            s.close()
        except OSError:
            pass


# ── 가짜 meta ────────────────────────────────────────────────────────────────
class _FakeMeta:
    """릴레이가 보는 meta 를 흉내 내는 최소 HTTP 서버.

    진짜 ``tetris_meta`` 대신 쓰는 이유는 셋 다 테스트 대상 때문이다:
      · 응답을 **느리게/깨지게/멈추게** 만들 수 있다. 진짜 meta 로는 만들 수 없는
        조건이고, 배포 대상에서 meta 는 성능이 매우 제한된 보조 기기에서 도므로
        느린 meta 는 가정이 아니라 기본값에 가깝다.
      · 토큰을 마음대로 발급할 수 있어 공개 레이트 상한(60 req/s)에 안 걸린다.
      · 프로세스를 하나 덜 띄운다.

    응답 모양이 릴레이가 실제로 받아들이는 것과 같은지는
    ``test_fake_meta_is_accepted_by_the_relay`` 가 지킨다 — 이 가짜가 어긋나면
    랭크드 케이스가 전부 "그냥 거절" 로 변해 조용히 무의미해지므로, 자체 점검이
    반드시 필요하다. (진짜 meta 와의 종단 간 계약은 기존
    ``test_relay_meta_smoke.py`` 가 계속 지킨다 — 여기서 대체하지 않는다.)
    """

    def __init__(self) -> None:
        self.delay = 0.0
        self.mode = "ok"          # ok | unknown | garbage | error500
        self._tokens: dict[str, int] = {}
        self._next_id = 1000
        self._lock = threading.Lock()
        self.verify_count = 0
        self.match_count = 0
        self.last_winner_null: bool | None = None

        outer = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *args):  # noqa: D102 - 조용히
                pass

            def _reply(self, status: int, body: str) -> None:
                raw = body.encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(raw)))
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(raw)

            def do_POST(self):  # noqa: N802 - BaseHTTPRequestHandler 규약
                n = int(self.headers.get("Content-Length", "0") or 0)
                body = self.rfile.read(n).decode("utf-8", "replace")
                if self.path == "/v1/auth/verify":
                    outer._on_verify(self, body)
                elif self.path == "/v1/matches":
                    outer._on_match(self, body)
                else:
                    self._reply(404, '{"error":"not_found"}')

        class QuietServer(ThreadingHTTPServer):
            # 릴레이는 인증 왕복 도중 사라질 수 있다(그게 여기서 재는 것이다).
            # 그때 나는 broken pipe 스택 트레이스를 stderr 로 쏟으면 진짜 실패
            # 메시지가 그 아래 묻힌다. 이 서버의 오류는 관측 대상이 아니다.
            def handle_error(self, request, client_address):
                pass

            # socketserver 기본 백로그는 5다. 이 스위트는 인증 왕복을 일부러 수백
            # 개씩 동시에 던지는데(스레드 모델은 워커가 256개라 그대로 다 나간다),
            # 그러면 accept 큐가 넘쳐 커널이 SYN 을 떨군다. 릴레이는 그것을
            # "meta 네트워크 오류" 로 보고 그 사용자를 거절하므로, 테스트가 재려던
            # "뒤에 줄 선 정상 사용자가 굶는가" 대신 "가짜 meta 가 접속을 못 받는가"
            # 를 재게 된다 — 실측 실패율 약 6~9%의 정체가 이것이다. 백로그를
            # 넉넉히 잡아 병목을 이 서버 밖으로 치운다.
            request_queue_size = 512

        self._server = QuietServer(("127.0.0.1", 0), Handler)
        self._server.daemon_threads = True
        self.port = self._server.server_address[1]
        self._thread = threading.Thread(target=self._server.serve_forever,
                                        kwargs={"poll_interval": 0.05}, daemon=True)
        self._thread.start()

    # -- 서버 쪽 핸들러 ------------------------------------------------------
    def _on_verify(self, handler, body: str) -> None:
        with self._lock:
            self.verify_count += 1
        if self.delay:
            time.sleep(self.delay)
        if self.mode == "error500":
            handler._reply(500, '{"error":"boom"}')
            return
        if self.mode == "garbage":
            handler._reply(200, "<html>this is not the json you are looking for")
            return
        token = ""
        marker = '"token":"'
        if marker in body:
            rest = body.split(marker, 1)[1]
            token = rest.split('"', 1)[0]
        pid = self._tokens.get(token)
        if self.mode == "unknown" or pid is None:
            handler._reply(404, '{"error":"unknown_token"}')
            return
        handler._reply(200, json.dumps({
            "player_id": pid, "username": f"p{pid}", "elo": 1000,
            "bp": 0, "xp": 0, "level": 1, "selected_icon_id": "default",
        }, separators=(",", ":")))

    def _on_match(self, handler, body: str) -> None:
        # 승자가 있는 결과에만 0 이 아닌 delta 를 돌려준다. 항상 0 을 주면
        # "결과가 반영되지 않았다" 를 재는 테스트가 어느 쪽이든 통과해 버려
        # 조용히 무의미해진다 — delta 가 신호 역할을 하게 만든다.
        winner_null = '"winner":null' in body.replace(" ", "")
        delta = 0 if winner_null else 20
        with self._lock:
            self.match_count += 1
            mid = self.match_count
            self.last_winner_null = winner_null
        # meta/http_client.cpp 의 find_sub 는 `"a":{` 를 리터럴로 찾는다 —
        # 콜론 뒤 공백이 있으면 파싱이 실패하므로 separators 를 붙여 둔다.
        handler._reply(200, json.dumps({
            "match_id": mid,
            "a": {"elo_before": 1000, "elo_after": 1000 + delta, "delta": delta},
            "b": {"elo_before": 1000, "elo_after": 1000 - delta, "delta": -delta},
        }, separators=(",", ":")))

    # -- 테스트 쪽 API -------------------------------------------------------
    def issue(self) -> str:
        with self._lock:
            self._next_id += 1
            pid = self._next_id
        token = "%032x" % pid
        self._tokens[token] = pid
        return token

    @property
    def url(self) -> str:
        return f"http://127.0.0.1:{self.port}"

    def close(self) -> None:
        self._server.shutdown()
        self._server.server_close()


@pytest.fixture
def fake_meta():
    meta = _FakeMeta()
    yield meta
    meta.close()


@pytest.fixture
def ranked(relays, fake_meta):
    """가짜 meta 를 붙인 랭크드 릴레이. (meta, port) 를 준다."""
    _, port = relays(["--meta", fake_meta.url, "--meta-secret", TEST_RELAY_SECRET])
    return fake_meta, port


# ── 공용 시나리오 조각 ───────────────────────────────────────────────────────
def _pair_unranked(port: int, track, *, ready: bool = False,
                   source_ips: tuple[str, str] = ("127.0.0.1", "127.0.0.1")):
    """큐 경로로 두 연결을 붙여 매치를 만든다. ready=True 면 포워딩까지 밀어 올린다."""
    a = track(_connect(port, source_ips[0]))
    b = track(_connect(port, source_ips[1]))
    a_buf, b_buf = bytearray(), bytearray()
    a.sendall(_queue_join())
    b.sendall(_queue_join())
    _recv_frame(a, MsgType.MATCH_FOUND, a_buf, 5.0)
    _recv_frame(b, MsgType.MATCH_FOUND, b_buf, 5.0)
    if ready:
        a.sendall(_ready(1))
        b.sendall(_ready(1))
        # 양쪽 READY 가 관측돼 포워딩이 시작될 때까지, 실제 왕복으로 확인한다.
        _handshake_forwarding(a, b, a_buf, b_buf)
    return a, b, a_buf, b_buf


def _handshake_forwarding(a, b, a_buf, b_buf, timeout: float = 5.0) -> None:
    """포워딩이 실제로 시작됐음을 왕복으로 확인한다 (sleep 대신 조건 대기).

    타이밍 가정("READY 뒤 0.2초면 되겠지")은 부하가 걸린 CI 에서 제일 먼저
    깨진다. A 가 보낸 프레임이 B 에 도착하는 것을 보는 편이 값도 싸고 확실하다.
    """
    deadline = time.monotonic() + timeout
    probe = build_frame(MsgType.CHAT, b"go")
    while time.monotonic() < deadline:
        a.sendall(probe)
        frames, closed = _collect(b, b_buf, 0.3, stop_on=MsgType.CHAT)
        if closed:
            raise RuntimeError("relay closed while waiting for forwarding")
        if any(t == MsgType.CHAT for t, _ in frames):
            return
        time.sleep(0.05)
    raise TimeoutError("forwarding never started")


def _relay_still_serves(port: int, track, timeout: float = 6.0,
                        source_ips: tuple[str, str] = ("127.0.9.1", "127.0.9.2")) -> None:
    """무관한 사용자 두 명이 여전히 정상적으로 매칭되는지 확인한다.

    이 파일의 거의 모든 테스트가 끝에 이걸 부른다 — "릴레이가 안 죽었다" 로는
    부족하고, **다른 사람이 계속 놀 수 있는가** 가 실제로 지켜야 할 계약이다.
    앞선 어뷰징이 per-IP 상한을 건드렸을 수 있으므로 기본 출발지를 따로 둔다.
    """
    a = track(_connect(port, source_ips[0]))
    b = track(_connect(port, source_ips[1]))
    a_buf, b_buf = bytearray(), bytearray()
    a.sendall(_queue_join())
    b.sendall(_queue_join())
    role_a, seed_a = _parse_match_found(
        _recv_frame(a, MsgType.MATCH_FOUND, a_buf, timeout))
    role_b, seed_b = _parse_match_found(
        _recv_frame(b, MsgType.MATCH_FOUND, b_buf, timeout))
    assert seed_a == seed_b
    assert {role_a, role_b} == {1, 2}
    a.close()
    b.close()


# ═══════════════════════════════════════════════════════════════════════════
# 0. 하니스 자체 점검
# ═══════════════════════════════════════════════════════════════════════════
def test_fake_meta_is_accepted_by_the_relay(ranked, socks):
    """가짜 meta 로 띄운 릴레이에서 랭크드 매칭이 실제로 성립하는가. **가드**.

    아래 랭크드 케이스들은 전부 이 가짜 meta 위에서 돈다. 응답 모양이 조금이라도
    어긋나면 릴레이는 모든 토큰을 거절하고, 그러면 "공격이 막혔다" 와 "아무도
    못 들어왔다" 가 구분되지 않는다 — 테스트가 조용히 무의미해지는 가장 흔한
    방식이다. 그 구분을 이 테스트 하나가 책임진다.
    """
    meta, port = ranked
    a = socks(_connect(port))
    b = socks(_connect(port))
    a_buf, b_buf = bytearray(), bytearray()
    a.sendall(_queue_join(meta.issue()))
    b.sendall(_queue_join(meta.issue()))
    role_a, seed_a = _parse_match_found(
        _recv_frame(a, MsgType.MATCH_FOUND, a_buf, 6.0))
    role_b, seed_b = _parse_match_found(
        _recv_frame(b, MsgType.MATCH_FOUND, b_buf, 6.0))
    assert seed_a == seed_b
    assert {role_a, role_b} == {1, 2}
    assert meta.verify_count >= 2


# ═══════════════════════════════════════════════════════════════════════════
# 1. 연결 수명 — 아무 단계에서나 갑자기 끊긴다
# ═══════════════════════════════════════════════════════════════════════════
def test_abrupt_reset_at_every_stage_leaves_the_relay_serving(unranked, socks):
    """모든 단계에서 RST 로 끊어도 릴레이가 계속 서비스하는가. **가드**.

    각 단계(첫 프레임 대기 / 큐 대기 / 룸 대기 / 로비 / 포워딩)마다 연결을 만들어
    그 단계에 세운 뒤 SO_LINGER=0 으로 RST 를 던진다. FIN 이 아니라 RST 인 이유는,
    정상 종료 경로만 밟으면 "회선이 끊겼다" 를 못 재기 때문이다 — 실제 모바일
    회선에서 흔한 쪽은 이쪽이다.

    다섯 단계를 모두 밟은 뒤, 무관한 두 사람이 여전히 매칭되는지 확인한다.
    릴레이가 안 죽었다는 것만으로는 부족하다. (단계마다 확인하지 않는 이유는
    시간이다 — 어느 단계가 범인인지는 실패했을 때 이 목록을 반으로 갈라 보면
    금방 나오고, 그 드문 경우를 위해 매번 다섯 번씩 매칭을 돌릴 이유는 없다.)
    """
    _, port = unranked

    # (1) 첫 프레임 대기 중 — 아무것도 안 보내고 끊는다.
    _abort(socks(_connect(port)))

    # (2) 큐 대기 중 — 상대가 없어 큐에 남아 있는 상태에서 끊는다.
    q = socks(_connect(port))
    q.sendall(_queue_join())
    time.sleep(0.05)
    _abort(q)

    # (3) 룸 대기 중 — ROOM_INFO 를 받아 방을 연 뒤 끊는다.
    r = socks(_connect(port))
    r_buf = bytearray()
    r.sendall(_room_create())
    _recv_frame(r, MsgType.ROOM_INFO, r_buf, 5.0)
    _abort(r)

    # (4) 로비 — MATCH_FOUND 를 받고 READY 전에 끊는다.
    la, lb, _, _ = _pair_unranked(port, socks)
    _abort(la)
    _abort(lb)

    # (5) 포워딩 중 — 실제로 프레임이 흐르는 상태에서 끊는다.
    fa, fb, _, _ = _pair_unranked(port, socks, ready=True)
    fa.sendall(_input_frame())
    _abort(fa)
    _abort(fb)

    _relay_still_serves(port, socks)


def test_both_sides_reset_simultaneously_during_forwarding(unranked, socks):
    """양쪽이 같은 순간에 끊겨도 채널 정리가 꼬이지 않는가. **가드**.

    한쪽만 끊기는 경우와 달리, 동시 단절은 "상대를 잃은 쪽" 이 양쪽 다여서 채널
    정리와 결과 통지 경로가 서로를 밟을 수 있다. 스레드 모델에는 실제로 이
    타이밍에 교차검증이 생략되던 경합이 있었고(server/relay.cpp 주석 참고),
    루프 모델은 그 경합을 구조적으로 없앴다고 주장한다 — 주장에 테스트를 붙인다.

    쌍을 여러 번 만들어 동시에 끊는다. 한 번으로는 타이밍을 못 맞출 수 있다.
    """
    _, port = unranked
    for _ in range(5):
        a, b, _, _ = _pair_unranked(port, socks, ready=True)
        a.sendall(_input_frame())
        b.sendall(_input_frame())
        _abort(a)
        _abort(b)
    _relay_still_serves(port, socks)


def test_write_half_close_does_not_leave_a_zombie_session(unranked, socks):
    """쓰기만 shutdown 한 half-close 가 세션을 좀비로 남기지 않는가. **가드**.

    클라이언트가 ``shutdown(SHUT_WR)`` 로 쓰기만 닫고 읽기는 유지하는 경우다.
    릴레이는 half-close 를 지원하지 않고 EOF 를 연결 종료로 본다 — 이 테스트는
    그 선택을 못 박는다. 지원하지 않는 것 자체는 문제가 아니지만, **읽기가 열려
    있다는 이유로 소켓이 계속 남아 자원(세션 슬롯·fd)을 붙들면** 문제다.

    포워딩 중에 half-close 하고, 그 연결이 실제로 닫히는지(내 쪽 recv 가 EOF 를
    보는지) 확인한 뒤 무관한 사용자가 계속 매칭되는지 본다.
    """
    _, port = unranked
    a, b, _, b_buf = _pair_unranked(port, socks, ready=True)
    a.shutdown(socket.SHUT_WR)
    assert _wait_closed(a, 6.0), (
        "half-close 한 연결이 닫히지도 않고 아무 응답도 없다 — 릴레이가 EOF 를 "
        "관측하지 못하고 소켓이 좀비로 남았다는 뜻")
    _relay_still_serves(port, socks)


def test_silent_and_trickling_connections_cannot_outlive_the_handshake_window(
        unranked, socks):
    """slow-loris 두 변형이 첫 프레임 창(5초)을 넘기지 못하는가. **가드**.

    두 가지를 함께 잰다:
      · 접속만 하고 아무것도 안 보내는 연결
      · 유효한 QUEUE_JOIN 을 1바이트씩 아주 느리게 흘리는 연결 — 활동이 있으니
        "유휴가 아니다" 로 판정되면 창이 무한히 연장된다. 첫 프레임 창은 **활동이
        아니라 accept 시각** 기준이어야 한다.

    타임아웃을 기다려야만 잴 수 있으므로 이 테스트는 느리다(약 7초). 그래서 두
    변형을 한 테스트에서 동시에 돌린다 — 대기 시간은 한 번만 치른다.
    """
    _, port = unranked
    silent = socks(_connect(port, "127.0.3.1"))
    trickle = socks(_connect(port, "127.0.3.2"))

    # QUEUE_JOIN 한 프레임은 8바이트다. 바이트마다 0.9초를 쉬면 다 보내는 데 7초가
    # 넘어 첫 프레임 창(5초)을 확실히 지나간다 — 간격이 짧아 프레임이 창 안에
    # 완성되면 이 테스트는 slow-loris 가 아니라 정상 접속을 재게 된다.
    frame = _queue_join()
    interval = 0.9
    index = 0
    start = time.monotonic()
    trickle_closed = False
    while time.monotonic() - start < RELAY_FIRST_FRAME_TIMEOUT + 4.0:
        if index < len(frame):
            try:
                trickle.sendall(frame[index:index + 1])
                index += 1
            except OSError:
                trickle_closed = True
                break
        try:
            trickle.settimeout(interval)
            if trickle.recv(4096) == b"":
                trickle_closed = True
                break
        except socket.timeout:
            continue
        except OSError:
            trickle_closed = True
            break
    trickle_elapsed = time.monotonic() - start

    assert index < len(frame), (
        "프레임을 창 안에 다 보내 버렸다 — 이 테스트가 slow-loris 를 재지 못했다 "
        "(간격을 늘려야 한다)")
    assert trickle_closed, (
        f"1바이트씩 흘리는 연결이 {trickle_elapsed:.1f}초를 살아남았다 — 첫 프레임 "
        "창이 활동 기준으로 연장되고 있다. 그러면 slow-loris 가 상한을 그대로 통과한다")

    assert _wait_closed(silent, 3.0), (
        "아무것도 안 보낸 연결이 첫 프레임 창을 넘겨 살아남았다 — 접속만으로 "
        "슬롯을 무한 점유할 수 있다는 뜻")
    _relay_still_serves(port, socks)


def test_handshake_flood_from_one_address_does_not_starve_another(unranked, socks):
    """한 주소의 핸드셰이크 독식이 다른 주소를 굶기지 않는가. **가드**.

    per-IP 핸드셰이크 예산(16)을 한 주소에서 조용한 연결로 가득 채운다. 그 주소는
    자기 예산을 다 썼으니 더 못 들어오는 게 맞다. 하지만 **다른 주소의 사용자**는
    아무 영향도 받지 않아야 한다 — 그러지 않으면 접속만 하고 가만히 있는 사람
    하나가 서버 전체의 입구를 잠그는 셈이 된다.
    """
    _, port = unranked
    hogged = "127.0.4.1"
    for _ in range(RELAY_MAX_HANDSHAKES_PER_IP):
        socks(_connect(port, hogged))
    # 예산을 넘긴 같은 주소의 연결은 거절돼야 한다(상한이 실제로 존재하는가).
    extra = socks(_connect(port, hogged))
    extra.sendall(_queue_join())
    assert _wait_closed(extra, 3.0), (
        "핸드셰이크 예산을 넘긴 같은 주소의 연결이 살아남았다 — 상한이 없다")
    # 그리고 다른 주소는 멀쩡해야 한다.
    _relay_still_serves(port, socks, source_ips=("127.0.4.2", "127.0.4.3"))


def test_mid_auth_disconnect_leaves_the_relay_serving(ranked, socks):
    """meta 왕복 도중에 끊어도 릴레이가 흔들리지 않는가. **가드**.

    인증은 HTTP 왕복이라 릴레이가 연결 상태를 들고 기다리는 유일한 구간이다.
    그 사이에 연결이 죽으면 재개 시점에 가리키는 곳이 이미 없다 — use-after-free
    가 가장 나기 쉬운 자리이고, 밖에서 볼 수 있는 증상은 "그 다음 사람부터
    이상해진다" 뿐이다. meta 를 느리게 만들어 창을 넓힌 뒤 그 안에서 끊는다.
    """
    meta, port = ranked
    meta.delay = 0.3
    # 출발지를 흩는다 — 스레드 모델은 인증이 끝날 때까지 핸드셰이크 슬롯을 붙들고
    # 있어서, 한 주소에서 몰아 치면 재려던 것(인증 중 단절)이 아니라 per-IP 상한을
    # 재게 된다.
    for i in range(10):
        s = socks(_connect(port, f"127.0.15.{i + 1}"))
        s.sendall(_queue_join(meta.issue()))
        time.sleep(0.02)      # 왕복이 시작될 만큼만 기다렸다가
        _abort(s)             # 재개 전에 사라진다
    meta.delay = 0.0

    a = socks(_connect(port))
    b = socks(_connect(port))
    a_buf, b_buf = bytearray(), bytearray()
    a.sendall(_queue_join(meta.issue()))
    b.sendall(_queue_join(meta.issue()))
    assert _try_recv_frame(a, MsgType.MATCH_FOUND, a_buf, 8.0) is not None
    assert _try_recv_frame(b, MsgType.MATCH_FOUND, b_buf, 8.0) is not None


def test_lobby_peer_disconnect_is_reported_to_the_survivor(unranked, socks):
    """로비에서 상대가 사라지면 남은 사람에게 알려야 한다. **결함 노출**.

    큐가 짝을 지어 MATCH_FOUND 를 보낸 뒤, 양쪽 READY 를 기다리는 수락 로비
    단계다. 여기서 한쪽이 그냥 끊기면 남은 사람은 상대가 아직 있는 줄 알고
    기다린다. 릴레이가 즉시 알려 주지 않으면 남은 사람은 로비 타임아웃(30초)이
    다 될 때까지 "상대 수락 대기" 화면에 갇혔다가 조용히 끊긴다.

    이건 단순한 UX 문제가 아니라 값싼 괴롭힘 수단이다. 공격자는 큐에 들어가
    MATCH_FOUND 만 받고 끊으면 되고, 그 비용은 TCP 연결 하나다. 그 대가로 정상
    사용자 한 명의 30초를 태운다 — 반복하면 큐가 사실상 마비된다.

    스레드 모델은 로비 스레드가 양쪽 소켓을 모두 폴링해 EOF 를 보는 즉시 상대에게
    READY(0) 을 보내고 두 소켓을 닫는다. 루프 모델은 unranked 채널에서 상대 상실
    경로가 곧바로 반환해 아무것도 보내지 않고, 남은 쪽은 로비 타이머만 남는다.
    """
    _, port = unranked
    a, b, _, b_buf = _pair_unranked(port, socks)
    _abort(a)

    # 스레드 모델은 밀리초 단위로 알려 준다 — 부재를 증명하는 데 2초면 넉넉하다.
    frames, closed = _collect(b, b_buf, 2.0)
    told = closed or bool(frames)
    if not told:
        pytest.xfail(
            "결함: 로비에서 상대가 끊겼는데 남은 연결에 아무 통지도, 종료도 "
            "없다. 남은 사람은 로비 타임아웃(30초)까지 기다린다 — 공격자는 "
            "연결 하나로 정상 사용자의 30초를 태울 수 있다")
    assert told
    _relay_still_serves(port, socks)


def test_forwarding_peer_disconnect_is_reported_to_a_quiet_survivor(unranked, socks):
    """포워딩 중 상대가 끊겼을 때, 조용히 있던 쪽도 알게 되는가. **결함 노출**.

    보내는 쪽은 다음 프레임을 밀 때 실패로 알아차린다. 문제는 그 순간 받기만 하고
    있던 쪽이다 — 관전하듯 조용한 연결은 스스로 아무것도 안 보내므로, 릴레이가
    알려 주지 않으면 유휴 타임아웃까지 상대가 아직 있다고 믿는다.

    스레드 모델은 한 방향 포워더가 죽으면 반대 방향도 함께 접고 두 소켓을 닫는다.
    루프 모델은 unranked 채널에서 상대 상실을 통지하지 않아, 조용한 쪽은 자기가
    무언가 보내려다 실패할 때까지 아무것도 모른다.
    """
    _, port = unranked
    a, b, _, b_buf = _pair_unranked(port, socks, ready=True)
    _abort(a)

    frames, closed = _collect(b, b_buf, 2.0)   # B 는 이 동안 한 바이트도 안 보낸다
    told = closed or bool(frames)
    if not told:
        pytest.xfail(
            "결함: 포워딩 중 상대가 끊겼는데 조용히 받기만 하던 쪽에는 통지도 "
            "종료도 없다. 그쪽은 유휴 타임아웃(15초)까지 끝난 경기를 붙들고 있다")
    assert told
    _relay_still_serves(port, socks)


# ═══════════════════════════════════════════════════════════════════════════
# 2. 프로토콜 악용
# ═══════════════════════════════════════════════════════════════════════════
def test_out_of_stage_control_frames_do_not_break_the_session(unranked, socks):
    """단계에 맞지 않는 메시지가 세션을 망가뜨리지 않는가. **가드**.

    큐 대기 중 INPUT, 룸 대기 중 QUEUE_JOIN, 포워딩 중 ROOM_CREATE/QUEUE_JOIN 처럼
    현재 단계가 소비할 수 없는 프레임을 넣는다. 릴레이는 이런 프레임을 이해하지
    못하는 게 아니라 **지금 이해하면 안 되는** 상태다. 무시하고 계속 가야 하고,
    무엇보다 그 때문에 상대의 경기가 깨지면 안 된다.
    """
    _, port = unranked

    # 큐 대기 중 INPUT — 큐 단계는 QUEUE_CANCEL 외에는 소비하지 않아야 한다.
    a = socks(_connect(port))
    a.sendall(_queue_join() + _input_frame(1) + _input_frame(2))
    b = socks(_connect(port))
    b.sendall(_queue_join())
    a_buf, b_buf = bytearray(), bytearray()
    assert _try_recv_frame(a, MsgType.MATCH_FOUND, a_buf, 6.0) is not None, (
        "큐 대기 중 보낸 INPUT 때문에 매칭이 실패했다")
    assert _try_recv_frame(b, MsgType.MATCH_FOUND, b_buf, 6.0) is not None

    # 룸 대기 중 QUEUE_JOIN — 방에 있는 사람이 큐 명령을 보내도 방이 깨지면 안 된다.
    h = socks(_connect(port))
    h_buf = bytearray()
    h.sendall(_room_create())
    code, _, _ = _parse_room_info(_recv_frame(h, MsgType.ROOM_INFO, h_buf, 5.0))
    h.sendall(_queue_join() + _room_create())
    g = socks(_connect(port))
    g_buf = bytearray()
    g.sendall(_room_join(code))
    assert _try_recv_frame(g, MsgType.ROOM_INFO, g_buf, 5.0) is not None, (
        "룸 대기 중 보낸 큐 명령 때문에 방이 사라졌다")

    # 포워딩 중 제어 프레임 — 상대에게 흘러가더라도 경기가 계속돼야 한다.
    fa, fb, _, fb_buf = _pair_unranked(port, socks, ready=True)
    fa.sendall(_room_create() + _queue_join() + build_frame(MsgType.ROOM_LEAVE, b""))
    fa.sendall(_input_frame(7))
    frames, closed = _collect(fb, fb_buf, 2.0, stop_on=MsgType.INPUT)
    assert not closed, "포워딩 중 제어 프레임을 보냈다고 상대까지 끊겼다"
    assert any(t == MsgType.INPUT for t, _ in frames), (
        "제어 프레임 뒤의 정상 게임 프레임이 상대에게 도달하지 못했다")

    _relay_still_serves(port, socks)


def test_server_only_frames_are_not_relayed_to_the_peer(unranked, socks):
    """클라이언트가 위조한 S→C 전용 프레임이 상대에게 전달되면 안 된다. **회귀 테스트**.

    MATCH_FOUND / MATCH_RESULT / ROOM_INFO 는 서버만 만들 수 있는 프레임이다.
    클라이언트는 이 타입을 보낼 이유가 없고, 상대 클라이언트는 이 타입이 오면
    서버가 보냈다고 믿는다 — 그게 프로토콜의 전제다.

    포워딩 단계는 프레임을 그대로 흘려보내므로, 한 플레이어가 이 타입들을 직접
    만들어 보내면 상대 화면에는 서버가 보낸 것과 구별되지 않는 프레임이 뜬다.
    위조된 MATCH_RESULT 는 있지도 않은 RP 변동을 보여 주고, 위조된 MATCH_FOUND /
    ROOM_INFO 는 상대 클라이언트를 있지도 않은 상태로 밀어 넣는다. 릴레이가
    "상대가 보낸 것" 과 "서버가 보낸 것" 을 구분해 주지 않으면, 신뢰 경계를
    지킬 수 있는 곳은 아무 데도 남지 않는다.

    릴레이는 이 타입들을 포워딩 경로에서 버려야 한다 (MATCH_SUMMARY 를 이미
    가로채듯이).
    """
    _, port = unranked
    a, b, _, b_buf = _pair_unranked(port, socks, ready=True)

    forged = (
        build_frame(MsgType.MATCH_RESULT, struct.pack("<iii", 9999, 99999, 90000))
        + build_frame(MsgType.ROOM_INFO, b"\x05HAXXX\x00\x02")
        + build_frame(MsgType.MATCH_FOUND,
                      b"\x01" + struct.pack("<Q", 0xDEAD) + b"\x00\x00\x00")
    )
    a.sendall(forged)
    a.sendall(_input_frame(1))          # 뒤이은 정상 프레임으로 도착 시점을 잡는다
    frames, closed = _collect(b, b_buf, 2.5, stop_on=MsgType.INPUT)
    assert not closed, "위조 프레임 때문에 상대 연결이 끊겼다"

    leaked = sorted({t.name for t, _ in frames
                     if t in (MsgType.MATCH_RESULT, MsgType.ROOM_INFO,
                              MsgType.MATCH_FOUND)})
    assert not leaked, (
        "클라이언트가 만든 서버 전용 프레임이 상대에게 그대로 전달됐다 "
        f"({', '.join(leaked)}). 상대 클라이언트는 이것을 서버가 보낸 것과 "
        "구별할 수 없다 — 위조 RP 결과·위조 매치 상태를 주입할 수 있다")
    _relay_still_serves(port, socks)


def test_malformed_frame_lengths_do_not_disturb_other_users(unranked, socks):
    """잘림/과대 길이/길이 불일치 프레임이 다른 사용자에게 번지지 않는가. **가드**.

    세 변형을 각각 다른 연결로 던진다:
      · 선언 길이가 상한(4096+1)을 넘는 프레임 — 파서가 body 를 기다리며 수신
        버퍼를 키우게 만들려는 고전적 수법
      · 선언 길이는 정상인데 실제 바이트가 모자란 프레임(영원히 미완성)
      · 길이 0 프레임 폭주 — 헤더만 있고 타입도 없는 프레임

    셋 다 그 연결 하나에서 끝나야 한다. 릴레이가 죽거나, 다른 사람의 매칭이
    막히거나, 메모리가 자라면 안 된다.
    """
    _, port = unranked

    oversized = socks(_connect(port, "127.0.5.1"))
    oversized.sendall(_raw_frame(int(MsgType.QUEUE_JOIN), b"\x00",
                                 declared_len=MAX_PAYLOAD_BYTES + 64))

    truncated = socks(_connect(port, "127.0.5.2"))
    truncated.sendall(struct.pack("<H", 2000) + b"\x0a" + b"x" * 100)  # 한참 모자람

    zero_len = socks(_connect(port, "127.0.5.3"))
    zero_len.sendall(struct.pack("<H", 0) * 1 + b"\x00" * 4)
    for _ in range(200):
        zero_len.sendall(struct.pack("<H", 0) + b"\x00" * 4)

    # 어떤 조합이든 릴레이가 살아 있고 무관한 사용자가 정상 매칭돼야 한다.
    _relay_still_serves(port, socks, source_ips=("127.0.5.10", "127.0.5.11"))

    # 그리고 과대 길이 선언은 wire 계약상 연결 종료 사유다 (net/framing.h 참고:
    # parse_frames 가 false 를 돌려주면 호출자는 소켓을 닫아야 한다).
    if not _wait_closed(oversized, 3.0):
        pytest.xfail(
            "결함(경미): 상한을 넘는 길이를 선언한 연결이 닫히지 않는다. "
            "net/framing.h 는 parse_frames 가 false 를 돌려주면 호출자가 연결을 "
            "닫는다고 명시하는데, 릴레이의 첫 프레임/룸/큐 단계는 반환값을 "
            "무시한다 — 스트림이 조용히 어긋난 채 연결이 유지된다")


def test_oversized_declaration_while_queued_does_not_swallow_queue_cancel(
        unranked, socks):
    """큐에서 깨진 길이를 흘린 연결이 뒤이은 QUEUE_CANCEL 을 삼키면 안 된다. **결함 노출**.

    이 케이스가 위험한 이유는 피해가 자기 연결에서 끝나지 않기 때문이다.

    큐 단계는 잔여 바이트를 뒤 단계로 넘겨야 해서 수신 버퍼를 소비하지 않고 사본을
    파싱한다. 그 사본 파싱이 상한 초과 길이에서 멈추면, 원본 버퍼 맨 앞에는 깨진
    헤더가 영원히 남는다. 그 뒤로 무엇을 보내도 파서는 같은 자리에서 다시 멈추므로
    **QUEUE_CANCEL 이 영영 관측되지 않는다.**

    결과는 이렇다: 취소한 줄 아는 연결이 큐에 남아 다음에 들어온 정상 사용자와
    짝지어지고, 로비 진입 순간 깨진 헤더 때문에 끊긴다. 정상 사용자는 상대를 잃은
    로비에 홀로 남고(위 로비 통지 결함과 겹쳐 30초를 태운다), 그 다음 사람은 짝을
    못 찾는다. 공격자는 깨진 헤더 7바이트로 정상 사용자 두 명을 묶어 둔다.

    스레드 모델은 큐 폴링이 parse_frames 의 반환값을 보고 그 자리에서 연결을
    닫으므로 이 경로가 없다.
    """
    _, port = unranked

    attacker = socks(_connect(port, "127.0.6.1"))
    attacker.sendall(_queue_join())
    time.sleep(0.15)                     # 큐에 들어갈 시간을 준다
    attacker.sendall(struct.pack("<H", MAX_PAYLOAD_BYTES + 64))   # 깨진 헤더
    time.sleep(0.05)
    attacker.sendall(build_frame(MsgType.QUEUE_CANCEL, b""))      # 취소 의사
    time.sleep(0.25)

    h1 = socks(_connect(port, "127.0.6.2"))
    h1.sendall(_queue_join())
    time.sleep(0.15)
    h2 = socks(_connect(port, "127.0.6.3"))
    h2.sendall(_queue_join())

    # 짝이 지어졌다면 밀리초 단위로 오므로 부재를 증명하는 데 긴 창이 필요 없다.
    b1, b2 = bytearray(), bytearray()
    got1 = _try_recv_frame(h1, MsgType.MATCH_FOUND, b1, 3.0)
    got2 = _try_recv_frame(h2, MsgType.MATCH_FOUND, b2, 3.0)
    if got1 is None or got2 is None:
        pytest.xfail(
            "결함: 큐에서 깨진 길이를 흘린 연결의 QUEUE_CANCEL 이 무시돼 그 "
            "연결이 큐에 남았다. 다음에 들어온 정상 사용자가 그 연결과 짝지어져 "
            "로비에서 버려지고, 그 다음 사람은 짝을 못 찾는다 — 7바이트로 정상 "
            f"사용자 둘을 묶었다 (h1 matched={got1 is not None}, "
            f"h2 matched={got2 is not None})")
    assert _parse_match_found(got1)[1] == _parse_match_found(got2)[1], (
        "취소한 연결이 빠진 뒤 남은 두 정상 사용자가 서로 매칭돼야 한다")


def test_unknown_message_types_are_ignored_at_the_handshake(unranked, socks):
    """모르는 메시지 타입이 핸드셰이크를 깨뜨리지 않는가. **가드**.

    구버전/신버전 클라이언트가 섞이면 서로 모르는 타입이 오간다. 릴레이는 모르는
    타입을 만나도 그 프레임만 넘기고 계속 기다려야 한다 — 여기서 연결을 끊으면
    프로토콜에 선택 기능을 하나도 추가할 수 없게 된다.
    """
    _, port = unranked
    a = socks(_connect(port))
    b = socks(_connect(port))
    junk = b"".join(_raw_frame(t, b"payload") for t in (99, 200, 255, 0))
    a.sendall(junk)
    time.sleep(0.1)
    a.sendall(_queue_join())          # 모르는 타입 뒤에도 정상 명령이 먹혀야 한다
    b.sendall(junk + _queue_join())   # 같은 세그먼트에 섞여 와도 마찬가지
    a_buf, b_buf = bytearray(), bytearray()
    assert _try_recv_frame(a, MsgType.MATCH_FOUND, a_buf, 6.0) is not None, (
        "모르는 타입을 먼저 보냈다고 이후 QUEUE_JOIN 이 무시됐다")
    assert _try_recv_frame(b, MsgType.MATCH_FOUND, b_buf, 6.0) is not None, (
        "모르는 타입과 한 세그먼트에 실려 온 QUEUE_JOIN 이 유실됐다")


def test_frame_boundaries_split_and_coalesced(unranked, socks):
    """프레임을 쪼개 보내거나 여러 개를 붙여 보내도 같은 결과인가. **가드**.

    TCP 는 경계를 보존하지 않으므로 이건 정상 클라이언트에서도 일어나는 일이다.
    다만 공격자는 그것을 **의도적으로** 최악의 모양으로 만든다 — 헤더 2바이트를
    1바이트씩 쪼개거나, 상태를 바꾸는 프레임 여러 개를 한 세그먼트에 붙인다.
    """
    _, port = unranked

    # (1) 1바이트씩 쪼개 보낸 ROOM_CREATE 도 정상 처리돼야 한다.
    split = socks(_connect(port))
    frame = _room_create()
    for i in range(len(frame)):
        split.sendall(frame[i:i + 1])
    s_buf = bytearray()
    code, status, peers = _parse_room_info(
        _recv_frame(split, MsgType.ROOM_INFO, s_buf, 6.0))
    assert len(code) == 5 and (status, peers) == (0, 1)

    # (2) JOIN + READY + CHAT 을 한 세그먼트에 붙여 보낸다.
    guest = socks(_connect(port))
    g_buf = bytearray()
    split.sendall(_ready(1))
    guest.sendall(_room_join(code) + _ready(1) + build_frame(MsgType.CHAT, b"hi"))
    assert _try_recv_frame(guest, MsgType.MATCH_FOUND, g_buf, 6.0) is not None, (
        "JOIN·READY 를 한 세그먼트로 보내면 매치가 시작되지 않는다")
    assert _try_recv_frame(split, MsgType.MATCH_FOUND, s_buf, 6.0) is not None


def test_bad_checksum_frames_are_dropped_but_not_fatal(unranked, socks):
    """체크섬이 틀린 프레임은 버리되 연결을 끊지는 않아야 한다. **가드**.

    체크섬 불일치는 스트림 오염이 아니라 프레임 하나의 손상이다 (길이 오염과
    다르다). 락스텝 루프를 관대하게 유지하려고 파서는 그 프레임만 버리고 계속
    읽는다 — 이 테스트는 릴레이도 같은 정책인지 못 박는다.
    """
    _, port = unranked
    a = socks(_connect(port))
    b = socks(_connect(port))
    bad_join = _raw_frame(int(MsgType.QUEUE_JOIN), b"\x00", checksum=0xDEADBEEF)
    a.sendall(bad_join)
    time.sleep(0.1)
    a.sendall(_queue_join())
    b.sendall(_queue_join())
    a_buf, b_buf = bytearray(), bytearray()
    assert _try_recv_frame(a, MsgType.MATCH_FOUND, a_buf, 6.0) is not None, (
        "체크섬이 틀린 프레임 하나 때문에 연결이 끊겼다")
    assert _try_recv_frame(b, MsgType.MATCH_FOUND, b_buf, 6.0) is not None

    # 포워딩 중 체크섬 오류도 상대의 경기를 끊으면 안 된다.
    a.sendall(_ready(1))
    b.sendall(_ready(1))
    _handshake_forwarding(a, b, a_buf, b_buf)
    a.sendall(_raw_frame(int(MsgType.INPUT), b"\x00" * 6, checksum=0x1234))
    a.sendall(_input_frame(3))
    frames, closed = _collect(b, b_buf, 2.0, stop_on=MsgType.INPUT)
    assert not closed, "포워딩 중 체크섬 오류로 상대까지 끊겼다"


# ═══════════════════════════════════════════════════════════════════════════
# 3. 게임 상태 악용
# ═══════════════════════════════════════════════════════════════════════════
def test_ready_storm_and_simultaneous_ready_still_start_the_match(unranked, socks):
    """READY 를 마구 토글하고 양쪽이 동시에 READY 를 눌러도 방이 정상 출발하는가. **가드**.

    룸의 READY 는 토글이라 클라이언트 버튼 연타만으로도 초당 수십 번 뒤집힌다.
    그리고 두 사람이 같은 순간 READY 를 누르는 것은 예외가 아니라 흔한 일이다 —
    양쪽 READY 를 관측한 쪽이 방을 파괴하며 매치를 시작하므로, 관측과 파괴가
    겹치는 이 지점이 룸 경로에서 가장 미끄럽다.

    바이트 레이트 상한(64 KiB/s) 아래에 머물도록 토글 수를 잡는다 — 상한에 먼저
    걸리면 재려던 것이 아니라 레이트 초과를 재게 된다.
    """
    _, port = unranked
    host = socks(_connect(port))
    h_buf = bytearray()
    host.sendall(_room_create())
    code, _, _ = _parse_room_info(_recv_frame(host, MsgType.ROOM_INFO, h_buf, 5.0))

    guest = socks(_connect(port))
    g_buf = bytearray()
    guest.sendall(_room_join(code))
    _recv_frame(guest, MsgType.ROOM_INFO, g_buf, 5.0)

    for _ in range(40):                       # 약 40 × 8B × 2 = 640B — 상한과 무관
        host.sendall(_ready(1) + _ready(0))
        guest.sendall(_ready(0) + _ready(1) + _ready(0))
        time.sleep(0.005)

    # 마지막에 양쪽이 동시에 READY(1).
    host.sendall(_ready(1))
    guest.sendall(_ready(1))

    mf_h = _try_recv_frame(host, MsgType.MATCH_FOUND, h_buf, 6.0)
    mf_g = _try_recv_frame(guest, MsgType.MATCH_FOUND, g_buf, 6.0)
    assert mf_h is not None and mf_g is not None, (
        "READY 토글 폭주 뒤 동시 READY 로 매치가 시작되지 않았다")
    assert _parse_match_found(mf_h)[1] == _parse_match_found(mf_g)[1]
    assert {_parse_match_found(mf_h)[0], _parse_match_found(mf_g)[0]} == {1, 2}
    _relay_still_serves(port, socks)


def test_room_code_abuse_does_not_disturb_other_rooms(unranked, socks):
    """룸 코드에 쓰레기를 넣어도 남의 방이 멀쩡한가. **가드**.

    룸 코드는 사람이 불러 주고 받아 적는 값이라 오타·붙여넣기 사고가 일상이고,
    공격자에게는 서버 내부 표(코드 → 방)에 임의 문자열을 넣어 보는 입구다. 없는
    코드, 상한을 넘긴 길이 바이트, 제어문자와 비ASCII 바이트를 던진 뒤 **그 사이에
    정상적으로 열려 있던 방** 이 그대로 동작하는지 본다.
    """
    _, port = unranked

    # 먼저 정상 방을 하나 열어 둔다 — 이 방이 피해를 입는지가 관심사다.
    host = socks(_connect(port, "127.0.7.1"))
    h_buf = bytearray()
    host.sendall(_room_create())
    good_code, _, _ = _parse_room_info(
        _recv_frame(host, MsgType.ROOM_INFO, h_buf, 5.0))

    abusive_codes = [
        b"ZZZZZ",                       # 없는 코드
        b"\x00\x01\x02",                # 제어문자
        b"\xff\xfe\xfd\xfc\xfb",        # 비ASCII (UTF-8 로도 안 읽힌다)
        b"AB\nCD",                      # 개행 섞기
    ]
    for i, code in enumerate(abusive_codes):
        s = socks(_connect(port, f"127.0.7.{10 + i}"))
        s.sendall(_room_join(code))
        # ROOM_INFO(notfound) 가 오든 그냥 닫히든 상관없다 — 릴레이가 이 연결
        # 하나로 끝내기만 하면 된다.
        _collect(s, bytearray(), 1.5)

    # 길이 바이트가 상한(5)을 넘는 ROOM_JOIN — 프레임 자체는 정상이지만 코드가 길다.
    long_code = socks(_connect(port, "127.0.7.20"))
    long_code.sendall(build_frame(MsgType.ROOM_JOIN,
                                  bytes([200]) + b"Q" * 200 + b"\x00"))
    _collect(long_code, bytearray(), 1.0)

    # 자기 방에 자기가 다시 들어가기 — 두 번째 연결로 같은 코드에 입장한다.
    self_join = socks(_connect(port, "127.0.7.1"))
    sj_buf = bytearray()
    self_join.sendall(_room_join(good_code))
    info = _try_recv_frame(self_join, MsgType.ROOM_INFO, sj_buf, 3.0)
    assert info is not None, "정상 코드로의 입장이 쓰레기 코드들 뒤에 막혔다"
    _, status, peers = _parse_room_info(info)
    assert status == 0 and peers == 2, (
        f"정상 방의 상태가 오염됐다 (status={status}, peers={peers})")

    # 그 방은 여전히 정상 출발할 수 있어야 한다.
    host.sendall(_ready(1))
    self_join.sendall(_ready(1))
    assert _try_recv_frame(host, MsgType.MATCH_FOUND, h_buf, 6.0) is not None
    _relay_still_serves(port, socks, source_ips=("127.0.7.30", "127.0.7.31"))


def test_queue_join_then_cancel_in_one_segment_leaves_the_queue_empty(
        unranked, socks):
    """QUEUE_JOIN 직후의 QUEUE_CANCEL 이 같은 세그먼트로 와도 존중돼야 한다. **가드**.

    취소를 놓치면 취소한 사람이 큐에 남아 다음 사람과 짝지어지고, 그 사람은 응답
    없는 상대와 로비에서 시간을 태운다. 순서를 뒤집어(넣고 → 짝짓고 → 취소 확인)
    구현하면 정확히 이 창이 열린다.

    취소한 연결 뒤에 정상 사용자 둘을 넣어 **그 둘이 서로** 매칭되는지 본다.
    """
    _, port = unranked
    canceller = socks(_connect(port, "127.0.8.1"))
    canceller.sendall(_queue_join() + build_frame(MsgType.QUEUE_CANCEL, b""))
    time.sleep(0.25)

    h1 = socks(_connect(port, "127.0.8.2"))
    h1.sendall(_queue_join())
    time.sleep(0.1)
    h2 = socks(_connect(port, "127.0.8.3"))
    h2.sendall(_queue_join())

    b1, b2 = bytearray(), bytearray()
    mf1 = _recv_frame(h1, MsgType.MATCH_FOUND, b1, 6.0)
    mf2 = _recv_frame(h2, MsgType.MATCH_FOUND, b2, 6.0)
    assert _parse_match_found(mf1)[1] == _parse_match_found(mf2)[1], (
        "취소한 연결이 큐에 남아 정상 사용자와 짝지어졌다")


def test_forged_match_summary_cannot_decide_the_result_alone(ranked, socks):
    """한쪽이 요약을 위조해도 그 값으로 결과가 확정되면 안 된다. **가드**.

    MATCH_SUMMARY 는 클라이언트가 만드는 값이므로 릴레이는 이것을 믿을 수 없다.
    유일한 방어는 교차검증이다 — 양쪽이 서로의 점수·라인·승패를 거울처럼 보고해야
    승자를 인정한다. 위조하면 교차검증이 깨지고 결과는 무효(RP 변동 0)여야 한다.

    이 테스트는 세 가지를 함께 못 박는다:
      · A 가 보낸 요약은 A 자리에만 들어간다 (상대 요약 사칭 불가)
      · 같은 쪽이 여러 번 보내면 첫 번째만 쓰인다 (유리한 값으로 덮어쓰기 불가)
      · 교차검증이 깨지면 양쪽 다 delta=0 (위조로 이득을 못 본다)
    """
    meta, port = ranked
    a = socks(_connect(port))
    b = socks(_connect(port))
    a_buf, b_buf = bytearray(), bytearray()
    a.sendall(_queue_join(meta.issue()))
    b.sendall(_queue_join(meta.issue()))
    _recv_frame(a, MsgType.MATCH_FOUND, a_buf, 6.0)
    _recv_frame(b, MsgType.MATCH_FOUND, b_buf, 6.0)
    a.sendall(_ready(1))
    b.sendall(_ready(1))
    _handshake_forwarding(a, b, a_buf, b_buf)

    # A: 자기 승리를 주장하고, 이어서 더 유리한 값으로 덮어쓰기를 시도하고,
    #    마지막으로 "상대(B)의 요약" 인 척하는 프레임까지 보낸다.
    a.sendall(_summary(1, 100, 10, 0, 0))
    a.sendall(_summary(1, 999999, 999, 0, 0))
    a.sendall(_summary(0, 0, 0, 999999, 999))
    # B: 진실을 보고한다 — A 의 주장과 맞지 않는다.
    b.sendall(_summary(1, 250, 20, 250, 20))

    res_a = _try_recv_frame(a, MsgType.MATCH_RESULT, a_buf, 8.0)
    res_b = _try_recv_frame(b, MsgType.MATCH_RESULT, b_buf, 8.0)
    assert res_a is not None and res_b is not None, (
        "교차검증이 깨진 경기에서 결과 프레임이 아예 오지 않았다 — 클라이언트는 "
        "결과 대기 화면에서 빠져나올 수 없다")
    assert meta.last_winner_null is True, (
        "교차검증이 깨졌는데 릴레이가 meta 에 승자를 지정해 보냈다 — 위조한 쪽이 "
        "이겼다고 기록된다")
    for name, payload in (("A", res_a), ("B", res_b)):
        before, after, delta = struct.unpack("<iii", payload[:12])
        assert delta == 0, (
            f"{name} 가 위조된 요약으로 RP 변동({delta})을 얻었다 — 교차검증이 "
            "실패했는데 결과가 반영됐다")
        assert before == after

    # 위조 프레임은 상대에게 흘러가서도 안 된다 (릴레이가 가로채는 타입이다).
    frames, _ = _collect(b, b_buf, 0.5)
    assert not any(t == MsgType.MATCH_SUMMARY for t, _ in frames), (
        "MATCH_SUMMARY 가 상대에게 전달됐다 — 릴레이가 가로채지 못했다")


def test_early_match_summary_does_not_end_other_matches(ranked, socks):
    """경기 시작 직후의 조기 MATCH_SUMMARY 가 다른 매치를 건드리지 않는가. **가드**.

    요약은 원래 경기가 끝난 뒤에 온다. 시작하자마자 보내면 릴레이는 결과 확정
    경로(교차검증 → meta POST → MATCH_RESULT)를 즉시 밟는다. 이 경로는 매치 상태를
    파괴하고 워커로 HTTP 왕복을 던지므로, 시작 직후에 여러 쌍이 동시에 이걸 하면
    가장 무거운 코드 경로를 최악의 순서로 밟게 만들 수 있다.

    조기 요약을 쏘는 쌍을 여러 개 만들고, 그 와중에 정상적으로 경기하는 쌍이
    영향을 받지 않는지 본다.
    """
    meta, port = ranked

    # 정상 경기를 하는 쌍 하나를 먼저 세워 둔다.
    good_a = socks(_connect(port))
    good_b = socks(_connect(port))
    ga_buf, gb_buf = bytearray(), bytearray()
    good_a.sendall(_queue_join(meta.issue()))
    good_b.sendall(_queue_join(meta.issue()))
    _recv_frame(good_a, MsgType.MATCH_FOUND, ga_buf, 6.0)
    _recv_frame(good_b, MsgType.MATCH_FOUND, gb_buf, 6.0)
    good_a.sendall(_ready(1))
    good_b.sendall(_ready(1))
    _handshake_forwarding(good_a, good_b, ga_buf, gb_buf)

    # 그 옆에서 조기 요약 쌍을 여러 개 만든다.
    for _ in range(4):
        x = socks(_connect(port))
        y = socks(_connect(port))
        xb, yb = bytearray(), bytearray()
        x.sendall(_queue_join(meta.issue()))
        y.sendall(_queue_join(meta.issue()))
        if _try_recv_frame(x, MsgType.MATCH_FOUND, xb, 6.0) is None:
            continue
        _try_recv_frame(y, MsgType.MATCH_FOUND, yb, 6.0)
        # READY 와 요약을 한 세그먼트로 — 로비에서 포워딩으로 넘어가는 그 순간에
        # 요약이 도착하게 만든다.
        x.sendall(_ready(1) + _summary(1, 10, 1, 20, 2))
        y.sendall(_ready(1) + _summary(0, 20, 2, 10, 1))

    # 정상 쌍은 계속 주고받을 수 있어야 한다.
    good_a.sendall(_input_frame(11))
    frames, closed = _collect(good_b, gb_buf, 3.0, stop_on=MsgType.INPUT)
    assert not closed, "옆에서 조기 요약을 쏘는 동안 무관한 경기가 끊겼다"
    assert any(t == MsgType.INPUT for t, _ in frames), (
        "옆에서 조기 요약을 쏘는 동안 무관한 경기의 프레임이 막혔다")


def test_one_token_cannot_hold_two_sessions_across_paths(ranked, socks):
    """같은 토큰으로 큐와 룸에 동시에 붙을 수 없어야 한다. **가드**.

    한 계정이 동시에 두 세션을 쥐면 자기 자신과 매칭해 RP 를 만들 수 있고, 한
    사람이 여러 자리를 차지해 남의 매칭을 밀어낼 수도 있다. 기존 스위트가 큐↔큐
    중복을 이미 막아 두었으므로, 여기서는 경로를 섞는다 — 첫 연결은 룸을 열고
    두 번째 연결은 같은 토큰으로 큐에 들어간다. 경로가 달라도 같은 문(세션 lease)을
    통과해야 한다.
    """
    meta, port = ranked
    token = meta.issue()

    first = socks(_connect(port))
    f_buf = bytearray()
    first.sendall(_room_create(token))
    code, _, _ = _parse_room_info(_recv_frame(first, MsgType.ROOM_INFO, f_buf, 6.0))
    assert len(code) == 5

    second = socks(_connect(port))
    second.sendall(_queue_join(token))
    assert _wait_closed(second, 6.0), (
        "같은 토큰의 두 번째 연결이 다른 경로(큐)로는 통과했다 — 한 계정이 동시에 "
        "두 자리를 쥘 수 있다")

    # 첫 연결은 아무 영향 없이 계속 살아 있어야 한다 (거절이 엉뚱한 쪽을 치면 안 된다).
    guest = socks(_connect(port))
    g_buf = bytearray()
    guest.sendall(_room_join(code, meta.issue()))
    assert _try_recv_frame(guest, MsgType.ROOM_INFO, g_buf, 6.0) is not None, (
        "중복 세션을 거절하면서 원래 열려 있던 방까지 정리해 버렸다")


# ═══════════════════════════════════════════════════════════════════════════
# 4. 자원 — 한 사람의 소비가 남의 몫을 먹는가
# ═══════════════════════════════════════════════════════════════════════════
def test_session_cap_is_scoped_to_the_offending_address(relays, socks):
    """한 주소의 연결 폭주가 다른 주소의 사용자를 막지 않는가. **가드**.

    상한 자체는 기존 스위트가 경계값으로 못 박아 두었다. 여기서 재는 것은 그
    상한의 **범위** 다 — 한 주소가 자기 몫을 다 썼을 때 거절되는 것은 그 주소뿐
    이어야 한다. 표를 IP 별로 나누지 않으면 접속을 퍼붓는 사람 하나가 서버 전체의
    입구를 잠근다.

    기본값(64)은 테스트를 느리게 만들 뿐 재는 것이 달라지지 않으므로
    ``--max-sessions-per-ip`` 로 8 로 줄여 돌린다 (두 바이너리 공통 인자다).
    """
    _, port = relays(["--max-sessions-per-ip", "8"])
    hog = "127.0.10.1"

    held = []
    for i in range(8):
        s = socks(_connect(port, hog))
        held.append(s)
        s.sendall(_room_create())
        assert _try_recv_frame(s, MsgType.ROOM_INFO, bytearray(), 5.0) is not None, (
            f"상한(8) 안쪽인 {i}번째 연결이 거절됐다 — 상한이 너무 빡빡하다")

    over = socks(_connect(port, hog))
    over.sendall(_room_create())
    assert _wait_closed(over, 4.0), (
        "세션 상한을 넘긴 연결이 살아남았다 — 한 주소가 전역 상한까지 쌓을 수 있다")

    # 다른 주소는 아무 일도 없었어야 한다.
    _relay_still_serves(port, socks, source_ips=("127.0.10.2", "127.0.10.3"))


def test_a_stalled_match_does_not_stall_other_matches(unranked, socks):
    """안 읽는 상대 때문에 멈춘 매치가 다른 매치를 멈추지 않는가. **가드**.

    이 파일에서 가장 중요한 계약이다. 배포 대상은 단일 이벤트 루프이므로 한
    연결에서 벌어지는 일이 곧 전원의 일이 될 수 있다. 상대가 전혀 읽지 않으면
    릴레이의 보류 송신이 쌓이고 흘려보내는 쪽의 읽기가 멈추는데 — 그 상태가
    **다른 매치의 지연으로 새어 나가면** 한 쌍의 나쁜 회선이 서버 전체의 품질이
    된다.

    기존 스위트는 같은 상황에서 "멈춰 세운 송신자가 억울하게 끊기지 않는가" 를
    본다. 여기서는 각도를 바꿔 **바깥에서 본 격리** 를 잰다: 정체된 쌍이 존재하는
    동안 무관한 쌍의 왕복 지연과 신규 접속이 정상인가.
    """
    _, port = unranked

    # (1) 정체될 쌍 — 한쪽은 수신 버퍼를 좁히고 한 바이트도 읽지 않는다.
    sa = socks(_connect(port, "127.0.11.1"))
    sb = socks(_connect(port, "127.0.11.2"))
    sb.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    sa_buf, sb_buf = bytearray(), bytearray()
    sa.sendall(_queue_join())
    sb.sendall(_queue_join())
    _recv_frame(sa, MsgType.MATCH_FOUND, sa_buf, 6.0)
    _recv_frame(sb, MsgType.MATCH_FOUND, sb_buf, 6.0)
    sa.sendall(_ready(1))
    sb.sendall(_ready(1))
    time.sleep(0.3)

    # (2) 정상 쌍.
    ga, gb, ga_buf, gb_buf = _pair_unranked(
        port, socks, ready=True, source_ips=("127.0.11.3", "127.0.11.4"))

    # (3) 정체를 만든다. 바이트 레이트 상한(64 KiB/s) 아래로 유지해야 "정체" 가
    #     아니라 "레이트 초과" 를 재게 되는 일이 없다.
    chunk = build_frame(MsgType.CHAT, b"x" * 400)      # 약 411 B
    sa.settimeout(0.5)
    stall_deadline = time.monotonic() + 3.0
    worst = 0.0
    while time.monotonic() < stall_deadline:
        try:
            for _ in range(5):                          # ≈ 2 KiB / 50 ms ≈ 41 KiB/s
                sa.sendall(chunk)
        except (socket.timeout, OSError):
            pass                                        # 멈춰 세워졌다 — 의도한 상태
        # 그 사이 정상 쌍의 왕복 지연을 잰다. 판정 기준은 절대 지연이 아니라
        # "1.5초 안에 도착하는가" 하나뿐이다 — 부하가 걸린 기계에서 밀리초 단위
        # 기준을 세우면 정체 격리가 아니라 기계의 컨디션을 재게 된다.
        t0 = time.monotonic()
        ga.sendall(_input_frame(int(t0) % 1000))
        frames, closed = _collect(gb, gb_buf, 1.5, stop_on=MsgType.INPUT)
        assert not closed, "정체된 쌍 때문에 무관한 매치가 끊겼다"
        worst = max(worst, time.monotonic() - t0)
        assert any(t == MsgType.INPUT for t, _ in frames), (
            f"정체된 쌍이 있는 동안 무관한 매치의 프레임이 1.5초 안에 도착하지 "
            f"않았다 (최악 {worst:.2f}초) — 한 쌍의 정체가 전원의 지연이 됐다")
        time.sleep(0.05)

    # (4) 그리고 새 사용자가 여전히 들어올 수 있어야 한다.
    _relay_still_serves(port, socks, source_ips=("127.0.11.5", "127.0.11.6"))


def test_relay_survives_meta_being_offline(relays, socks):
    """meta 가 죽어 있을 때 릴레이가 빠르게 거절하고 계속 도는가. **가드**.

    배포 구성에서 meta 는 릴레이와 다른 기기에 있다. 그 기기가 죽거나 네트워크가
    끊기는 것은 예외가 아니라 언젠가 반드시 오는 상태다. 그때 릴레이가 해야 할
    일은 **빠르게 거절하고 계속 서비스하는 것** 이다 — 인증을 못 해 랭크드 매칭이
    안 되는 것은 어쩔 수 없지만, 프로세스가 멈추거나 연결이 무한정 매달려 있으면
    meta 장애 하나가 릴레이 장애로 번진다.
    """
    dead_port = _free_port()          # 아무도 안 듣는 포트
    _, port = relays(["--meta", f"http://127.0.0.1:{dead_port}",
                      "--meta-secret", TEST_RELAY_SECRET])

    # 거절 하나에 실측 2초 안팎이 걸린다(연결 실패를 확인하는 왕복). 이 테스트가
    # 재려는 것은 "매달리지 않는가" 이므로 반복 횟수는 두 번이면 충분하다 —
    # 더 돌려 봐야 같은 값을 다시 재면서 스위트만 느려진다.
    for i in range(2):
        s = socks(_connect(port, f"127.0.12.{i + 1}"))
        s.sendall(_queue_join("f" * 32))
        started = time.monotonic()
        assert _wait_closed(s, 10.0), (
            "meta 가 죽었는데 연결이 닫히지도, 응답을 받지도 못한 채 매달려 있다")
        assert time.monotonic() - started < 8.0

    # 프로세스는 살아 있어야 하고, 새 접속도 계속 받아야 한다.
    probe = socks(_connect(port, "127.0.12.20"))
    probe.sendall(_queue_join("e" * 32))
    assert _wait_closed(probe, 10.0)


def test_relay_rejects_garbage_from_meta(relays, fake_meta, socks):
    """meta 가 쓰레기를 돌려줄 때 릴레이가 그걸 인증으로 받아들이면 안 된다. **가드**.

    프록시가 끼어 HTML 오류 페이지를 돌려주거나, 앞단이 잘못된 라우팅으로 다른
    서비스의 응답을 내보내는 일은 실제로 일어난다. 응답이 200 이라는 사실만으로
    인증을 통과시키면 그런 사고 하나가 곧바로 인증 우회가 된다.

    쓰레기(200 + 비JSON)와 5xx 두 가지를 넣고, 어느 쪽도 통과하지 못하는지 본다.
    """
    _, port = relays(["--meta", fake_meta.url, "--meta-secret", TEST_RELAY_SECRET])

    for mode in ("garbage", "error500"):
        fake_meta.mode = mode
        s = socks(_connect(port))
        s.sendall(_queue_join(fake_meta.issue()))
        assert _wait_closed(s, 8.0), (
            f"meta 가 {mode} 를 돌려줬는데 릴레이가 연결을 유지했다 — 인증이 "
            "통과했을 가능성이 있다")

    # 정상으로 돌아오면 다시 매칭돼야 한다 (거절 상태가 눌러붙지 않는가).
    fake_meta.mode = "ok"
    a = socks(_connect(port))
    b = socks(_connect(port))
    a_buf, b_buf = bytearray(), bytearray()
    a.sendall(_queue_join(fake_meta.issue()))
    b.sendall(_queue_join(fake_meta.issue()))
    assert _try_recv_frame(a, MsgType.MATCH_FOUND, a_buf, 8.0) is not None, (
        "meta 가 회복됐는데 릴레이가 계속 거절한다")
    assert _try_recv_frame(b, MsgType.MATCH_FOUND, b_buf, 8.0) is not None


def test_slow_meta_backlog_does_not_starve_new_players(relays, fake_meta, socks):
    """버려진 인증 요청이 쌓여 정상 사용자를 굶기면 안 된다. **회귀 테스트**.

    배포 대상에서 meta 는 성능이 매우 제한된 보조 기기에서 돈다 — 인증 왕복이
    수백 밀리초 걸리는 것은 이상 상황이 아니라 평상시다. 그 전제에서 이 경로를 본다.

    공격 비용은 TCP 연결 하나다: 붙어서 QUEUE_JOIN 만 던지고 즉시 끊는다. 릴레이는
    연결이 죽었다는 것을 알면서도 이미 던진 인증 작업을 취소하지 않고, per-IP
    상한은 연결이 죽는 순간 반납되므로 **다시 붙는 것을 막지도 않는다**. 그래서
    공격자는 상한에 한 번도 걸리지 않으면서 인증 워커 큐를 원하는 만큼 길게 만들 수
    있고, 그 뒤에 줄을 선 정상 사용자는 큐가 다 빠질 때까지 로그인하지 못한다.
    출발지를 흩으면 per-IP 상한은 아예 관여하지 않는다.

    스레드 모델에는 이 경로가 없다. 인증이 그 연결의 워커 스레드에서 동기로 돌아
    작업과 슬롯이 같은 수명을 갖고, 워커가 다 차면 새 연결을 **거절** 한다 —
    굶기는 대신 거절하는 것이 옳은 실패 모드다.
    """
    fake_meta.delay = 0.25
    _, port = relays(["--meta", fake_meta.url, "--meta-secret", TEST_RELAY_SECRET])

    # 기준선: 한산할 때 정상 사용자가 얼마나 빨리 들어오는가.
    warm = socks(_connect(port, "127.0.13.1"))
    w_buf = bytearray()
    t0 = time.monotonic()
    warm.sendall(_room_create(fake_meta.issue()))
    assert _try_recv_frame(warm, MsgType.ROOM_INFO, w_buf, 10.0) is not None, (
        "기준선조차 통과하지 못했다 — 하니스 문제")
    baseline = time.monotonic() - t0
    assert baseline < 3.0, f"기준선이 이미 느리다 ({baseline:.2f}s)"

    # 공격: 붙어서 QUEUE_JOIN 만 던지고 즉시 끊기를 반복한다. 출발지를 흩어
    # per-IP 상한에 한 번도 닿지 않게 한다 (여러 IP 분산 시나리오).
    churn = 120
    tokens = [fake_meta.issue() for _ in range(churn)]
    for i, tok in enumerate(tokens):
        try:
            s = _connect(port, f"127.0.14.{(i % 200) + 1}", timeout=3.0)
        except OSError:
            continue
        try:
            s.sendall(_queue_join(tok))
        except OSError:
            pass
        s.close()          # FIN — 데이터는 이미 큐에 있으므로 릴레이가 읽고 작업을 던진다
        if i % 16 == 15:
            time.sleep(0.01)   # accept backlog 를 넘기지 않도록 살짝 페이싱

    # 피해자: 공격과 무관한 정상 사용자.
    victim = socks(_connect(port, "127.0.13.2"))
    v_buf = bytearray()
    t1 = time.monotonic()
    victim.sendall(_room_create(fake_meta.issue()))
    info = _try_recv_frame(victim, MsgType.ROOM_INFO, v_buf, 20.0)
    waited = time.monotonic() - t1

    # 기준선의 10배 남짓이면서, 결함이 있을 때 관측되는 대기(수 초)와는 확실히
    # 갈리는 값. 절대 지연이 아니라 "정상 사용자가 굶는가" 를 재는 문턱이다.
    threshold = 4.0
    fail_note = (
        "이미 끊긴 연결들이 남긴 인증 작업이 큐에 쌓여 무관한 사용자가 "
        f"{'끝내 못 들어왔다' if info is None else f'{waited:.1f}초를 기다렸다'} "
        f"(한산할 때 {baseline:.2f}초, 공격 비용은 연결 {churn}개와 "
        "QUEUE_JOIN 한 프레임씩). 연결이 죽으면 그 연결의 인증 작업도 함께 "
        "취소돼야 한다 — 상한이 있는 곳과 일이 쌓이는 곳이 어긋나면 안 된다")
    assert info is not None, fail_note
    assert waited <= threshold, fail_note


def test_connect_and_close_churn_does_not_kill_the_relay(unranked, socks):
    """붙었다 바로 끊기를 반복하는 것만으로 릴레이가 죽으면 안 된다. **결함 노출**.

    이 파일에서 가장 값싼 공격이다. 인증도, 프로토콜도, 단 한 바이트도 필요 없다 —
    TCP 로 붙었다가 곧바로 닫기만 반복한다. 방화벽에서 막을 명분도 없다: 정상
    클라이언트도 서버를 확인할 때 똑같이 한다.

    관측된 것: 두 바이너리 모두 이 반복 도중 어느 순간 종료 코드 0xFFFFFFFF 로
    사라진다. 종료 로그("shutting down"/"done")도, poll 오류 메시지도 남지 않으므로
    정상 종료 경로를 밟지 않았다. 죽는 시점은 일정하지 않다 — 연결 여섯 개 만에
    죽은 적도, 오천 개를 버틴 적도 있다(경합으로 보이는 이유다).

    **이 테스트가 초록이라고 결함이 없다는 뜻이 아니다.** 한 번에 도는 횟수로는
    확률적으로 절반도 못 잡는다. 진짜 그물은 ``relays`` 픽스처의 생존 확인이고,
    그건 이 파일의 모든 테스트에 걸려 있다. 여기서는 그 공격을 이름 붙여
    문서화하고, 다른 테스트보다 짙은 농도로 한 번 더 흔들어 볼 뿐이다.
    """
    proc, port = unranked

    for i in range(600):
        if proc.poll() is not None:
            pytest.xfail(
                f"결함: 붙었다 끊기를 {i}번 반복했을 뿐인데 릴레이가 종료 코드 "
                f"{proc.returncode} 로 사라졌다. 인증도 데이터도 없는 연결만으로 "
                "서버를 내릴 수 있다")
        try:
            s = _connect(port, f"127.0.16.{(i % 200) + 1}", timeout=2.0)
        except OSError:
            continue          # 임시 포트 고갈 등 클라이언트 사정 — 세지 않는다
        s.close()

    _relay_still_serves(port, socks, source_ips=("127.0.16.230", "127.0.16.231"))


# ═══════════════════════════════════════════════════════════════════════════
# 5. 종료
# ═══════════════════════════════════════════════════════════════════════════
def test_sigterm_with_connections_in_every_stage_exits_cleanly(relays, socks):
    """모든 단계에 연결이 걸려 있는 상태에서 종료해도 깔끔하게 끝나는가. **가드**.

    기존 스위트는 포워딩 중인 매치 하나를 두고 종료를 확인한다. 실제 배포에서
    재시작 시점의 서버는 그것보다 지저분하다 — 첫 프레임을 기다리는 연결, 큐에
    선 사람, 빈 방에서 기다리는 사람, 로비, 진행 중인 경기가 동시에 있다. 어느
    한 단계라도 정리 순서가 어긋나면 종료가 걸리고, 운영자에게는 "재시작이 안
    끝난다" 로만 보인다.

    Windows 의 terminate() 는 TerminateProcess 라 핸들러가 돌 기회 자체가 없으므로,
    새 프로세스 그룹으로 띄우고 CTRL_BREAK_EVENT 를 보낸다 (기존 스위트의 선례).
    """
    proc, port = relays(new_group=True)
    relays.allow_exit(proc)          # 이 테스트만은 릴레이가 끝나는 것이 합격이다

    socks(_connect(port))                                  # 첫 프레임 대기
    room = socks(_connect(port))                           # 룸 대기
    room.sendall(_room_create())
    _recv_frame(room, MsgType.ROOM_INFO, bytearray(), 5.0)
    _pair_unranked(port, socks)                            # 로비
    fa, fb, _, _ = _pair_unranked(port, socks, ready=True)  # 포워딩
    fa.sendall(_input_frame(5))
    # 큐 대기는 마지막에 넣는다 — 홀로 남아야 큐 단계로 남고, 먼저 넣으면 뒤에
    # 오는 쌍의 한쪽과 짝지어져 정작 큐 단계에 아무도 없게 된다.
    queued = socks(_connect(port))
    queued.sendall(_queue_join())
    time.sleep(0.1)

    if sys.platform == "win32":
        proc.send_signal(signal.CTRL_BREAK_EVENT)
    else:
        proc.terminate()
    try:
        rc = proc.wait(timeout=10.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        pytest.fail("모든 단계에 연결이 걸린 상태에서 종료가 10초 안에 끝나지 않았다")
    assert rc == 0, (
        f"종료 코드 {rc} — 시그널 핸들러를 거치지 못했거나 정리에 실패했다")


def test_match_seeds_are_not_drawn_from_a_stream_the_client_can_continue(
        unranked, socks):
    """한 매치의 seed 로 다음 매치의 seed 를 계산할 수 없어야 한다.

    seed 는 MATCH_FOUND 로 두 클라이언트에게 그대로 나가는 값이다. 이 값을
    xorshift64 같은 스트림에서 뽑으면 받은 값이 곧 생성기의 내부 상태가 되어,
    한 판을 마친 사람이 같은 변환을 이어 돌리는 것만으로 그 뒤 서버 전체에서
    생성되는 모든 매치의 seed 를 순서대로 얻는다. 브루트포스가 아니라 산술
    한 번이라 상한으로는 막을 수 없다.

    그래서 여기서는 "두 seed 가 다르다" 로 만족하지 않고, **관측한 seed 에서
    스트림을 이어 돌려 다음 seed 가 나오는지** 를 직접 확인한다. 그것이 실제
    공격이 하는 계산이고, 이 성질이 깨지면 여기서 걸린다.
    """
    def xorshift64(x: int) -> int:
        m = (1 << 64) - 1
        x ^= (x << 13) & m
        x ^= x >> 7
        x ^= (x << 17) & m
        return x & m

    _, port = unranked

    seeds = []
    for _ in range(4):
        a = socks(_connect(port, "127.0.0.1"))
        b = socks(_connect(port, "127.0.0.1"))
        a_buf, b_buf = bytearray(), bytearray()
        a.sendall(_queue_join())
        b.sendall(_queue_join())
        _, seed_a = _parse_match_found(
            _recv_frame(a, MsgType.MATCH_FOUND, a_buf, 5.0))
        _, seed_b = _parse_match_found(
            _recv_frame(b, MsgType.MATCH_FOUND, b_buf, 5.0))
        assert seed_a == seed_b, "같은 매치의 두 사람은 같은 seed 를 받아야 한다"
        seeds.append(seed_a)
        a.close()
        b.close()

    assert len(set(seeds)) == len(seeds), f"seed 가 반복된다: {seeds}"
    assert all(s != 0 for s in seeds), "0 seed 는 클라이언트 RNG 를 죽인다"

    # 관측한 seed 에서 스트림을 이어 돌려 본다. 몇 걸음 안에 다음 seed 가 나오면
    # 그 값은 생성기 상태 그대로라는 뜻이다.
    for i in range(len(seeds) - 1):
        x = seeds[i]
        for _ in range(64):
            x = xorshift64(x)
            assert x != seeds[i + 1], (
                f"매치 {i} 의 seed 에서 매치 {i+1} 의 seed 가 계산된다 — "
                "seed 가 노출된 스트림에서 뽑히고 있다")
