#ifndef STUDY_NET_FLOW_QUEUE_H
#define STUDY_NET_FLOW_QUEUE_H

// Byte-bounded flow-control queue for decoded application frames.
//
// Purpose
// -------
// FlowQueue adds wire-backlog accounting and pause/resume admission to the
// FrameQueue ring. It sits between an application producer and a single transport
// worker: the worker takes exactly one frame at a time, and the queue keeps
// the remaining frames until the worker reports that the transport accepted
// them.
//
// Design notes
// ------------
// * Storage is a fixed ring of `Capacity` Frame slots, allocated inline in the
//   object. Nothing allocates or grows at run time, so memory use is bounded
//   and independent of how long the queue runs. The whole array exists even
//   when the queue is empty.
// * Accounting is wire backlog, not exact RSS. Each frame is charged
//   `frame.size + 3` bytes (the 2-byte length header plus the type byte plus
//   the payload), matching the encoded size of a frame. The running byte total
//   covers the frames still queued plus, when one is checked out, the single
//   active frame. It is an admission heuristic, not a measurement of resident
//   memory.
// * `bytes` counts the queued frames and the one active worker frame. It is
//   never decremented by `try_pop`; the charge transfers to `active_bytes` and
//   is released by `complete()`. This is why a checked-out frame still counts
//   against `ByteLimit`.
// * The frame-count limit includes the active frame: occupancy is `count + 1`
//   whenever `active_bytes != 0`. The array slot for the active frame is free,
//   but it is not offered to a producer, so at most `Capacity` frames are ever
//   in flight.
// * Pause is a hysteresis band. Crossing `High` on a store sets `paused` and
//   refuses further pushes; the queue resumes only once a `complete()` brings
//   the accounted bytes down to `Low` or below. A paused queue reports `full`
//   to producers rather than blocking them.
// * `try_push`/`try_pop` never wait for queue room or queue data. They can
//   still block while acquiring the mutex, which is ordinary mutual exclusion,
//   not a wait for capacity. This is not a lock-free design and must not be
//   described as one.
// * Every read or write of a queue member happens with `mutex_` held, so the
//   ring indices, the counters, the pause/closed flags and the slot contents
//   always change together as one consistent shape.
// * No user callback, socket call or other I/O ever runs while the mutex is
//   held. The lock is released before the caller regains control.
// * `close()` marks the queue closed without discarding anything already
//   stored. A worker may drain the remaining frames and only then observes
//   QueueGet::closed. Closing is idempotent.
// * There is no reset or discard operation: a queue is created for one
//   connection and retired with it, so partially delivered state can never be
//   reused by mistake.
// * Frame owns its payload inline (an array, not a borrowed pointer), so a
//   queued copy stays valid for as long as it sits in the queue.
// * No pointers to internal storage are exposed; `stats()` returns a plain
//   value snapshot.
//
// A mutex member makes the type non-copyable and non-movable; both are
// deleted explicitly rather than left implicit. Methods are intentionally not
// `noexcept`: locking a mutex may throw (for example std::system_error).

#include <array>
#include <cstddef>
#include <mutex>

#include "net/frame_queue.h"  // Frame, QueuePut, QueueGet

namespace study_net {

// Consistent snapshot of a FlowQueue, for diagnostics only.
// The values are read under the queue mutex, so they describe one instant, but
// they are not an admission guarantee: by the time a caller reads them another
// thread may already have pushed, popped or completed a frame.
struct FlowStats {
  std::size_t queued{};        // frames in the ring, not counting active
  std::size_t bytes{};         // wire bytes queued plus active
  std::size_t active_bytes{};  // wire bytes of the checked-out frame, else 0
  bool paused{};               // producers refused until complete() <= Low
  bool closed{};               // close() has been called
};

// Bounded, thread-safe flow-control queue.
//
//   Capacity  : number of Frame slots in the ring; must be positive.
//   ByteLimit : maximum wire bytes admitted (queued + active).
//   High      : crossing this many bytes on a push sets `paused`.
//   Low       : completing a frame down to this many bytes clears `paused`.
//               Requires 0 < Low < High <= ByteLimit.
template <std::size_t Capacity, std::size_t ByteLimit, std::size_t High,
          std::size_t Low>
class FlowQueue {
  static_assert(Capacity > 0, "FlowQueue needs a positive capacity");
  static_assert(ByteLimit >= kMaxFrameBytes,
                "ByteLimit must hold at least one maximum frame");
  static_assert(Low > 0 && Low < High && High <= ByteLimit,
                "FlowQueue needs 0 < Low < High <= ByteLimit");

