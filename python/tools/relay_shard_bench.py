"""포워딩 샤딩(--loops N)이 실제로 처리량을 올리는지 재는 벤치 (Linux 전용).

relay_capacity.py 와 무엇이 다른가
----------------------------------
relay_capacity.py 는 "정해진 속도를 릴레이가 따라오는가"를 보는 운영 탐침이다.
폐루프(보내고 → 받을 때까지 기다리고 → 다음 라운드)라서 생성기 왕복 지연이
속도를 정하고, 목표 속도도 플레이어당 120 프레임/초 수준이다. 그 부하로는 어떤
루프 구성이든 여유가 남아 loops=1 과 loops=4 가 같은 숫자를 낸다 — 병목에 닿지
못하는 측정은 두 구성을 구분할 수 없다.

샤딩이 나누는 것은 **포워딩 경로의 CPU** 다. 그래서 이 도구는 다르게 잰다:

* 개루프(open loop). 각 소켓에 목표 바이트율로 계속 밀어 넣고, 받는 쪽은 큰
  버퍼로 비우기만 한다. 릴레이가 못 따라오면 커널 송신 버퍼가 차서 생성기가
  저절로 눌린다 — 그 지점의 전달량이 곧 릴레이의 용량이다.
* 생성기 비용을 릴레이 비용보다 훨씬 싸게 만든다. 한 번의 send 로 수백 개의
  최소 크기 프레임을 밀어 넣고, 받는 쪽은 256KiB recv 로 한 번에 비운다.
  생성기는 초당 수천 번의 syscall 만 쓰는데, ranked 포워딩 경로는 프레임마다
  send 를 하므로 초당 수십만 번을 쓴다. 이 비대칭이 없으면 4코어 러너에서
  병목은 언제나 파이썬 쪽이고, 측정은 릴레이에 대해 아무것도 말하지 못한다.
* 릴레이 프로세스의 **스레드별** CPU 를 샘플링한다. 처리량이 안 올라도
  "샤드가 일을 나눠 가졌는가"는 스레드별 분포가 직접 보여 준다.

unranked 와 ranked 를 왜 둘 다 재는가
-------------------------------------
두 경로의 포워딩 비용 구조가 다르다.

* unranked 는 받은 바이트를 프레임 경계도 안 보고 그대로 흘린다 — recv 이벤트당
  send 한 번. 게다가 tcp_recv_some 은 한 번에 4KiB 까지만 읽고, 연결당 바이트율
  상한이 걸려 있어 연결당 recv 이벤트 수 자체에 천장이 있다. 즉 비용이 O(이벤트)
  이고 이벤트는 밀릴수록 오히려 줄어든다(한 번에 더 많이 읽으니까). 포화가
  구조적으로 어렵다.
* ranked 는 프레임 경계를 세면서 프레임마다 send 를 부른다 — 비용이 O(프레임).
  밀려도 프레임 수는 줄지 않는다. 샤딩이 값을 할 수 있는 쪽은 여기다.

그래서 두 모드를 같은 부하로 돌리고 둘 다 보고한다. "unranked 는 차이가 없다"는
실패가 아니라 설명이 붙는 결과다.

측정이 못 말하는 것
-------------------
* 부하 생성기가 릴레이와 같은 머신·같은 코어를 나눠 쓴다. 코어가 넉넉하지
  않으면 샤딩의 상한은 코어 수에서 먼저 걸린다. generator_cpu_cores 를 같이
  찍는 이유다.
* loopback 이다. NIC·인터럽트 분산·실제 RTT 는 없다.
* 게임 프레임이 아니라 최소 크기 PING 이다. 프레임 수당 비용은 재지만 페이로드
  처리 비용은 재지 않는다(릴레이는 어차피 페이로드를 해석하지 않는다).

사용:

    uv run python python/tools/relay_shard_bench.py \
        --relay-bin build/tetris_relay_reactor --loops 4 --json out/r.json
    uv run python python/tools/relay_shard_bench.py --summarize out/
"""
from __future__ import annotations

import argparse
import json
import multiprocessing as mp
import os
import signal
import socket
import statistics
import http.client
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from netbot.framing import FramingError, MsgType, build_frame, parse_frames

RECV_CHUNK = 1 << 18          # 받는 쪽을 싸게 유지한다 — 한 번에 최대한 비운다
HANDSHAKE_TIMEOUT = 10.0
FORWARD_PROBE_TIMEOUT = 10.0


# ── 잡동사니 ──────────────────────────────────────────────────────────────────

