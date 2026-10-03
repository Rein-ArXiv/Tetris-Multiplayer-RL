# 98 End negotiation

- One main owner, one nonzero round; no I/O, clock reads, threads or verdict API.
- TYPE42: exactly 9 bytes, nonzero round u64 LE plus restart1/leave2. Decode/encode preserve output on rejection.
- Dormant accepts one immutable remote intention without starting the timeout. Activation requires local simulation terminal.
- Waiting starts its deadline on activation. Either leave vetoes restart; both restarts produce local observed agreement.
- Agreement does not acknowledge delivery of our choice or commit a new configuration. Retain accepted output in bounded transport queues.
- Old/future rounds never change clocks/state. Duplicate choices are idempotent while accepting; conflicting choices preserve the first.
- Caller supplies monotonic time in consistent units. Elapsed subtraction avoids deadline-addition overflow. Backward time is reported without mutation.
- Timeout is checked before valid current-round choices; deadline wins ties. Caller must poll status during silence.
- Transport loss revokes a pending/agreed restart on this connection. Leave/timeout remain terminal. No outcome/reward inference.
- end_timeline composes RoundPlay through real terminal; the fixture explicitly supplies next-round config.
- Root Session TYPE9 remains untagged one byte, with exact shape/domain and immutable remote choice checks. Root Q closes directly; ranked restart requires requeue.
- Root seed value polling and no wire round identity remain limits, separate from this tagged teaching codec.
