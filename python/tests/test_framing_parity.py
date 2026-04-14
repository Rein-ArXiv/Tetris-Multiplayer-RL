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
