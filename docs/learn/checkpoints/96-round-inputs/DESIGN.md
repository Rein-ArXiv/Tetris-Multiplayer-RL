# 96 Round-scoped input history

- One main owner composes RoundGate and DelayedLockstep. No new thread or clock.
- Local capture only in playing; current-round remote input may stage during countdown.
- Prepare validates both candidate lifecycle and game before replacement; id strictly increases, zero reserved, no wrap.
- TYPE41 owns a u64 round + existing input-batch fields (payload15..30). Exact size and transactional codec outputs.
- Receive validates syntax, then scope, then the tick window. Old/future rounds never mutate or reset current history.
- Capture success creates local history and a Frame; caller must retain it on send full and bound pending production.
- Missing remote input is never synthesized as neutral. Advance owns terminal transitions.
- Round id is connection-scoped identity, not authentication or start agreement. Existing HASH/SEED formats remain unchanged.
- Root SendInput guards connected/ready/quit; main guards ready plus gameplay state. This does not fix the separate worker-neutral-input ownership boundary.
