"""Wire-format framing — Python port of ``net/framing.cpp``.

Frame layout (matches the C++ implementation byte-for-byte)::

    [LEN: u16 LE][TYPE: u8][PAYLOAD: LEN-1 bytes][CHECKSUM: u32 LE]

- ``LEN``      = 1 + len(PAYLOAD) (i.e. it counts the TYPE byte)
- ``CHECKSUM`` = FNV-1a 32-bit over the PAYLOAD bytes only (NOT over LEN/TYPE).
  When the payload is empty the checksum is 0 (matches the C++ short-circuit).

This module exposes:

- :data:`MsgType` enum mirroring ``net::MsgType``
- :func:`build_frame` for serialisation
- :func:`parse_frames` for stream parsing — operates on a ``bytearray`` and
  trims consumed bytes in place, just like the C++ ``erase`` does
- Little-endian read/write helpers

The unit test ``python/tests/test_framing_parity.py`` round-trips against
captured C++ frames to keep this in sync.
"""

from __future__ import annotations

import enum
import struct

FNV1A32_OFFSET = 2166136261  # 0x811C9DC5
FNV1A32_PRIME = 16777619     # 0x01000193
FNV1A32_MASK = 0xFFFFFFFF

LEN_FIELD_BYTES = 2
TYPE_FIELD_BYTES = 1
CHECKSUM_FIELD_BYTES = 4
MIN_FRAME_BYTES = LEN_FIELD_BYTES + TYPE_FIELD_BYTES + CHECKSUM_FIELD_BYTES  # 7
# Matches net/framing.cpp: MAX_PAYLOAD_BYTES guard.
# u16의 자연 한계(65535)는 사실상 상한이 없으므로, 실사용 최대(CHAT 200자 UTF-8 ~800B,
# HASH/INPUT은 수십 B)에 맞춰 실질적인 하드 리미트로 4KB를 건다.
MAX_PAYLOAD_BYTES = 4096


class MsgType(enum.IntEnum):
    HELLO = 1
    HELLO_ACK = 2
    SEED = 3
    INPUT = 4
    ACK = 5
    PING = 6
    PONG = 7
    HASH = 8
    GAME_OVER_CHOICE = 9

    # Relay / matchmaking extensions (only used between client and relay server).
    # After MATCH_FOUND the relay forwards raw bytes, so these types never reach
    # the lockstep game loop — they live at the "outer" protocol layer.
    QUEUE_JOIN = 10     # C→S: empty payload (anonymous queue)
    QUEUE_CANCEL = 11   # C→S: empty payload (cancel matchmaking)
    MATCH_FOUND = 12    # S→C: [role:1][seed:8 LE]  role: 1=HOST, 2=GUEST

    # Custom rooms (Section D) — 5-char code flow
    ROOM_CREATE = 13    # C→S: empty payload; server mints a code and echoes ROOM_INFO
    ROOM_JOIN = 14      # C→S: [code_len:1][code:N]
    ROOM_INFO = 15      # S→C: [code_len:1][code:N][status:1][peer_count:1]
                        #   status: 0=waiting 1=full 2=notfound 3=gonefull
    ROOM_LEAVE = 16     # C→S: empty payload
    READY = 17          # C→S, S→C(forward): [ready:1] (1=ready, 0=not)

    CHAT = 20           # bidirectional: [text_len:2 LE][utf8:N] (relay passes through)


def fnv1a32(data: bytes, seed: int = FNV1A32_OFFSET) -> int:
    """FNV-1a 32-bit hash. Identical bit pattern to ``net::fnv1a32`` in C++."""
    h = seed & FNV1A32_MASK
    for byte in data:
        h ^= byte
        h = (h * FNV1A32_PRIME) & FNV1A32_MASK
    return h


# ---- LE primitives ---------------------------------------------------------

def le_write_u16(buf: bytearray, value: int) -> None:
    buf += struct.pack("<H", value & 0xFFFF)


def le_write_u32(buf: bytearray, value: int) -> None:
    buf += struct.pack("<I", value & 0xFFFFFFFF)


def le_write_u64(buf: bytearray, value: int) -> None:
    buf += struct.pack("<Q", value & 0xFFFFFFFFFFFFFFFF)


def le_read_u16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def le_read_u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def le_read_u64(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


# ---- Framing ---------------------------------------------------------------

def build_frame(msg_type: MsgType | int, payload: bytes | bytearray) -> bytes:
    """Serialise ``(msg_type, payload)`` into the wire format.

    The result is exactly what ``net::build_frame`` produces in C++ — bytewise
    identical, including the empty-payload checksum=0 short-circuit.
    """
    payload_bytes = bytes(payload)
    if len(payload_bytes) > MAX_PAYLOAD_BYTES:
        raise ValueError(f"frame payload exceeds MAX_PAYLOAD_BYTES: {len(payload_bytes)}")
    out = bytearray()
    length = TYPE_FIELD_BYTES + len(payload_bytes)
    if length > 0xFFFF:
        raise ValueError(f"frame payload too large: {len(payload_bytes)} bytes")
    le_write_u16(out, length)
    out.append(int(msg_type) & 0xFF)
    out += payload_bytes
    checksum = 0 if not payload_bytes else fnv1a32(payload_bytes)
    le_write_u32(out, checksum)
    return bytes(out)


def parse_frames(stream_buf: bytearray) -> list[tuple[MsgType, bytes]]:
    """Pull all complete frames out of ``stream_buf`` and return them.

    Bytes belonging to fully-parsed frames are removed from ``stream_buf`` in
    place — partial frames at the end are left for the next call. Frames whose
    checksum doesn't match are silently dropped (same behaviour as the C++
    parser, which keeps the lockstep loop forgiving rather than fatal).
    """
    out: list[tuple[MsgType, bytes]] = []
    offset = 0
    buf_len = len(stream_buf)

    while True:
        if buf_len - offset < LEN_FIELD_BYTES:
            break

        length = le_read_u16(stream_buf, offset)
        # Drop the whole stream if a frame declares a payload larger than the
        # cap — matches the C++ behaviour and prevents an attacker from
        # making our recv buffer grow without bound.
        if length > MAX_PAYLOAD_BYTES + TYPE_FIELD_BYTES:
            del stream_buf[:]
            return out
        need = LEN_FIELD_BYTES + length + CHECKSUM_FIELD_BYTES
        if buf_len - offset < need:
            break

        msg_type_byte = stream_buf[offset + LEN_FIELD_BYTES]
        payload_start = offset + LEN_FIELD_BYTES + TYPE_FIELD_BYTES
        payload_len = length - TYPE_FIELD_BYTES
        payload = bytes(stream_buf[payload_start : payload_start + payload_len])

        chk_pos = offset + LEN_FIELD_BYTES + length
        chk = le_read_u32(stream_buf, chk_pos)
        calc = 0 if payload_len == 0 else fnv1a32(payload)

        if chk == calc:
            try:
                msg_type = MsgType(msg_type_byte)
            except ValueError:
                # Unknown type — drop the frame defensively rather than crash.
                pass
            else:
                out.append((msg_type, payload))

        offset += need

    if offset > 0:
        del stream_buf[:offset]

    return out
