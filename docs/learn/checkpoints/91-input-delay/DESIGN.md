# 91 로컬 생성 일정으로 지연을 적용

- DelayedLockstep owns Lockstep, local Side, immutable delay<=30, u64 capture cursor.
- Factory rejects invalid side/delay. first tick defaults0; nonzero is useful for boundary tests.
- capture validates mask and wire domain before narrowing; only stored increments cursor.
  32-slot overflow rejects without changing cursor. Caller owns retry/termination policy.
- receive/receive_batch route to opposite Side, keep wire numbers and batch atomicity.
- local_limit = int64(next_capture)-1-delay, including negative startup values.
- advance reports underlying exhausted/finished first, gates local schedule, then asks
  Lockstep for the exact input pair. No new time source or remote-index shift.
- Fixed capture pulses are independent from simulation success. A stalled simulation
  must not prevent generating the inputs needed to open the local release ceiling.
- delay_timeline injects deterministic arrivals, showing D0 and D2 on the same input history.
- delay_probe negotiates D, produces32 inputs, sends two16-input frames (future half first).
  It consumes32-D on EOF, explicitly retainingD. It is a bounded network diagnostic,
  not paced continuous gameplay. Longer gaps, tick-space exhaustion and EOF never synthesize input.
- Current main implements min(local_last-D,remote_max), then checks exact input presence.
  Its maps/queue limits and frame scheduling differ from this fixed-window pure wrapper.
- Delaying remote_max as well would freeze the reserved records whenever new arrivals stop.
  A local release ceiling allows already received records to cover a short arrival gap.
- Delay does not bound receive duration, authenticate peers, remove all jitter or guarantee
  any total user-perceived latency. GUI clocks and server authority remain separate concerns.
