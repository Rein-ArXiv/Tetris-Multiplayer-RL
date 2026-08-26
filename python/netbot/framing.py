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
- :class:`FramingError` — 오버사이즈 길이 선언 등 스트림을 오염시키는 위반.
  C++ 의 ``parse_frames`` return false 에 대응하며, 받은 호출자는 연결을
  닫아야 한다.
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
# net/framing.h 의 net::kMaxPayloadBytes 와 같은 값이어야 한다 (패리티 테스트가 고정).
# u16의 자연 한계(65535)는 이 프로토콜에서 실질적인 상한이 아니다. 현재의
# 정상 메시지는 4KiB 안에 들어오므로 C++ 파서와 같은 hard limit를 둔다.
# 이 값은 단순 메모리 최적화가 아니라, 악성 길이 선언을 받은 파서가 끝없이
# body를 기다리며 수신 버퍼를 키우지 않게 하는 wire 보안 경계다.
MAX_PAYLOAD_BYTES = 4096


class FramingError(Exception):
    """스트림 자체를 오염시키는 프레이밍 위반 (오버사이즈 길이 선언).

    C++ 의 ``net::parse_frames`` 는 이 상황에서 수신 버퍼를 비우고
    ``return false`` 하며, 호출자(relay/Session)는 false 를 보고 연결을
    닫는다. Python 은 반환값이 조용히 무시되기 쉬우므로 예외로 승격해
    호출자가 반드시 연결 종료로 대응하게 강제한다.

    ``frames`` 속성에는 오염 지점 이전까지 정상 파싱된 프레임들이 담긴다
    (C++ 에서 out 파라미터에 이미 쌓인 프레임과 동일).
    """

    def __init__(self, message: str,
                 frames: list[tuple["MsgType", bytes]] | None = None):
        super().__init__(message)
        self.frames: list[tuple[MsgType, bytes]] = frames if frames is not None else []


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

    # 큐·룸 제어는 relay와 Session이 소비하고, 일반 게임 프레임은 전달한다.
    # ranked MATCH_SUMMARY만 relay가 결과 검증을 위해 가로챈다.
    QUEUE_JOIN = 10     # C→S: [tok_len:1][token:N]  (tok_len=0 → unranked)
    QUEUE_CANCEL = 11   # C→S: empty payload (cancel matchmaking)
    MATCH_FOUND = 12    # S→C: [role:1][seed:8 LE][my_icon_len:1][my_icon:N]
                        #      [peer_icon_len:1][peer_icon:N][uuid_len:1][uuid:N]

    # 커스텀 방. 5자리 코드로 붙는다.
    ROOM_CREATE = 13    # C→S: [tok_len:1][token:N]
    ROOM_JOIN = 14      # C→S: [code_len:1][code:N][tok_len:1][token:N]
    ROOM_INFO = 15      # S→C: [code_len:1][code:N][status:1][peer_count:1]
                        #   status: 0=waiting 1=full 2=notfound 3=gonefull
    ROOM_LEAVE = 16     # C→S: empty payload
    READY = 17          # C→S, S→C(forward): [ready:1] (1=ready, 0=not)

    # 메타/RP 연동. 프로토콜 필드명 elo는 하위 호환을 위해 유지한다.
    MATCH_SUMMARY = 18  # C→S: [won:1][my_score:4][my_lines:4]
                        #      [opp_score_observed:4][opp_lines_observed:4]
                        #      [duration_s:4]   (LE, 21 bytes total)
    MATCH_RESULT  = 19  # S→C: [elo_before:4][elo_after:4][delta:4 LE signed]

    CHAT = 20           # bidirectional: [text_len:2 LE][utf8:N] (relay passes through)

    # 서버가 상한에 걸린 연결을 거절하며 사유를 밝힌다.
    #   S→C: [reason:1][text_len:1][utf8:N]
    # 소켓을 그냥 닫으면 사용자에게는 원인 모를 끊김이라, 닫기 직전에 이 프레임을
    # 먼저 내려보낸다. 이 타입을 모르는 구버전 소비자도 안전하다 — 아래
    # parse_frames 는 모르는 타입을 그 프레임만 소비하고 계속 읽고, C++ 쪽도
    # 디스패치 default 로 흘려보낸다 (net/framing.h 의 SERVER_REJECT 주석 참고).
    SERVER_REJECT = 21


