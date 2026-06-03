"""Frame format parity tests for ``netbot.framing``.

These tests are pure Python — they don't need the native ``tetris_py`` module
or a running C++ host. They lock in the wire format so refactors to either
``net/framing.cpp`` or ``netbot/framing.py`` immediately fail loud if either
side drifts.

If you need to regenerate the reference frames against a real C++ binary,
write a tiny dump program that calls ``net::build_frame`` for the same
``(type, payload)`` pairs and compares the bytes against the literals below.
"""

from __future__ import annotations

import struct

import pytest

from netbot.framing import (
    FNV1A32_OFFSET,
    MAX_PAYLOAD_BYTES,
    MsgType,
    build_frame,
    fnv1a32,
    parse_frames,
)


# ---- FNV-1a32 known answers ------------------------------------------------
# Reference vectors from the official FNV test suite at
# http://www.isthe.com/chongo/src/fnv/test_fnv.c — these are the values the
# algorithm specification requires, so any drift here is a hash bug.
@pytest.mark.parametrize(
    "data, expected",
    [
        (b"", FNV1A32_OFFSET),       # empty input -> offset basis
        (b"a", 0xE40C292C),
        (b"b", 0xE70C2DE5),
        (b"foobar", 0xBF9CF968),
    ],
)
def test_fnv1a32_known_values(data: bytes, expected: int) -> None:
    assert fnv1a32(data) == expected


# ---- Frame round-trip ------------------------------------------------------
def test_build_frame_empty_payload() -> None:
    frame = build_frame(MsgType.HELLO_ACK, b"")
    # LEN(2) = 1 (just TYPE), TYPE(1) = 2 (HELLO_ACK), no payload, CHK(4) = 0
    assert frame == bytes([0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00])


def test_build_frame_input_payload_layout() -> None:
    # INPUT payload = [tick u32][count u16=1][mask u8]
    payload = struct.pack("<IHB", 42, 1, 0x10)  # tick=42, count=1, mask=DROP
    frame = build_frame(MsgType.INPUT, payload)
    # LEN = 1 (TYPE) + 7 (payload) = 8
    assert frame[:2] == bytes([0x08, 0x00])
    assert frame[2] == int(MsgType.INPUT)  # 4
    assert frame[3:10] == payload
    # Last 4 bytes: FNV-1a32 over payload
    expected_chk = fnv1a32(payload)
    assert struct.unpack_from("<I", frame, 10)[0] == expected_chk


def test_parse_frames_round_trip() -> None:
    payloads = [
        (MsgType.HELLO, struct.pack("<H", 1)),
        (MsgType.SEED, struct.pack("<QIBB", 0xCAFEBABE, 120, 2, 1)),
        (MsgType.INPUT, struct.pack("<IHB", 7, 1, 0b10101)),
        (MsgType.HASH, struct.pack("<IQ", 60, 0xDEADBEEFCAFEBABE)),
        (MsgType.GAME_OVER_CHOICE, b"\x02"),
    ]
    stream = bytearray()
    for t, p in payloads:
        stream += build_frame(t, p)

    parsed = parse_frames(stream)
    assert len(stream) == 0  # all consumed
    assert parsed == payloads


def test_parse_frames_partial_buffer() -> None:
    full = build_frame(MsgType.PING, b"abcd")
    # Feed it one byte short — parser should hold the bytes for the next call.
    stream = bytearray(full[:-1])
    out = parse_frames(stream)
    assert out == []
    assert len(stream) == len(full) - 1  # nothing consumed
    # Now top up with the missing byte and re-parse.
    stream += full[-1:]
    out = parse_frames(stream)
    assert out == [(MsgType.PING, b"abcd")]
    assert len(stream) == 0


def test_parse_frames_drops_bad_checksum() -> None:
    frame = bytearray(build_frame(MsgType.PONG, b"xyz"))
    # Corrupt the checksum (last 4 bytes).
    frame[-1] ^= 0xFF
    out = parse_frames(frame)
    assert out == []  # bad-checksum frame is dropped
    # And the bytes are consumed (parser advances past the corrupt frame).
    assert len(frame) == 0


