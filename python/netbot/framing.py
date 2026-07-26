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
# net/framing.cpp의 MAX_PAYLOAD_BYTES와 같은 값이어야 한다.
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

    # 여기부터는 클라이언트와 relay 서버 사이에서만 오가는 매치메이킹용이다.
    # MATCH_FOUND 이후로는 relay가 바이트를 그대로 흘려보내기만 하므로
    # 이 메시지들이 lockstep 루프까지 내려오는 일은 없다.
    QUEUE_JOIN = 10     # C→S: [tok_len:1][token:N]  (tok_len=0 → unranked)
    QUEUE_CANCEL = 11   # C→S: empty payload (cancel matchmaking)
    MATCH_FOUND = 12    # S→C: [role:1][seed:8 LE][my_icon_len:1][my_icon:N]
                        #      [peer_icon_len:1][peer_icon:N]  role: 1=HOST, 2=GUEST

    # 커스텀 방. 5자리 코드로 붙는다.
    ROOM_CREATE = 13    # C→S: [tok_len:1][token:N]
    ROOM_JOIN = 14      # C→S: [code_len:1][code:N][tok_len:1][token:N]
    ROOM_INFO = 15      # S→C: [code_len:1][code:N][status:1][peer_count:1]
                        #   status: 0=waiting 1=full 2=notfound 3=gonefull
    ROOM_LEAVE = 16     # C→S: empty payload
    READY = 17          # C→S, S→C(forward): [ready:1] (1=ready, 0=not)

    # Section K — 메타/RP 연동(프로토콜 필드명은 하위 호환상 elo 유지).
    MATCH_SUMMARY = 18  # C→S: [won:1][my_score:4][my_lines:4]
                        #      [opp_score_observed:4][opp_lines_observed:4]
                        #      [duration_s:4]   (LE, 21 bytes total)
    MATCH_RESULT  = 19  # S→C: [elo_before:4][elo_after:4][delta:4 LE signed]

    CHAT = 20           # bidirectional: [text_len:2 LE][utf8:N] (relay passes through)


def fnv1a32(data: bytes, seed: int = FNV1A32_OFFSET) -> int:
    """FNV-1a 32-bit hash. Identical bit pattern to ``net::fnv1a32`` in C++."""
    h = seed & FNV1A32_MASK
    for byte in data:
        h ^= byte
        h = (h * FNV1A32_PRIME) & FNV1A32_MASK
    return h


# --- little-endian 읽기/쓰기 ---

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


# --- 프레임 만들기와 뜯기 ---

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
        # 길이가 상한을 넘으면 스트림 전체를 버린다.
        # 길이 필드가 깨졌다는 뜻이고, 그러면 다음 프레임이 어디서 시작하는지도
        # 알 수 없다. 억지로 복구하려 들면 쓰레기를 계속 먹는다.
        # 무한히 큰 길이를 보내 수신 버퍼를 불리는 공격도 여기서 막힌다.
        if length > MAX_PAYLOAD_BYTES + TYPE_FIELD_BYTES:
            del stream_buf[:]
            return out
        need = LEN_FIELD_BYTES + length + CHECKSUM_FIELD_BYTES
        if buf_len - offset < need:
            break

        if length < TYPE_FIELD_BYTES:
            offset += need
            continue

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
                # 모르는 타입은 그 프레임만 버리고 계속 읽는다.
                # 나중에 메시지가 추가돼도 구버전 클라이언트가 죽지 않는다.
                pass
            else:
                out.append((msg_type, payload))

        offset += need

    if offset > 0:
        del stream_buf[:offset]

    return out