class RejectReason(enum.IntEnum):
    """SERVER_REJECT 의 reason 코드 — ``net::RejectReason`` 미러.

    값은 wire 규약이므로 재사용하거나 다시 번호를 매기지 않는다. 새 사유는
    뒤에 덧붙인다.
    """

    SERVER_FULL        = 1  # 프로세스 전체 동시 연결 상한
    IP_SESSION_LIMIT   = 2  # per-IP 동시 세션 상한
    IP_HANDSHAKE_LIMIT = 3  # per-IP 동시 핸드셰이크 상한
    TX_BUDGET          = 4  # 프로세스 전체 보류 송신 예산
    AUTH_BACKLOG       = 5  # 대기 중인 meta 인증 왕복 상한
    ROOM_GUESS_LIMIT   = 6  # per-IP 룸 코드 오답 예산 소진
    QUEUE_NO_SHOW      = 7  # per-IP 매칭 후 무응답 예산 소진


# 서버만 만들 수 있는 프레임 — ``net::is_server_only_type`` 미러.
# 릴레이는 포워딩 중 클라이언트가 올려보낸 이 타입들을 상대에게 전달하지 않는다
# (근거는 net/framing.h 의 is_server_only_type 주석).
SERVER_ONLY_TYPES = frozenset({
    MsgType.MATCH_FOUND,
    MsgType.ROOM_INFO,
    MsgType.MATCH_RESULT,
    MsgType.SERVER_REJECT,
})


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

    Raises:
        FramingError: 선언된 길이(LEN)가 상한
            ``MAX_PAYLOAD_BYTES + TYPE_FIELD_BYTES`` 를 넘는 경우.
            ``stream_buf`` 는 비워진 상태로 던져진다 (버퍼 리셋은 여기서
            이미 완료). C++ 대응 동작: ``net::parse_frames`` 가
            ``return false`` 하고 호출자가 소켓을 close 한다 — Python
            호출자도 이 예외를 받으면 연결을 닫는 것으로 대응해야 한다.
            오염 전까지 파싱된 프레임은 예외의 ``frames`` 속성으로 전달된다.
    """
    out: list[tuple[MsgType, bytes]] = []
    offset = 0
    buf_len = len(stream_buf)

    while True:
        if buf_len - offset < LEN_FIELD_BYTES:
            break

        length = le_read_u16(stream_buf, offset)
        # 길이가 상한을 넘으면 스트림 전체를 버린다. 길이 필드가 깨졌다는
        # 뜻이므로 다음 프레임의 시작점을 더는 신뢰할 수 없다. 억지로 한
        # 프레임만 건너뛰면 쓰레기 바이트를 새 header로 오인할 수 있고,
        # 선언된 body를 기다리면 공격자가 수신 버퍼를 계속 키울 수 있다.
        #
        # 예전에는 여기서 그냥 out 을 반환해 정상 종료와 구분이 안 됐다.
        # C++ 은 return false 로 호출자에게 "연결을 끊어라" 를 알리므로,
        # Python 도 같은 신호를 FramingError 로 준다 (버퍼는 비운 뒤 raise —
        # 호출자는 연결 종료만 책임지면 된다).
        if length > MAX_PAYLOAD_BYTES + TYPE_FIELD_BYTES:
            del stream_buf[:]
            raise FramingError(
                f"declared frame length {length} exceeds cap "
                f"{MAX_PAYLOAD_BYTES + TYPE_FIELD_BYTES}",
                frames=out,
            )
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
                # 모르는 타입은 그 프레임만 소비하고 계속 읽는다. 선택 기능이
                # 추가돼도 구버전 Python 소비자가 예외로 종료되지 않게 한다.
                pass
            else:
                out.append((msg_type, payload))

        offset += need

    if offset > 0:
        del stream_buf[:offset]

    return out
