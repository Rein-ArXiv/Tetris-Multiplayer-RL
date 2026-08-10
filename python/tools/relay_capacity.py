"""Exercise a relay with real TCP pairs and report Linux process usage.

The payload is a small PING frame because the relay's unranked forwarding path
does not interpret ordinary game frames.  The default rate approximates one
60 Hz INPUT plus its ACK in each client direction.  Run from the repository
root.  This is an operator load probe, not a pytest or a WAN latency test.
"""
from __future__ import annotations

import argparse
import os
import signal
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from netbot.framing import FramingError, MsgType, build_frame, parse_frames


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def wait_listen(port: int, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("relay did not start listening")


def receive_type(sock: socket.socket, wanted: MsgType,
                 buffers: dict[int, bytearray], timeout: float = 5.0) -> bytes:
    deadline = time.monotonic() + timeout
    sock.settimeout(min(timeout, 0.5))
    buf = buffers.setdefault(sock.fileno(), bytearray())
    while time.monotonic() < deadline:
        try:
            frames = parse_frames(buf)
        except FramingError as exc:
            # C++ 대응(parse_frames false → 호출자 close): relay 가 오버사이즈
            # 길이를 선언했다는 뜻. 버퍼 리셋은 parse_frames 가 이미 했으니
            # 여기서는 연결을 닫고, 라운드 단위 실패 집계 경로가 잡는
            # ConnectionError 로 승격한다.
            sock.close()
            raise ConnectionError(
                "relay declared an oversized frame length") from exc
        for msg_type, payload in frames:
            if msg_type == wanted:
                return bytes(payload)
        try:
            data = sock.recv(4096)
        except socket.timeout:
            continue
        if not data:
            raise ConnectionError("relay closed the connection")
        buf.extend(data)
    raise TimeoutError(f"no {wanted.name} frame")


def process_sample(pid: int) -> tuple[int, int, int]:
    stat = Path(f"/proc/{pid}/stat").read_text().split()
    cpu_ticks = int(stat[13]) + int(stat[14])
    status = Path(f"/proc/{pid}/status").read_text().splitlines()
    values = {line.split(":", 1)[0]: line.split(":", 1)[1].strip()
              for line in status if ":" in line}
    rss_kib = int(values["VmRSS"].split()[0])
    threads = int(values["Threads"])
    return cpu_ticks, rss_kib, threads


def run(relay_bin: Path, matches: int, duration: float, interval: float) -> None:
    port = free_port()
    proc = subprocess.Popen(
        [str(relay_bin), "--port", str(port)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    sockets: list[socket.socket] = []
    buffers: dict[int, bytearray] = {}
    delivered = 0
    failures = 0
    try:
        wait_listen(port)
        join = build_frame(MsgType.QUEUE_JOIN, b"\x00")
        ready = build_frame(MsgType.READY, b"\x01")
        pairs: list[tuple[socket.socket, socket.socket]] = []
        for _ in range(matches):
            a = socket.create_connection(("127.0.0.1", port), timeout=2.0)
            b = socket.create_connection(("127.0.0.1", port), timeout=2.0)
            sockets.extend((a, b))
            a.sendall(join)
            b.sendall(join)
            receive_type(a, MsgType.MATCH_FOUND, buffers)
            receive_type(b, MsgType.MATCH_FOUND, buffers)
            a.sendall(ready)
            b.sendall(ready)
            receive_type(a, MsgType.READY, buffers)
            receive_type(b, MsgType.READY, buffers)
            pairs.append((a, b))

        ticks_per_second = os.sysconf("SC_CLK_TCK")
        start_ticks, _, _ = process_sample(proc.pid)
        # 생성기(이 스크립트) 자신의 CPU 시간도 같이 샘플링한다. 폐루프
        # 생성기가 코어를 포화시키면 병목은 relay 가 아니라 우리 쪽이므로,
        # 이 비율 없이는 결과를 해석할 수 없다.
        self_start = os.times()
        start = time.monotonic()
        deadline = start + duration
        sequence = 0
        try:
            while time.monotonic() < deadline:
                payload = struct.pack("<Q", sequence)
                ping = build_frame(MsgType.PING, payload)
                # 라운드 단위 실패 집계: 한 라운드가 timeout/연결 오류로
                # 죽어도 측정 전체를 버리지 않고 failures 만 올리고 계속한다.
                # 예전에는 예외가 run() 을 통째로 탈출해 요약이 아예 안
                # 찍혔고, 요약의 'failures=0' 은 하드코딩된 거짓말이었다.
                try:
                    for a, b in pairs:
                        a.sendall(ping)
                        b.sendall(ping)
                    for a, b in pairs:
                        receive_type(a, MsgType.PING, buffers)
                        receive_type(b, MsgType.PING, buffers)
                        delivered += 2
                except (ConnectionError, TimeoutError, OSError):
                    failures += 1
                sequence += 1
                remaining = start + sequence * interval - time.monotonic()
                if remaining > 0:
                    time.sleep(remaining)
        finally:
            # KeyboardInterrupt 등으로 루프를 탈출해도 부분 요약은 남긴다 —
            # 측정을 끝까지 못 돌린 운영자에게도 지금까지의 수치가 유용하다.
            end = time.monotonic()
            self_end = os.times()
            elapsed = max(end - start, 1e-9)
            self_cpu_s = ((self_end.user - self_start.user)
                          + (self_end.system - self_start.system))
            self_cpu_ratio = self_cpu_s / elapsed
            players = matches * 2
            achieved_rate = delivered / players / elapsed
            requested_rate = 1.0 / interval
            rate_ratio = achieved_rate / requested_rate
            print(f"players={players} matches={matches} duration_s={elapsed:.1f}")
            try:
                end_ticks, rss_kib, threads = process_sample(proc.pid)
                cpu_percent = (100.0 * (end_ticks - start_ticks)
                               / ticks_per_second / elapsed)
                print(f"cpu_percent={cpu_percent:.1f} "
                      f"rss_mib={rss_kib / 1024:.1f} threads={threads}")
            except OSError:
                # relay 가 도중에 죽었으면 /proc 샘플은 못 얻는다 — 나머지
                # 부분 요약은 그대로 출력한다.
                print("cpu_percent=unavailable (relay process exited mid-run)")
            print(
                f"requested_frames_per_player_s={requested_rate:.1f} "
                f"achieved_frames_per_player_s={achieved_rate:.1f} "
                f"rate_ratio={rate_ratio:.3f}"
            )
            print(f"delivered_frames={delivered} failures={failures}")
            print(f"generator_cpu_s={self_cpu_s:.1f} "
                  f"generator_cpu_ratio={self_cpu_ratio:.2f}")
            if self_cpu_ratio >= 0.9:
                print("경고: 생성기 병목 — 결과 신뢰 불가 "
                      "(generator_cpu_ratio>=0.9, 단일 스레드 폐루프 생성기가 "
                      "코어 포화에 근접 — relay 는 더 여유가 있을 수 있다)")
            print("caveat: 부하 생성기가 relay 와 같은 머신에서 CPU 를 "
                  "경쟁하는 단일 스레드 폐루프라서, 생성기 병목이 relay "
                  "한계로 오인될 수 있다.")
            print("caveat: 이 수치는 unranked raw 포워딩 기준이다 — ranked 는 "
                  "프레임 파싱 + meta POST 비용이 추가된다.")
    finally:
        for sock in sockets:
            sock.close()
        if proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                proc.kill()


def main() -> None:
    # /proc 샘플링과 os.sysconf("SC_CLK_TCK") 를 쓰는 Linux 전용 도구다.
    # 예전에는 relay 기동 + 접속 셋업을 다 마친 뒤에야 /proc 읽기 단계에서
    # AttributeError 로 죽었다 — 진입 즉시 명시적으로 거절해 헛일과
    # "relay 가 깨졌나?" 류의 오해를 막는다.
    if sys.platform != "linux":
        sys.exit("relay_capacity.py is Linux-only (/proc process sampling); "
                 f"got sys.platform={sys.platform!r}")
    parser = argparse.ArgumentParser()
    parser.add_argument("--relay-bin", type=Path, required=True)
    parser.add_argument(
        "--matches",
        type=int,
        default=50,
        help="simultaneous two-player matches (default: 50, or 100 players)",
    )
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument(
        "--interval",
        type=float,
        default=1.0 / 120.0,
        help="seconds between frames from each client (default: 1/120, INPUT+ACK approximation)",
    )
    args = parser.parse_args()
    if args.matches < 1 or args.duration <= 0 or args.interval <= 0:
        parser.error("matches, duration, and interval must be positive")
    run(args.relay_bin, args.matches, args.duration, args.interval)


if __name__ == "__main__":
    main()