def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def wait_listen(port: int, timeout: float = 15.0, proc=None) -> bool:
    """포트가 열릴 때까지 기다린다.

    proc 를 주면 그 프로세스가 죽는 즉시 포기한다 — bind 실패로 이미 끝난
    프로세스를 타임아웃 끝까지 기다리는 것은 순전한 낭비다.
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return True
        except OSError:
            if proc is not None and proc.poll() is not None:
                return False
            time.sleep(0.05)
    return False


def proc_cpu_seconds(pid: int) -> float:
    """프로세스 전체 CPU 초 (utime+stime)."""
    with open(f"/proc/{pid}/stat", encoding="utf-8") as fh:
        raw = fh.read()
    # comm 에 공백·괄호가 들어갈 수 있으므로 마지막 ')' 뒤부터 자른다.
    fields = raw[raw.rfind(")") + 2:].split()
    ticks = int(fields[11]) + int(fields[12])
    return ticks / os.sysconf("SC_CLK_TCK")


def thread_cpu_seconds(pid: int) -> dict[str, float]:
    """스레드(tid)별 CPU 초. 샤드가 일을 나눠 가졌는지 보는 직접 증거."""
    out: dict[str, float] = {}
    tick = os.sysconf("SC_CLK_TCK")
    try:
        tids = os.listdir(f"/proc/{pid}/task")
    except OSError:
        return out
    for tid in tids:
        try:
            with open(f"/proc/{pid}/task/{tid}/stat", encoding="utf-8") as fh:
                raw = fh.read()
        except OSError:
            continue
        fields = raw[raw.rfind(")") + 2:].split()
        out[tid] = (int(fields[11]) + int(fields[12])) / tick
    return out


def system_cpu_seconds() -> dict[str, float]:
    """머신 전체 CPU 회계 (/proc/stat 첫 줄).

    릴레이가 코어 3개를 100% 로 쓰는데 처리량이 3배가 아니라면, 남은 원인은
    둘 중 하나다 — 샤딩이 원래 그만큼밖에 못 벌거나, 기계에 더 줄 코어가
    없거나. 프로세스 CPU 만 봐서는 두 설명을 가를 수 없다. loopback 은 커널
    softirq 에서 상당한 일을 하는데 그건 어느 프로세스에도 잡히지 않는다.
    """
    tick = os.sysconf("SC_CLK_TCK")
    with open("/proc/stat", encoding="utf-8") as fh:
        parts = fh.readline().split()[1:]
    names = ["user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal"]
    return {name: int(value) / tick for name, value in zip(names, parts)}


def rss_mib(pid: int) -> float:
    try:
        with open(f"/proc/{pid}/status", encoding="utf-8") as fh:
            for line in fh:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) / 1024.0
    except OSError:
        pass
    return 0.0


# ── 핸드셰이크 (부모 프로세스가 순차로 한다) ──────────────────────────────────

class Reader:
    """설정 단계에서만 쓰는 프레임 리더. 여기서는 파이썬 비용이 상관없다."""

    def __init__(self, sock: socket.socket):
        self.sock = sock
        self.buf = bytearray()

    def wait(self, wanted: MsgType, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while True:
            try:
                frames = parse_frames(self.buf)
            except FramingError as exc:
                raise ConnectionError("relay declared an oversized frame") from exc
            for msg_type, payload in frames:
                if msg_type == wanted:
                    return payload
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"no {wanted.name} frame within deadline")
            self.sock.settimeout(min(remaining, 0.5))
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                continue
            if not data:
                raise ConnectionError("relay closed the connection")
            self.buf.extend(data)


def token_payload(token: str) -> bytes:
    if not token:
        return b"\x00"
    raw = token.encode("ascii")
    return bytes([len(raw)]) + raw


def connect(port: int, source_ip: str) -> socket.socket:
    """source_ip 에서 나가는 연결을 만든다.

    릴레이는 peer IP 당 동시 핸드셰이크를 16개로 묶는다. 부하를 전부 127.0.0.1
    에서 내면 그 한 버킷에 다 몰린다 — 127.0.0.0/8 은 전부 loopback 이므로
    출발지를 흩어 각 버킷을 여유 있게 유지한다. 서버의 admission 정책을
    측정 편의로 건드리지 않으려는 것이지, 그 정책을 우회하려는 게 아니다.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((source_ip, 0))
    sock.settimeout(5.0)
    sock.connect(("127.0.0.1", port))
    sock.settimeout(None)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return sock


def source_ips(count: int) -> list[str]:
    return [f"127.0.{(i // 250) + 1}.{(i % 250) + 1}" for i in range(max(count, 1))]