def test_parse_frames_drops_malformed_zero_length_frame() -> None:
    # LEN=0 has no TYPE byte. C++ consumes that complete malformed frame and
    # keeps parsing later bytes; Python should match that forgiving behavior.
    stream = bytearray(struct.pack("<H", 0) + struct.pack("<I", 0))
    out = parse_frames(stream)
    assert out == []
    assert len(stream) == 0


# ---- Relay / matchmaking enum parity --------------------------------------
def test_queue_join_and_match_found_round_trip() -> None:
    q = build_frame(MsgType.QUEUE_JOIN, b"")
    mf_payload = bytes([1]) + struct.pack("<Q", 0xCAFEBABEDEADBEEF)  # role=HOST, seed
    mf = build_frame(MsgType.MATCH_FOUND, mf_payload)
    stream = bytearray(q + mf)
    parsed = parse_frames(stream)
    assert parsed == [
        (MsgType.QUEUE_JOIN, b""),
        (MsgType.MATCH_FOUND, mf_payload),
    ]
    assert len(stream) == 0


# ---- Custom room / chat enum values & round-trip (Section D) --------------
def test_new_msg_types_have_expected_numeric_values() -> None:
    # These values are the on-wire contract — if anyone edits the enum without
    # also bumping the server/client in lockstep, this test catches it.
    assert int(MsgType.QUEUE_CANCEL) == 11
    assert int(MsgType.ROOM_CREATE)  == 13
    assert int(MsgType.ROOM_JOIN)    == 14
    assert int(MsgType.ROOM_INFO)    == 15
    assert int(MsgType.ROOM_LEAVE)   == 16
    assert int(MsgType.READY)        == 17
    assert int(MsgType.CHAT)         == 20


def test_room_flow_round_trip() -> None:
    code = b"XK7QP"
    create = build_frame(MsgType.ROOM_CREATE, b"")
    join = build_frame(MsgType.ROOM_JOIN, bytes([len(code)]) + code)
    # ROOM_INFO: [code_len:1][code:N][status:1=full][peer_count:1]
    info_payload = bytes([len(code)]) + code + bytes([1, 2])
    info = build_frame(MsgType.ROOM_INFO, info_payload)
    ready = build_frame(MsgType.READY, bytes([1]))
    leave = build_frame(MsgType.ROOM_LEAVE, b"")
    cancel = build_frame(MsgType.QUEUE_CANCEL, b"")

    stream = bytearray(create + join + info + ready + leave + cancel)
    parsed = parse_frames(stream)
    assert parsed == [
        (MsgType.ROOM_CREATE,  b""),
        (MsgType.ROOM_JOIN,    bytes([len(code)]) + code),
        (MsgType.ROOM_INFO,    info_payload),
        (MsgType.READY,        bytes([1])),
        (MsgType.ROOM_LEAVE,   b""),
        (MsgType.QUEUE_CANCEL, b""),
    ]
    assert len(stream) == 0


def test_chat_utf8_round_trip() -> None:
    text = "안녕! hello 👋".encode("utf-8")
    payload = struct.pack("<H", len(text)) + text
    f = build_frame(MsgType.CHAT, payload)
    stream = bytearray(f)
    parsed = parse_frames(stream)
    assert parsed == [(MsgType.CHAT, payload)]
    assert len(stream) == 0


# ---- Payload size guard ---------------------------------------------------
def test_build_frame_rejects_oversized_payload() -> None:
    too_big = b"\x00" * (MAX_PAYLOAD_BYTES + 1)
    with pytest.raises(ValueError):
        build_frame(MsgType.HELLO, too_big)


def test_parse_frames_discards_stream_when_length_exceeds_cap() -> None:
    # Forge a frame header that declares a payload above the cap.
    bad = bytearray()
    bad += struct.pack("<H", MAX_PAYLOAD_BYTES + 2)  # LEN = TYPE + oversized
    bad.append(int(MsgType.HELLO))
    # Don't bother supplying the huge body — the parser should bail before
    # waiting for the bytes to arrive.
    out = parse_frames(bad)
    assert out == []
    assert len(bad) == 0  # entire buffer dropped
