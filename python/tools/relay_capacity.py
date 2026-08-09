"""Exercise a relay with real TCP pairs and report Linux process usage.

Run from the repository root. This is an operator load probe, not a pytest.
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

from netbot.framing import MsgType, build_frame, parse_frames


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
        for msg_type, payload in parse_frames(buf):
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
        start = time.monotonic()
        deadline = start + duration
        sequence = 0
        while time.monotonic() < deadline:
            payload = struct.pack("<Q", sequence)
            ping = build_frame(MsgType.PING, payload)
            for a, b in pairs:
                a.sendall(ping)
                b.sendall(ping)
            for a, b in pairs:
                receive_type(a, MsgType.PING, buffers)
                receive_type(b, MsgType.PING, buffers)
                delivered += 2
            sequence += 1
            remaining = start + sequence * interval - time.monotonic()
            if remaining > 0:
                time.sleep(remaining)

        end = time.monotonic()
        end_ticks, rss_kib, threads = process_sample(proc.pid)
        cpu_percent = 100.0 * (end_ticks - start_ticks) / ticks_per_second / (end - start)
        print(f"players={matches * 2} matches={matches} duration_s={end - start:.1f}")
        print(f"cpu_percent={cpu_percent:.1f} rss_mib={rss_kib / 1024:.1f} threads={threads}")
        print(f"delivered_frames={delivered} failures=0")
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
    parser = argparse.ArgumentParser()
    parser.add_argument("--relay-bin", type=Path, required=True)
    parser.add_argument("--matches", type=int, default=100)
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--interval", type=float, default=1.0)
    args = parser.parse_args()
    if args.matches < 1 or args.duration <= 0 or args.interval <= 0:
        parser.error("matches, duration, and interval must be positive")
    run(args.relay_bin, args.matches, args.duration, args.interval)


if __name__ == "__main__":
    main()