def handshake_pairs(port: int, tokens: list[str], matches: int,
                    keepalive_frame: bytes,
                    pace_per_second: float) -> list[tuple[socket.socket, socket.socket]]:
    """큐 경로로 matches 쌍을 만들고 포워딩 단계까지 밀어 올린다.

    한 쌍씩 순차로 붙인다 — 큐에 정확히 두 명만 있는 순간에 페어링되므로 짝이
    결정적이고, per-IP 동시 핸드셰이크 상한(16)에도 걸리지 않는다.

    ranked 에서는 접속 하나가 meta /v1/auth/verify 왕복 하나다. meta 는 relay
    버킷에 초당 512 요청 상한을 두는데, verify_token 의 두 번째 인자는 재시도
    횟수가 아니라 타임아웃(초)이라 429 를 맞으면 그대로 실패하고 릴레이가 그
    연결을 끊는다. 그래서 쌍 생성 자체를 페이싱한다 — 셋업에서 몇백 밀리초를
    아끼려다 측정을 통째로 날리는 거래는 남는 게 없다.
    """
    pairs: list[tuple[socket.socket, socket.socket]] = []
    ready_frame = build_frame(MsgType.READY, b"\x01")
    step = 1.0 / pace_per_second if pace_per_second > 0 else 0.0
    next_at = time.monotonic()
    # 연결당 하나씩 다른 출발지를 준다 — per-IP 버킷(16)에 절대 닿지 않게.
    ips = source_ips(matches * 2)
    for index in range(matches):
        delay = next_at - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        next_at += step
        sock_a = connect(port, ips[2 * index])
        sock_b = connect(port, ips[2 * index + 1])
        reader_a, reader_b = Reader(sock_a), Reader(sock_b)
        sock_a.sendall(build_frame(MsgType.QUEUE_JOIN, token_payload(tokens[2 * index])))
        sock_b.sendall(build_frame(MsgType.QUEUE_JOIN, token_payload(tokens[2 * index + 1])))
        reader_a.wait(MsgType.MATCH_FOUND, HANDSHAKE_TIMEOUT)
        reader_b.wait(MsgType.MATCH_FOUND, HANDSHAKE_TIMEOUT)
        sock_a.sendall(ready_frame)
        sock_b.sendall(ready_frame)
        reader_a.wait(MsgType.READY, HANDSHAKE_TIMEOUT)
        reader_b.wait(MsgType.READY, HANDSHAKE_TIMEOUT)
        pairs.append((sock_a, sock_b))

        # 먼저 붙은 쌍이 idle 타임아웃(15초)에 걸리지 않게 주기적으로 깨운다.
        # 셋업이 길어질수록 첫 쌍이 위험하므로 개수 기준으로 훑는다.
        if (index + 1) % 24 == 0:
            keepalive(pairs, keepalive_frame)
    return pairs


def keepalive(pairs, frame: bytes) -> None:
    for sock_a, sock_b in pairs:
        try:
            sock_a.sendall(frame)
            sock_b.sendall(frame)
        except OSError:
            pass
    drain_all(pairs)


def drain_all(pairs) -> int:
    total = 0
    for sock_a, sock_b in pairs:
        for sock in (sock_a, sock_b):
            sock.setblocking(False)
            try:
                while True:
                    chunk = sock.recv(RECV_CHUNK)
                    if not chunk:
                        break
                    total += len(chunk)
                    if len(chunk) < RECV_CHUNK:
                        break
            except (BlockingIOError, OSError):
                pass
            sock.setblocking(True)
    return total


def verify_forwarding(pairs, frame: bytes) -> int:
    """각 쌍이 실제로 전달 중인지 확인한다.

    샤딩에서는 begin_forwarding 이 소켓 소유권을 샤드 스레드의 우편함으로 넘기고
    등록은 그쪽에서 일어난다. 인계가 끝나기 전에 부하를 시작하면 초반 구간이
    측정에서 왜곡되므로, 여기서 한 프레임을 왕복시켜 인계 완료를 확인한다.
    """
    for sock_a, _ in pairs:
        sock_a.sendall(frame)
    ok = 0
    deadline = time.monotonic() + FORWARD_PROBE_TIMEOUT
    pending = list(range(len(pairs)))
    seen = [0] * len(pairs)
    while pending and time.monotonic() < deadline:
        still: list[int] = []
        for index in pending:
            sock_b = pairs[index][1]
            sock_b.setblocking(False)
            try:
                chunk = sock_b.recv(RECV_CHUNK)
            except (BlockingIOError, OSError):
                chunk = b""
            sock_b.setblocking(True)
            seen[index] += len(chunk)
            if seen[index] >= len(frame):
                ok += 1
            else:
                still.append(index)
        pending = still
        if pending:
            time.sleep(0.01)
    drain_all(pairs)
    return ok


