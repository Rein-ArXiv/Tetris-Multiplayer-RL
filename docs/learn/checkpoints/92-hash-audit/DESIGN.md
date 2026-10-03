# 92 동일 상태 번호를 비교

- StateStamp tick counts completed input pairs: S0 initial, after input0 is S1.
- TYPE21/payload24 stores tick/host/peer as LE u64. Canonical host/peer fields are
  independent of local/remote origin. Hash0 is valid data. Tick<=UINT32_MAX+1, period4.
- Codecs publish only a fully valid candidate. No struct memory/padding is serialized.
- HashAudit stores local/remote optional snapshots in an 8-sample window. The next
  comparison is always first. Future arrivals cannot evict pending old samples.
- record classifies identical duplicate/conflict/stale/too_far/invalid. Poll preserves
  output while either origin is missing; success consumes one comparison even on mismatch.
- Fresh HashAudit per round. All calls serialized by one owner; mutexes/threads/I/O absent.
- hash_probe publishes initial state, sends two16-input batches, then routes input/hash.
  After each successful advance it publishes each periodic state. It half-closes only
  after all its input and hash frames have been sent; comparison continues until EOF.
- Target32-D input steps; expected samples0..floor(target/4)*4. Tail between last sample
  and target is not checked on wire. End hashes printed for diagnostics are separate.
- Bounded tiny burst experiment; receive duration and continuous-game scheduling remain
  separate policies. Missing periodic samples fail completion. No fallback neutral hashes.
- Current game retains only latest remote hash and four local samples. This example's
  complete-in-window comparison contract is stronger than that current storage policy.
- Role-tagged pair hash in current core/hash.h avoids equal-value XOR cancellation,
  but does not eliminate all collisions or authenticate any remote claim.
