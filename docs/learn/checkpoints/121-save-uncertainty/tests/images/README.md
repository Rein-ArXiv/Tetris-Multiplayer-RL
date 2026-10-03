# Fixture provenance

PNG fixtures and player.png are generated locally with Python struct/zlib (filter 0, no external images).
RGB/gray/RGBA samples use the byte values in expected.json. player.png encodes badge_pixels.h exactly.
color.jpg is a constant 8x8 RGB(64,128,192) image encoded with Pillow, quality=95, subsampling=0.
JPEG assertions allow +/-2 rounding and require alpha=255. No Pillow dependency at build/test time.
corrupt.png is a 45-byte PNG prefix; too-large.png declares 8192x8192 RGBA with an empty IDAT.
The latter must be rejected by the decoded-size policy after header inspection, before full decoding.
These fixtures demonstrate contracts, not an exhaustive malformed-input/fuzzing corpus.
