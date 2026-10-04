# stb_image dependency

This checkpoint vendors the repository's stb_image v2.30 header unchanged.
SHA-256: `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`. Upstream: https://github.com/nothings/stb
The complete dual MIT/public-domain license is retained at the end of stb_image.h.
No download occurs during configure/build. Implementation is compiled in
renderer/image_decode.cpp only, with PNG/JPEG support and memory input.
The accepted format/policy is this checkpoint's choice, not all stb capabilities.

## stb_truetype

The repository's stb_truetype v1.26 header is vendored unchanged. Its complete
dual MIT/public-domain license remains in the header. Implementation is compiled
only in text/font.cpp. Use trusted packaged fonts; the parser does not validate
all offsets against a byte length. No download occurs during configure/build.
