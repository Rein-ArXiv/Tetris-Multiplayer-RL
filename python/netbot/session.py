"""Lockstep session client — Python port of ``net/session.cpp`` (client path only).

The C++ ``net::Session`` runs an I/O thread; here the bot is single-threaded
and pumps the socket explicitly each tick. That's intentional: a 60Hz tick
loop is plenty of slack for a non-blocking ``recv``/``send`` pair, and a
single thread keeps the lockstep loop trivially deterministic.

Protocol (matches the C++ host's expectations):

1. Client connects, immediately sends ``HELLO`` with payload ``u16(1)``
2. Host replies ``HELLO_ACK`` then ``SEED``
3. Client parses ``SEED`` -> :class:`SeedParams`, sets ``ready = True``
4. Each tick the client sends ``INPUT`` with ``[tick u32][count u16=1][mask u8]``
5. The client mirrors the host's ``ACK``/``HASH``/``GAME_OVER_CHOICE`` handling
6. ``HASH`` cross-checks happen in :mod:`netbot.client`, not here

The C++ session sends an ``ACK`` after every received ``INPUT`` batch — we
do the same so the host's lockstep watermark advances normally.
"""

from __future__ import annotations

import errno
import socket
import struct
from collections import deque
from dataclasses import dataclass
from typing import Optional

from .framing import (
    MsgType,
    build_frame,
    le_read_u32,
    le_read_u64,
    parse_frames,
)


@dataclass
class SeedParams:
    """Mirror of ``net::SeedParams``. Populated when the host's SEED arrives."""

    seed: int = 0
    start_tick: int = 120
    input_delay: int = 2
    role: int = 1  # 1 = Host, 2 = Peer (we're the Peer when we Connect)


class GameOverChoice:
    NONE = 0
    RESTART = 1
    GO_TO_TITLE = 2


