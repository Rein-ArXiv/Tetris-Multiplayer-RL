"""Codec for pipe-separated opponent/profile lines in a trusted local config.

A profile is one text line with nine ``|``-separated fields::

    identity | display name | model | icon | portrait | difficulty | interval | think | minimum

This module is a standard-library-only text helper. It performs no file-system
access and claims no path or sandbox security; callers decide how trusted local
data is stored and read.
"""

from __future__ import annotations

import re
from typing import Sequence

__all__ = ["split_profile_line", "encode_profile"]

FIELD_COUNT = 9

# Field positions in a normalized profile.
IDENTITY = 0
NAME = 1
MODEL = 2
ICON = 3
PORTRAIT = 4
DIFFICULTY = 5
INTERVAL = 6
THINK = 7
MINIMUM = 8

_BOM = "\ufeff"

# ASCII characters trimmed from the outer edges of parsed fields.
_TRIM = " \t\r\n"

# ASCII whitespace that must not sit at an encoded field's outer edges.
_ASCII_WHITESPACE = " \t\n\r\v\f"

_IDENTITY_RE = re.compile(r"[a-z0-9_-]{1,32}\Z")
_DECIMAL_RE = re.compile(r"[0-9]+\Z")

_MAX_NAME_BYTES = 96

# (position, label, minimum, maximum) for the numeric pacing fields.
_PACING_BOUNDS = (
    (INTERVAL, "interval", 1, 30),
    (THINK, "think", 0, 180),
    (MINIMUM, "minimum", 1, 600),
)


def _first_forbidden_char(value: str) -> str | None:
    """Return the first character forbidden in a field, or ``None``."""
    for char in value:
        if char in "#|":
            return char
        codepoint = ord(char)
        if codepoint < 0x20 or codepoint == 0x7F:
            return char
    return None


def _validate_semantics(fields: Sequence[str]) -> None:
    """Validate normalized fields, raising ``ValueError`` when invalid.

    Shared by :func:`split_profile_line` and :func:`encode_profile` so both
    paths share field semantics after parser normalization.
    """
    if len(fields) != FIELD_COUNT:
        raise ValueError(f"expected {FIELD_COUNT} fields, got {len(fields)}")

    for position, value in enumerate(fields):
        if not isinstance(value, str):
            raise ValueError(f"field {position} must be a string")
        forbidden = _first_forbidden_char(value)
        if forbidden is not None:
            raise ValueError(f"field {position} must not contain {forbidden!r}")
        if value and (value[0] in _ASCII_WHITESPACE or value[-1] in _ASCII_WHITESPACE):
            raise ValueError(
                f"field {position} must not have leading or trailing whitespace"
            )

    identity = fields[IDENTITY]
    name = fields[NAME]
    model = fields[MODEL]

    if _IDENTITY_RE.fullmatch(identity) is None:
        raise ValueError("identity must match [a-z0-9_-]{1,32}")

    if not name:
        raise ValueError("display name must not be empty")
    if len(name.encode("utf-8")) > _MAX_NAME_BYTES:
        raise ValueError("display name must be at most 96 UTF-8 bytes")

    if not model:
        raise ValueError("model must not be empty")

    for position, label, low, high in _PACING_BOUNDS:
        value = fields[position]
        if _DECIMAL_RE.fullmatch(value) is None:
            raise ValueError(f"{label} must be ASCII decimal digits")
        if not low <= int(value) <= high:
            raise ValueError(f"{label} must be between {low} and {high}")

    # ``difficulty`` is optional: any legal string, including empty, is allowed.


def split_profile_line(line: str) -> list[str] | None:
    """Parse one config line into normalized fields.

    Returns the nine stripped fields, or ``None`` when the line is blank or a
    comment. Raises ``ValueError`` for a line with the wrong field count or
    invalid field values. A leading U+FEFF and everything from the first ``#``
    onward are discarded before splitting.
    """
    if not isinstance(line, str):
        raise TypeError("line must be a string")

    if line.startswith(_BOM):
        line = line[1:]

    comment = line.find("#")
    if comment != -1:
        line = line[:comment]

    if not line.strip(_TRIM):
        return None

    parts = line.split("|")
    if len(parts) != FIELD_COUNT:
        raise ValueError(f"expected {FIELD_COUNT} fields, got {len(parts)}")

    fields = [part.strip(_TRIM) for part in parts]
    _validate_semantics(fields)
    return fields


def encode_profile(fields: Sequence[str]) -> str:
    """Validate nine fields and serialize them to one newline-terminated line.

    Raises ``ValueError`` if the field count or any value is invalid; see
    :func:`_validate_semantics`.
    """
    values = list(fields)
    _validate_semantics(values)
    return "|".join(values) + "\n"