# ── meta (ranked 모드) ────────────────────────────────────────────────────────

def fetch_guest_tokens(meta_port: int, count: int, per_second: float = 45.0) -> list[str]:
    """게스트 토큰을 발급받는다.

    meta 는 공개 버킷에 초당 60 요청 상한을 두므로 그 아래로 페이싱한다. 상한을
    넘기면 429 가 오고 그 연결은 토큰 없이 릴레이에 붙어 거절당한다.

    연결 하나를 keep-alive 로 재사용한다. 요청마다 새로 붙으면 수백 개의 임시
    포트를 태우고 그만큼 TIME_WAIT 을 남기는데, 그 임시 포트는 곧이어 릴레이가
    listen 할 포트와 같은 풀에서 나온다 — 실제로 그 충돌로 릴레이가 bind 에
    실패한 적이 있다.
    """
    tokens: list[str] = []
    interval = 1.0 / per_second
    next_at = time.monotonic()
    headers = {"Content-Type": "application/json"}
    conn = http.client.HTTPConnection("127.0.0.1", meta_port, timeout=10.0)
    try:
        for _ in range(count):
            sleep_for = next_at - time.monotonic()
            if sleep_for > 0:
                time.sleep(sleep_for)
            next_at += interval
            for attempt in range(5):
                try:
                    conn.request("POST", "/v1/guest", body=b"{}", headers=headers)
                    response = conn.getresponse()
                    body = response.read()
                    if response.status == 429 and attempt < 4:
                        time.sleep(0.5)
                        continue
                    if response.status != 200:
                        raise RuntimeError(
                            f"POST /v1/guest returned HTTP {response.status}: "
                            f"{body[:200]!r}")
                    tokens.append(json.loads(body)["token"])
                    break
                except (http.client.HTTPException, OSError):
                    conn.close()
                    conn = http.client.HTTPConnection(
                        "127.0.0.1", meta_port, timeout=10.0)
                    if attempt == 4:
                        raise
    finally:
        conn.close()
    return tokens


# ── 부하 워커 ─────────────────────────────────────────────────────────────────

def load_worker(pipe, socks, cfg) -> None:
    """한 슬라이스의 소켓에 개루프로 부하를 걸고 받은 바이트를 센다.

    비싼 것은 전부 피한다 — 프레임 파싱 없음, 타임스탬프 없음, 소켓당 상태는
    정수 몇 개. 릴레이보다 생성기가 먼저 포화하면 측정 자체가 무의미하다.
    """
    burst = cfg["burst"]
    interval = cfg["interval"]
    tick = cfg["tick"]
    warm_end = cfg["warm_end"]
    end = cfg["end"]

    count = len(socks)
    for sock in socks:
        sock.setblocking(False)
    view = memoryview(burst)
    burst_len = len(burst)
    pending: list[memoryview | None] = [None] * count
    next_send = [0.0] * count
    dead = [False] * count

    start = time.monotonic()
    for index in range(count):
        # 소켓별 송신 시각을 한 주기에 고루 흩어 놓는다. 다 같이 보내면 릴레이가
        # 주기적인 스파이크만 보게 되고, 그건 정상 상태의 용량이 아니다.
        next_send[index] = start + interval * index / max(count, 1)

    sent = recvd = 0
    win_sent = win_recvd = 0
    measuring = False
    cpu_start = 0.0
    dead_count = 0

    while True:
        now = time.monotonic()
        if now >= end:
            break
        if not measuring and now >= warm_end:
            measuring = True
            times = os.times()
            cpu_start = times.user + times.system
            win_sent = win_recvd = 0

        for index in range(count):
            if dead[index]:
                continue
            sock = socks[index]
            while True:
                try:
                    chunk = sock.recv(RECV_CHUNK)
                except BlockingIOError:
                    break
                except OSError:
                    dead[index] = True
                    dead_count += 1
                    break
                if not chunk:
                    dead[index] = True
                    dead_count += 1
                    break
                got = len(chunk)
                recvd += got
                if measuring:
                    win_recvd += got
                if got < RECV_CHUNK:
                    break
            if dead[index]:
                continue

            held = pending[index]
            if held is not None:
                try:
                    written = sock.send(held)
                except BlockingIOError:
                    written = 0
                except OSError:
                    dead[index] = True
                    dead_count += 1
                    continue
                sent += written
                if measuring:
                    win_sent += written
                pending[index] = None if written >= len(held) else held[written:]
                if pending[index] is not None:
                    continue

            if now >= next_send[index]:
                try:
                    written = sock.send(view)
                except BlockingIOError:
                    written = 0
                except OSError:
                    dead[index] = True
                    dead_count += 1
                    continue
                sent += written
                if measuring:
                    win_sent += written
                if written < burst_len:
                    pending[index] = view[written:]
                next_send[index] += interval
                if next_send[index] < now:
                    # 릴레이가 못 받아서 밀린 것이다. 밀린 만큼 몰아 보내면
                    # 부하가 톱니가 되므로 일정만 현재로 되돌린다.
                    next_send[index] = now + interval

        slack = tick - (time.monotonic() - now)
        if slack > 0:
            time.sleep(slack)

    times = os.times()
    pipe.send({
        "sent_bytes": sent,
        "recv_bytes": recvd,
        "window_sent_bytes": win_sent,
        "window_recv_bytes": win_recvd,
        "cpu_seconds": (times.user + times.system) - cpu_start,
        "dead_sockets": dead_count,
        "sockets": count,
    })
    pipe.close()