class BotSession:
    """Non-blocking lockstep client.

    Usage::

        sess = BotSession("127.0.0.1", 7777)
        sess.connect()
        while not sess.ready:
            sess.pump()
            time.sleep(0.002)
        # ... per-tick loop:
        sess.send_input(tick, mask)
        sess.pump()
        remote_mask = sess.get_remote_input(tick)

    The session does **not** drive the simulation — that's the caller's job.
    All this class does is move bytes and parse frames.
    """

    RECV_CHUNK = 4096

    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port

        self.sock: Optional[socket.socket] = None
        self.recv_buf = bytearray()
        self.send_queue: deque[bytes] = deque()

        self.connected = False
        self.ready = False
        self.failed = False

        self.seed_params = SeedParams()

        # remote_inputs[tick] -> mask. Populated by INPUT frames.
        self.remote_inputs: dict[int, int] = {}
        # own_inputs[tick] -> mask. Populated by send_input(); the lockstep
        # loop reads from here so we don't need a separate localInputs map.
        self.own_inputs: dict[int, int] = {}

        self.last_remote_tick = 0
        self.last_local_sent = -1

        self.last_remote_hash_tick = 0
        self.last_remote_hash = 0

        self.remote_game_over_choice = GameOverChoice.NONE

    # ---- connection lifecycle -------------------------------------------
    def connect(self) -> None:
        """Open the TCP connection and queue the initial HELLO frame."""
        self.sock = socket.create_connection((self.host, self.port))
        self.sock.setblocking(False)
        self.connected = True
        # HELLO payload is a single u16=1 (matches Session::Connect)
        self.send_queue.append(build_frame(MsgType.HELLO, struct.pack("<H", 1)))

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None
        self.connected = False
        self.ready = False

    # ---- per-tick pump ---------------------------------------------------
    def pump(self) -> None:
        """Drain pending sends, drain pending recvs, dispatch frames."""
        if not self.connected or self.sock is None:
            return
        self._drain_send()
        self._drain_recv()

    def _drain_send(self) -> None:
        assert self.sock is not None
        while self.send_queue:
            packet = self.send_queue[0]
            try:
                sent = self.sock.send(packet)
            except BlockingIOError:
                return
            except OSError as exc:
                self._mark_failed(f"send failed: {exc}")
                return
            if sent == 0:
                self._mark_failed("send returned 0 (peer closed?)")
                return
            if sent < len(packet):
                self.send_queue[0] = packet[sent:]
                return
            self.send_queue.popleft()

    def _drain_recv(self) -> None:
        assert self.sock is not None
        while True:
            try:
                chunk = self.sock.recv(self.RECV_CHUNK)
            except BlockingIOError:
                break
            except OSError as exc:
                if getattr(exc, "errno", None) in (errno.EAGAIN, errno.EWOULDBLOCK):
                    break
                self._mark_failed(f"recv failed: {exc}")
                return
            if not chunk:
                self._mark_failed("peer closed connection")
                return
            self.recv_buf += chunk

        for msg_type, payload in parse_frames(self.recv_buf):
            self._handle_frame(msg_type, payload)

    def _mark_failed(self, reason: str) -> None:
        self.failed = True
        self.connected = False
        # Caller can read .failed; we don't raise so the main loop stays in
        # control of how to handle the disconnect.

    # ---- frame dispatch --------------------------------------------------
    def _handle_frame(self, msg_type: MsgType, payload: bytes) -> None:
        if msg_type is MsgType.HELLO:
            # Defensive: clients normally don't receive HELLO from the host
            # (the host queues HELLO + SEED back-to-back), but reflect a
            # HELLO_ACK if we ever do, matching session.cpp:222-229.
            self.send_queue.append(build_frame(MsgType.HELLO_ACK, b"\x01"))

        elif msg_type is MsgType.HELLO_ACK:
            # No-op; the host is just acknowledging our HELLO.
            pass

        elif msg_type is MsgType.SEED:
            if len(payload) >= 8 + 4 + 1 + 1:
                self.seed_params = SeedParams(
                    seed=le_read_u64(payload, 0),
                    start_tick=le_read_u32(payload, 8),
                    input_delay=payload[12],
                    role=payload[13],
                )
                self.ready = True

        elif msg_type is MsgType.INPUT:
            if len(payload) >= 4 + 2:
                from_tick = le_read_u32(payload, 0)
                count = struct.unpack_from("<H", payload, 4)[0]
                masks = payload[6 : 6 + count]
                for i, mask in enumerate(masks):
                    tick = from_tick + i
                    self.remote_inputs[tick] = mask
                    if tick > self.last_remote_tick:
                        self.last_remote_tick = tick
                # Acknowledge the new high-watermark, exactly like the C++ host.
                ack_payload = struct.pack("<I", self.last_remote_tick)
                self.send_queue.append(build_frame(MsgType.ACK, ack_payload))

        elif msg_type is MsgType.ACK:
            # We don't currently use ACK for retry — the lockstep watermark
            # is enough. Kept here for symmetry with the C++ dispatch.
            pass

        elif msg_type is MsgType.HASH:
            if len(payload) == 4 + 8:
                self.last_remote_hash_tick = le_read_u32(payload, 0)
                self.last_remote_hash = le_read_u64(payload, 4)

        elif msg_type is MsgType.GAME_OVER_CHOICE:
            if len(payload) >= 1:
                self.remote_game_over_choice = payload[0]

        elif msg_type is MsgType.PING:
            # Echo PING -> PONG with the same payload (matches session.cpp:289-292)
            self.send_queue.append(build_frame(MsgType.PONG, payload))

        elif msg_type is MsgType.PONG:
            # RTT measurement hook — not implemented yet.
            pass

    # ---- send helpers ----------------------------------------------------
    def send_input(self, tick: int, mask: int) -> None:
        """Queue a single-tick INPUT frame.

        Payload layout matches ``Session::SendInput`` exactly:
        ``[tick u32][count u16 = 1][mask u8]``
        """
        self.own_inputs[tick] = mask
        if tick > self.last_local_sent:
            self.last_local_sent = tick
        payload = struct.pack("<IHB", tick & 0xFFFFFFFF, 1, mask & 0xFF)
        self.send_queue.append(build_frame(MsgType.INPUT, payload))

    def send_hash(self, tick: int, hash_value: int) -> None:
        payload = struct.pack(
            "<IQ", tick & 0xFFFFFFFF, hash_value & 0xFFFFFFFFFFFFFFFF
        )
        self.send_queue.append(build_frame(MsgType.HASH, payload))

    def send_game_over_choice(self, choice: int) -> None:
        self.send_queue.append(build_frame(MsgType.GAME_OVER_CHOICE, bytes([choice])))

    # ---- read helpers ----------------------------------------------------
    def get_remote_input(self, tick: int) -> int | None:
        return self.remote_inputs.get(tick)

    def get_own_input(self, tick: int) -> int | None:
        return self.own_inputs.get(tick)

    def clear_inputs(self) -> None:
        self.remote_inputs.clear()
        self.own_inputs.clear()
        self.last_remote_tick = 0
        self.last_local_sent = -1
