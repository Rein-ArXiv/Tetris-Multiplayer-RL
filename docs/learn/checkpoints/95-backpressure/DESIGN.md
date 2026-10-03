# 95 Admission and ownership of backlog

- FlowQueue stores owning Frames, bounds queued+active count and wire cost. Fixed storage is not measured RSS.
- Capacity8, hard140B, High105, Low70; push crossing High pauses admission, complete down to Low resumes.
- Pop grants a single active consumer lease without reducing bytes. Final application send completion releases once.
- Queue operations remain mutex-based. Stats is a consistent snapshot, not a reservation.
- Control frames use bounded worker buffers outside application charge; receiver keeps fail-on-overflow policy.
- Producer advances a record number only on stored; full retries same value, with admission and total time limits.
- Native integration checks FIFO/no omissions with immediate and delayed readers; it does not guarantee kernel saturation.
- Root Session now caps queued frames and queued+active wire bytes; WSS acceptance transfers to a separate budget.
- Root round filtering releases discarded queued costs only; transport return releases active cost; joined reset clears both.
- Root drain bounded at64 with quit recheck. Per-call I/O duration still affects scheduling.
- INPUT capacity preflight prevents partial eligible batch admission/ACK. Existing distance/conflict/round policies remain separate.