# ── 한 번 측정 ────────────────────────────────────────────────────────────────

def run_once(args) -> dict:
    workdir = Path(tempfile.mkdtemp(prefix="shardbench-"))
    relay_log = workdir / "relay.log"
    meta_log = workdir / "meta.log"
    meta_proc = None
    relay_proc = None
    pairs: list[tuple[socket.socket, socket.socket]] = []
    players = args.matches * 2

    payload = bytes(args.frame_payload)
    frame = build_frame(MsgType.PING, payload)
    burst = frame * args.burst_frames
    target_bytes = int(args.kib_per_conn * 1024)
    interval = len(burst) / target_bytes

    result: dict = {
        "mode": "ranked" if args.meta_bin else "unranked",
        "loops": args.loops,
        "matches": args.matches,
        "players": players,
        "workers": args.workers,
        "frame_bytes": len(frame),
        "burst_bytes": len(burst),
        "offered_kib_per_conn": args.kib_per_conn,
        "duration_s": args.duration,
        "warmup_s": args.warmup,
        "nproc": os.cpu_count(),
        "label": args.label,
    }

    try:
        relay_extra: list[str] = []
        tokens = [""] * players
        if args.meta_bin:
            meta_port = free_port()
            with open(meta_log, "wb") as log:
                meta_proc = subprocess.Popen(
                    [str(args.meta_bin), "--db", str(workdir / "bench.db"),
                     "--http", f"127.0.0.1:{meta_port}",
                     "--relay-secret", args.meta_secret],
                    stdout=log, stderr=log)
            if not wait_listen(meta_port):
                raise RuntimeError("tetris_meta did not start listening")
            token_start = time.monotonic()
            tokens = fetch_guest_tokens(meta_port, players)
            result["token_fetch_s"] = round(time.monotonic() - token_start, 1)
            relay_extra = ["--meta", f"http://127.0.0.1:{meta_port}",
                           "--meta-secret", args.meta_secret]

        # 릴레이 포트는 여기서 잡는다. 미리 잡아 두면 그 사이에 나가는 연결이
        # 같은 임시 포트 풀에서 그 포트를 가져갈 수 있고, 실제로 그렇게 됐다.
        # 잡는 순간과 bind 사이의 틈은 없앨 수 없으니 몇 번 다시 시도한다.
        relay_port = 0
        for attempt in range(5):
            relay_port = free_port()
            with open(relay_log, "wb") as log:
                relay_proc = subprocess.Popen(
                    [str(args.relay_bin), "--port", str(relay_port),
                     "--loops", str(args.loops)] + relay_extra,
                    stdout=log, stderr=log)
            if wait_listen(relay_port, timeout=10.0, proc=relay_proc):
                break
            if relay_proc.poll() is None:
                relay_proc.kill()
            relay_proc.wait(timeout=5.0)
            relay_proc = None
            if attempt == 4:
                raise RuntimeError("relay did not start listening")

        setup_start = time.monotonic()
        pairs = handshake_pairs(relay_port, tokens, args.matches, frame,
                                args.handshake_per_second)
        forwarding = verify_forwarding(pairs, frame)
        result["setup_s"] = round(time.monotonic() - setup_start, 1)
        result["pairs_forwarding"] = forwarding
        if forwarding != len(pairs):
            result["warning_setup"] = (
                f"{len(pairs) - forwarding} pair(s) never echoed a probe frame")

        # 워커에 소켓 슬라이스를 나눠 준다. fork 컨텍스트라 소켓 객체가 그대로
        # 상속된다 — 피클링이 필요 없다.
        flat = [sock for pair in pairs for sock in pair]
        slices: list[list[socket.socket]] = [[] for _ in range(args.workers)]
        for index, sock in enumerate(flat):
            slices[index % args.workers].append(sock)

        ctx = mp.get_context("fork")
        base = time.monotonic() + 0.5      # fork 비용을 흡수할 여유
        warm_end = base + args.warmup
        end = warm_end + args.duration
        cfg = {"burst": burst, "interval": interval, "tick": args.tick,
               "warm_end": warm_end, "end": end}

        procs = []
        pipes = []
        for slice_socks in slices:
            recv_end, send_end = ctx.Pipe(duplex=False)
            proc = ctx.Process(target=load_worker, args=(send_end, slice_socks, cfg))
            proc.start()
            send_end.close()
            procs.append(proc)
            pipes.append(recv_end)

        # 부모는 창(window) 양 끝에서만 /proc 을 읽는다. 워커와 같은
        # CLOCK_MONOTONIC 을 쓰므로 두 창이 정확히 겹친다.
        sleep_for = warm_end - time.monotonic()
        if sleep_for > 0:
            time.sleep(sleep_for)
        cpu_before = proc_cpu_seconds(relay_proc.pid)
        threads_before = thread_cpu_seconds(relay_proc.pid)
        system_before = system_cpu_seconds()
        wall_before = time.monotonic()

        sleep_for = end - time.monotonic()
        if sleep_for > 0:
            time.sleep(sleep_for)
        cpu_after = proc_cpu_seconds(relay_proc.pid)
        threads_after = thread_cpu_seconds(relay_proc.pid)
        system_after = system_cpu_seconds()
        wall_after = time.monotonic()
        result["relay_rss_mib"] = round(rss_mib(relay_proc.pid), 1)

        worker_reports = []
        for pipe, proc in zip(pipes, procs):
            try:
                worker_reports.append(pipe.recv())
            except EOFError:
                worker_reports.append(None)
            proc.join(timeout=10.0)
            if proc.is_alive():
                proc.terminate()

        elapsed = wall_after - wall_before
        recv_bytes = sum(r["window_recv_bytes"] for r in worker_reports if r)
        sent_bytes = sum(r["window_sent_bytes"] for r in worker_reports if r)
        gen_cpu = sum(r["cpu_seconds"] for r in worker_reports if r)
        dead = sum(r["dead_sockets"] for r in worker_reports if r)

        thread_deltas = sorted(
            (after - threads_before.get(tid, 0.0)
             for tid, after in threads_after.items()),
            reverse=True)

        result.update({
            "window_s": round(elapsed, 2),
            "delivered_frames": recv_bytes // len(frame),
            "delivered_frames_per_s": round(recv_bytes / len(frame) / elapsed, 1),
            "delivered_mib_per_s": round(recv_bytes / (1024 * 1024) / elapsed, 2),
            "offered_frames_per_s": round(sent_bytes / len(frame) / elapsed, 1),
            "relay_cpu_cores": round((cpu_after - cpu_before) / elapsed, 3),
            "relay_thread_cores": [round(v / elapsed, 3) for v in thread_deltas[:8]],
            "relay_busy_threads": sum(1 for v in thread_deltas if v / elapsed >= 0.10),
            "generator_cpu_cores": round(gen_cpu / elapsed, 3),
            "dead_sockets": dead,
            "system_idle_cores": round(
                (system_after["idle"] - system_before["idle"]) / elapsed, 2),
            "system_softirq_cores": round(
                (system_after["softirq"] - system_before["softirq"]) / elapsed, 2),
            "system_busy_cores": round(
                sum(system_after[k] - system_before[k]
                    for k in system_after if k not in ("idle", "iowait"))
                / elapsed, 2),
        })
        if result["delivered_frames_per_s"] > 0:
            result["relay_us_per_frame"] = round(
                1e6 * (cpu_after - cpu_before) / (recv_bytes / len(frame)), 3)

        log_text = relay_log.read_text(encoding="utf-8", errors="replace")
        result["handoffs"] = log_text.count("인계 받음")
        result["shard_fallback"] = "단일 루프로 실행합니다" in log_text
        result["relay_alive"] = relay_proc.poll() is None
        if dead:
            result["warning_dead"] = (
                f"{dead} socket(s) died mid-run — 릴레이가 끊었을 수 있다 "
                f"(byte rate/idle). 수치 신뢰도 하락")
        if result["generator_cpu_cores"] >= 0.85 * args.workers:
            result["warning_generator"] = (
                "생성기가 자기 코어를 포화시켰다 — 이 수치는 릴레이 용량이 "
                "아니라 생성기 한계일 수 있다")
        return result
    except BaseException:
        # 여기서 죽으면 원인은 거의 항상 서버 쪽 로그에 한 줄로 적혀 있다
        # (per-IP 상한, meta verify 실패, 중복 세션 …). 로그를 안 보여 주면
        # 원인 하나 확인하는 데 CI 왕복을 통째로 태우게 된다.
        print("--- 실패: 서버 로그 꼬리 ---", file=sys.stderr)
        for name, path in (("relay", relay_log), ("meta", meta_log)):
            if not path.exists():
                continue
            tail = path.read_text(encoding="utf-8", errors="replace").splitlines()
            print(f"[{name}] 마지막 {min(len(tail), 30)}줄:", file=sys.stderr)
            for line in tail[-30:]:
                print(f"  {line}", file=sys.stderr)
        raise
    finally:
        if args.json:
            # 아티팩트에 로그를 함께 남긴다 — 나중에 수치가 이상해 보일 때
            # 릴레이가 그 순간 뭐라고 말했는지 확인할 방법이 이것뿐이다.
            args.json.parent.mkdir(parents=True, exist_ok=True)
            for name, path in (("relay", relay_log), ("meta", meta_log)):
                if path.exists():
                    target = args.json.parent / f"{args.json.stem}.{name}.log"
                    try:
                        target.write_bytes(path.read_bytes())
                    except OSError:
                        pass
        for sock_a, sock_b in pairs:
            for sock in (sock_a, sock_b):
                try:
                    sock.close()
                except OSError:
                    pass
        for proc in (relay_proc, meta_proc):
            if proc is not None and proc.poll() is None:
                proc.send_signal(signal.SIGTERM)
                try:
                    proc.wait(timeout=10.0)
                except subprocess.TimeoutExpired:
                    proc.kill()


