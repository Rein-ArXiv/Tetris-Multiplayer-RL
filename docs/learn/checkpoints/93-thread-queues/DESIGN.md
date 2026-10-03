# 93 Worker ownership and bounded handoff

- Preserve all cumulative game/hash/Connection tools; add ThreadLink, not concurrent calls to Connection.
- Runtime outlives ThreadLink. Main transfers a connected Socket; worker alone owns it and the parser.
- ThreadLink noncopy/nonmove; worker declared last; public methods used by one controlling thread.
- Fixed FrameQueue<8> copies owning Frame values; mutex protects slots/head/count/closed together.
- try operations do not wait for items/space, but may wait for mutex. No lock-free/latency guarantee.
- close forbids pushes but drains accepted items. Caller frame must remain stable during copying.
- Worker keeps one partial outgoing EncodedFrame outside queue; total storage includes parser/OS buffers.
- Socket mode stays nonblocking. One send attempt, one parse or 16-byte receive per turn.
- Five-second cooperative send deadline starts at dequeue, not admission. Receive idle has no deadline.
- A 1ms idle sleep limits polling; scheduling can add delay. Startup accept/connect are synchronous.
- Incoming overflow aborts with receive_full, no silent drop. Caller handles outgoing full.
- Terminal report has separate mutex/optional; cannot be lost to full data queue. Drain and report checked separately.
- finish_sending drains then half-closes; complete needs peer EOF too. Cancellation may abandon data.
- Destructor requests stop and joins outside locks before members die. OS/thread/mutex system failures are not recovered.
- Tests use actual threads/loopback sockets; fixture hashes test transport, not simulation authority.
- Current root Session latest-hash storage and connect cancellation remain distinct unresolved contracts.
