# 94 Correlated response and elapsed-time policy

- Optional ThreadLink heartbeat defaults off; preserve all cumulative tools and tests.
- Pure Heartbeat takes monotonic logical uint64 milliseconds; ordered subtraction only.
- waiting vs confirmed healthy; suspect at 2000, sticky expiry at 3000 since activation/last confirmation.
- Issue one outstanding token, interval from last issue. Token0 invalid; no wrap reuse.
- Failed issue/pong may age time; backwards clock leaves state unchanged. Expiry wins at equal deadline.
- TYPE30/31 exact8 LE token. Separate candidate output validation. Not authentication.
- Worker owns timer/control dispatch. Ordinary frames still flow through main's bounded queue.
- Pending echo1 slot, priority at outgoing frame boundaries; current partial frame is never interleaved.
- Echo overflow fails. Main not pumping ordinary messages can still overflow that queue.
- Last-good age includes local scheduling and send progress. Probe admission is not kernel delivery.
- Half-close stops new sends; peer EOF or timer failure still settles terminal report.
- Probe uses known control traffic and TYPE32 completion marker, no gameplay/SEED integration claim.
- Current Session uses 16 timestamp requests and max10s age, readiness, exact length, consume-once.
- Current LinkStatus may recover during grace; teaching expiry is terminal.
- TCP keepalive and application heartbeat observe different processing boundaries.