# ── 요약 ──────────────────────────────────────────────────────────────────────

def summarize(directory: Path) -> int:
    runs = []
    for path in sorted(directory.glob("*.json")):
        try:
            runs.append(json.loads(path.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            print(f"  (skipped unreadable {path.name})")
    if not runs:
        print("측정 결과 JSON 이 없다")
        return 1

    print(f"러너 코어 수: nproc={runs[0].get('nproc')}")
    print()
    header = (f"{'mode':<9}{'loops':>6}{'rep':>5}{'frames/s':>12}{'MiB/s':>9}"
              f"{'relay_cor':>11}{'busy_thr':>10}{'gen_cor':>9}"
              f"{'sysidle':>9}{'us/frame':>10}{'handoff':>9}{'dead':>6}")
    print(header)
    print("-" * len(header))
    buckets: dict[tuple[str, int], list[dict]] = {}
    for index, run in enumerate(runs):
        key = (run["mode"], run["loops"])
        buckets.setdefault(key, []).append(run)
        print(f"{run['mode']:<9}{run['loops']:>6}{run.get('label', index):>5}"
              f"{run['delivered_frames_per_s']:>12,.0f}"
              f"{run['delivered_mib_per_s']:>9.2f}"
              f"{run['relay_cpu_cores']:>11.2f}"
              f"{run['relay_busy_threads']:>10}"
              f"{run['generator_cpu_cores']:>9.2f}"
              f"{run.get('system_idle_cores', 0):>9.2f}"
              f"{run.get('relay_us_per_frame', 0):>10.2f}"
              f"{run.get('handoffs', 0):>9}"
              f"{run.get('dead_sockets', 0):>6}")

    print()
    print("중앙값과 loops=1 대비 배율 (spread = (max-min)/중앙값):")
    modes = sorted({mode for mode, _ in buckets})
    for mode in modes:
        loop_counts = sorted(loops for m, loops in buckets if m == mode)
        if not loop_counts:
            continue
        baseline = statistics.median(
            r["delivered_frames_per_s"] for r in buckets[(mode, loop_counts[0])])
        for loops in loop_counts:
            group = buckets[(mode, loops)]
            rates = [r["delivered_frames_per_s"] for r in group]
            median = statistics.median(rates)
            spread = (max(rates) - min(rates)) / median * 100 if median else 0.0
            ratio = median / baseline if baseline else 0.0
            cores = statistics.median(r["relay_cpu_cores"] for r in group)
            gen = statistics.median(r["generator_cpu_cores"] for r in group)
            per_frame = statistics.median(
                r.get("relay_us_per_frame", 0) for r in group)
            idle = statistics.median(
                r.get("system_idle_cores", 0) for r in group)
            print(f"  {mode:<9} loops={loops:<3} n={len(group)} "
                  f"median={median:>11,.0f} f/s  spread={spread:5.1f}%  "
                  f"x{ratio:<5.2f} relay={cores:5.2f}코어  "
                  f"{per_frame:5.2f}us/frame  gen={gen:4.2f}코어  "
                  f"놀고있는코어={idle:4.2f}")
        # 처리량 배율과 CPU 배율을 나란히 놓으면 "샤딩이 코어를 더 쓰고 그만큼
        # 덜 벌었는지" 가 바로 보인다. 배율만 적으면 그 대가가 숨는다.
        if len(loop_counts) > 1:
            base_cores = statistics.median(
                r["relay_cpu_cores"] for r in buckets[(mode, loop_counts[0])])
            top = loop_counts[-1]
            top_cores = statistics.median(
                r["relay_cpu_cores"] for r in buckets[(mode, top)])
            top_rate = statistics.median(
                r["delivered_frames_per_s"] for r in buckets[(mode, top)])
            if base_cores > 0 and baseline > 0:
                print(f"    -> loops={top}: 처리량 x{top_rate / baseline:.2f} 를 "
                      f"CPU x{top_cores / base_cores:.2f} 로 샀다")

    print()
    for run in runs:
        for key in ("warning_setup", "warning_dead", "warning_generator"):
            if key in run:
                print(f"  [!] {run['mode']} loops={run['loops']} "
                      f"rep={run.get('label')}: {run[key]}")
        if run["loops"] > 1 and run.get("shard_fallback"):
            print(f"  [!] {run['mode']} loops={run['loops']}: "
                  f"백엔드가 샤딩을 지원하지 않아 단일 루프로 폴백했다")
        if run["loops"] > 1 and not run.get("handoffs"):
            print(f"  [!] {run['mode']} loops={run['loops']}: "
                  f"샤드 인계 기록이 0 이다 — 샤딩이 실제로 일어나지 않았다")
    return 0


# ── main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--summarize", type=Path,
                        help="이 디렉터리의 *.json 을 표로 요약하고 끝낸다")
    parser.add_argument("--relay-bin", type=Path)
    parser.add_argument("--meta-bin", type=Path,
                        help="주면 ranked 모드 (meta 기동 + 게스트 토큰 발급)")
    parser.add_argument("--meta-secret", default="bench-relay-secret")
    parser.add_argument("--loops", type=int, default=1)
    parser.add_argument("--matches", type=int, default=128,
                        help="동시 매치 수 (연결은 그 두 배)")
    parser.add_argument("--workers", type=int, default=2,
                        help="부하 생성기 프로세스 수")
    parser.add_argument("--duration", type=float, default=20.0)
    parser.add_argument("--warmup", type=float, default=4.0)
    parser.add_argument("--kib-per-conn", type=float, default=40.0,
                        help="연결당 목표 송신량 KiB/s (릴레이 상한 64 아래로)")
    parser.add_argument("--burst-frames", type=int, default=512,
                        help="send 한 번에 밀어 넣을 프레임 수")
    parser.add_argument("--frame-payload", type=int, default=0,
                        help="프레임 페이로드 바이트 (0 이면 최소 7바이트 프레임)")
    parser.add_argument("--tick", type=float, default=0.005,
                        help="워커 루프 한 바퀴의 목표 주기(초)")
    parser.add_argument("--handshake-per-second", type=float, default=120.0,
                        help="쌍 생성 속도 상한 (meta 의 relay 버킷 512/s 보호)")
    parser.add_argument("--label", default="0", help="반복 회차 표시용")
    parser.add_argument("--json", type=Path, help="결과 JSON 저장 경로")
    args = parser.parse_args()

    if args.summarize:
        sys.exit(summarize(args.summarize))

    if sys.platform != "linux":
        sys.exit("relay_shard_bench.py is Linux-only (/proc sampling, fork start "
                 f"method); got sys.platform={sys.platform!r}")
    if not args.relay_bin:
        parser.error("--relay-bin is required")
    if args.matches < 1 or args.workers < 1 or args.duration <= 0:
        parser.error("matches, workers, duration must be positive")
    if args.kib_per_conn >= 64:
        parser.error("--kib-per-conn must stay below the relay's 64 KiB/s per-"
                     "connection cap, otherwise it kills the connection")
    # kMaxConns=512. 샤딩에서는 인계된 연결이 앞단 표에서 빠지지만 loops=1 은
    # 전부 한 표에 남는다. 두 구성을 같은 부하로 비교하려면 낮은 쪽에 맞춘다.
    if args.matches * 2 > 512:
        parser.error(f"matches*2={args.matches * 2} exceeds the relay's "
                     "kMaxConns=512 — loops=1 would refuse the surplus and the "
                     "two configurations would no longer be comparable")

    result = run_once(args)
    for key, value in result.items():
        print(f"{key}={value}")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, ensure_ascii=False, indent=2),
                             encoding="utf-8")


if __name__ == "__main__":
    main()