 public:
  FlowQueue() = default;

  FlowQueue(const FlowQueue&) = delete;
  FlowQueue& operator=(const FlowQueue&) = delete;
  FlowQueue(FlowQueue&&) = delete;
  FlowQueue& operator=(FlowQueue&&) = delete;

  // Copy `frame` to the back of the queue.
  // The payload size is validated before the mutex is taken, so an invalid
  // frame never disturbs the queue. A full, paused or closed queue is returned
  // as such and left exactly as it was, and `frame` is not modified.
  //
  // `full` covers three separate admission failures: the ring has no free
  // non-active slot, the byte budget cannot take this frame, or the queue is
  // paused above `High` and waiting for `complete()`.
  QueuePut try_push(const Frame& frame) {
    if (frame.size > kMaxPayloadBytes) {
      return QueuePut::invalid;
    }
    std::lock_guard<std::mutex> guard(mutex_);
    if (closed_) {
      return QueuePut::closed;
    }
    const std::size_t cost = frame.size + kHeaderBytes + 1u;
    const std::size_t occupancy = count_ + (active_bytes_ != 0 ? 1u : 0u);
    if (paused_ || occupancy >= Capacity || cost > ByteLimit - bytes_) {
      return QueuePut::full;
    }
    buffer_[(head_ + count_) % Capacity] = frame;
    ++count_;
    bytes_ += cost;
    if (bytes_ >= High) {
      paused_ = true;
    }
    return QueuePut::stored;
  }

  // Check out the oldest frame into `out`.
  // At most one frame is in flight: while a previous frame is active
  // (`active_bytes_ != 0`) this reports empty so the caller cannot lose track
  // of what it still has to complete. On empty or closed, `out` is not written
  // at all, so a rejected pop never clobbers the caller's variable.
  //
  // The frame's wire bytes stay in `bytes_` and are mirrored into
  // `active_bytes_`; they are released only by complete().
  QueueGet try_pop(Frame& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (active_bytes_ != 0) {
      return QueueGet::empty;
    }
    if (count_ == 0) {
      return closed_ ? QueueGet::closed : QueueGet::empty;
    }
    out = buffer_[head_];
    head_ = (head_ + 1) % Capacity;
    --count_;
    active_bytes_ = out.size + kHeaderBytes + 1u;
    return QueueGet::item;
  }

  // Report that the active frame has been handed to the transport.
  // "Completed" means every wire byte was accepted by the transport, not that
  // the peer has applied it. Releases the active frame's bytes and may clear
  // `paused` once the accounted bytes fall to `Low` or below. Returns false
  // when no frame is active. The single consumer must complete its current
  // frame exactly once; this API does not validate a separate lease identifier.
  bool complete() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (active_bytes_ == 0) {
      return false;
    }
    bytes_ -= active_bytes_;
    active_bytes_ = 0;
    if (bytes_ <= Low) {
      paused_ = false;
    }
    return true;
  }

  // Mark the queue closed. Stored frames are kept so a worker can drain them;
  // only afterwards do pops report QueueGet::closed. Calling close() again is
  // harmless.
  void close() {
    std::lock_guard<std::mutex> guard(mutex_);
    closed_ = true;
  }

  // Snapshot of the current accounting. Diagnostic only: it is internally
  // consistent, but it is not an admission promise, since the queue may change
  // immediately after it returns.
  FlowStats stats() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return FlowStats{count_, bytes_, active_bytes_, paused_, closed_};
  }

 private:
  mutable std::mutex mutex_;
  std::array<Frame, Capacity> buffer_{};
  std::size_t head_{0};          // index of the oldest queued frame
  std::size_t count_{0};         // queued frames, not counting the active one
  bool closed_{false};           // set by close(), never cleared
  bool paused_{false};           // set by push at High, cleared at Low
  std::size_t bytes_{0};         // wire bytes queued plus active
  std::size_t active_bytes_{0};  // wire bytes of the checked-out frame
};

}  // namespace study_net

#endif  // STUDY_NET_FLOW_QUEUE_H
