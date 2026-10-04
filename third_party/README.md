# Vendored dependencies

## nlohmann/json

- Version: 3.11.3, single header `json.hpp` (MIT; copyright and license are included in the header).
- Official release: https://github.com/nlohmann/json/releases/tag/v3.11.3
- Source: https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
- SHA-256: `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6`
- Used by `meta/json_input.h` for complete JSON validation, including a bounded-depth and duplicate-key policy.

Other existing dependencies retain their license notices in their source/header files.

## cpp-httplib local integration patch

- Base header: cpp-httplib 0.18.5, with its original MIT license retained.
- `discard_handled_request_body` handles bodies left unread when a pre-routing
  handler returns `Handled` (for example, the meta service's 429 response).
  It discards only a single decimal Content-Length within both the configured
  payload limit and a 64 KiB cap. It allocates no body buffer and does not parse
  or decompress the rejected data. Missing framing means an empty HTTP body.
- A two-second deadline is checked between reads; an individual blocking read
  still uses the configured stream timeout. This is not an exact two-second
  total deadline. Unsupported transfer encoding, ambiguous/oversized length,
  truncated input or timeout closes the connection instead of reusing unread
  data as another request. For an unsupported body already arriving on the
  socket, complete delivery of the rejection response is not guaranteed.
- Normal fixed-length rejections are drained before sending their response,
  preventing unread bytes from corrupting keep-alive parsing or causing a
  Windows reset while the client reads the response body.
- Preserve/review this patch on dependency upgrades. `http_early_response_test`
  checks framing/caps without sockets; `test_meta_db_smoke.py` checks a real 429
  followed by health on the same TCP connection and unsupported framing.
