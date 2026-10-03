'''Korean-aware emphasis for markdown-it-py (CommonMark, HTML disabled).

This is a Korean authoring extension, NOT strict CommonMark. It keeps the
CommonMark emphasis algorithm intact and only relaxes the flanking rules
for '*' runs that sit between Hangul and punctuation, so Korean sentences
such as '한**(강조)**글' render while Latin rules and '_' emphasis stay
unchanged.
'''

from __future__ import annotations

from markdown_it import MarkdownIt
from markdown_it.common.utils import isMdAsciiPunct, isPunctChar
from markdown_it.rules_inline.state_inline import Delimiter, StateInline

# Hangul syllables plus modern/compatibility (and extended/halfwidth) Jamo.
_HANGUL_RANGES = (
    (0x1100, 0x11FF),  # Hangul Jamo (modern)
    (0x3130, 0x318F),  # Hangul Compatibility Jamo
    (0xAC00, 0xD7A3),  # Hangul Syllables
    (0xA960, 0xA97F),  # Hangul Jamo Extended-A
    (0xD7B0, 0xD7FF),  # Hangul Jamo Extended-B
    (0xFFA0, 0xFFDC),  # Hangul Halfwidth Jamo
)


def _is_hangul(ch: str) -> bool:
    '''True when ch is a Hangul syllable or Jamo code point.'''
    if not ch:
        return False
    cp = ord(ch)
    return any(lo <= cp <= hi for lo, hi in _HANGUL_RANGES)


def _is_punct(ch: str) -> bool:
    '''Punctuation notion used by scanDelims.'''
    if not ch:
        return False
    return isMdAsciiPunct(ord(ch)) or isPunctChar(ch)


def tokenize(state: StateInline, silent: bool) -> bool:
    '''Tokenize '*' and '_' runs, mirroring the built-in emphasis rule.

    Korean authoring extension: for '*' only, an opener also opens when the
    preceding char is Hangul and the following char is punctuation, and a
    closer also closes when the preceding char is punctuation and the
    following char is Hangul. All other delimiter handling is unchanged.
    '''
    if silent:
        return False

    start = state.pos
    marker = state.src[start]
    if marker not in ('*', '_'):
        return False

    scanned = state.scanDelims(start, marker == '*')
    can_open = scanned.can_open
    can_close = scanned.can_close
    length = scanned.length

    if marker == '*':
        prev_ch = state.src[start - 1] if start > 0 else ''
        after = start + length
        next_ch = state.src[after] if after < state.posMax else ''

        if _is_hangul(prev_ch) and _is_punct(next_ch):
            can_open = True
        if _is_punct(prev_ch) and _is_hangul(next_ch):
            can_close = True

    marker_char = ord(marker)
    for _ in range(length):
        token = state.push('text', '', 0)
        token.content = marker
        state.delimiters.append(
            Delimiter(
                marker=marker_char,
                length=length,
                token=len(state.tokens) - 1,
                end=-1,
                open=can_open,
                close=can_close,
            )
        )

    state.pos += length
    return True


def make_markdown() -> MarkdownIt:
    '''Safe (HTML-disabled) CommonMark parser with the Korean extension.'''
    md = MarkdownIt('commonmark', {'html': False})
    md.enable('table').enable('strikethrough')
    md.inline.ruler.at('emphasis', tokenize)
    return md