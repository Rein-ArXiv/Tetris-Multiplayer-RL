#ifndef STUDY_NET_FRAME_QUEUE_H
#define STUDY_NET_FRAME_QUEUE_H

// Bounded, thread-safe hand-off queue for decoded application frames.
//
// Design notes
// ------------
// * Storage is a fixed-size ring buffer: `Capacity` Frame slots, allocated
//   inline in the object. Nothing allocates or grows at run time, so memory
//   use is bounded and independent of how long the queue runs.
// * `try_push`/`try_pop` never wait for queue room or queue data. They can
//   still block while acquiring the mutex, which is ordinary mutual
//   exclusion, not a wait for capacity. This is not a lock-free design and
//   must not be described as one.
// * Every read or write of a queue member happens with `mutex_` held, so the
//   ring indices, the count, the closed flag and the slot contents always
//   change together as one consistent shape.
// * No user callback, socket call or other I/O ever runs while the mutex is
//   held. The lock is released before the caller regains control.
// * `close()` marks the queue closed without discarding anything already
//   stored. A reader may drain the remaining frames and only then observes
//   QueueGet::closed. Closing is idempotent.
// * Frame owns its payload inline (an array, not a borrowed pointer), so a
//   queued copy stays valid for as long as it sits in the queue.
//
// A mutex member makes the type non-copyable and non-movable; both are
// deleted explicitly rather than left implicit. Methods are intentionally not
// `noexcept`: locking a mutex may throw (for example std::system_error).

#include <array>
#include <cstddef>
#include <mutex>

#include "net/framing.h"

namespace study_net {

// Outcome of a push attempt.
//   stored  : the frame was copied into the queue.
//   full    : the queue is at capacity; nothing was stored.
//   closed  : the queue has been closed; nothing was stored.
//   invalid : the frame payload exceeds kMaxPayloadBytes; nothing was stored.
enum class QueuePut { stored, full, closed, invalid };

// Outcome of a pop attempt.
//   item  : `out` now holds the oldest queued frame.
//   empty : the queue holds no frames and is still open; `out` is untouched.
//   closed: the queue holds no frames and has been closed; `out` is untouched.
enum class QueueGet { item, empty, closed };

template <std::size_t Capacity>
class FrameQueue {
  static_assert(Capacity > 0, "FrameQueue needs a positive capacity");

 public:
  FrameQueue() = default;

  FrameQueue(const FrameQueue&) = delete;
  FrameQueue& operator=(const FrameQueue&) = delete;
  FrameQueue(FrameQueue&&) = delete;
  FrameQueue& operator=(FrameQueue&&) = delete;

  // Copy `frame` to the back of the queue.
  // The payload size is validated before the mutex is taken, so an invalid
  // frame never disturbs the queue. A full or closed queue is returned as such
  // and left exactly as it was.
  QueuePut try_push(const Frame& frame) {
    if (frame.size > kMaxPayloadBytes) {
      return QueuePut::invalid;
    }
    std::lock_guard<std::mutex> guard(mutex_);
    if (closed_) {
      return QueuePut::closed;
    }
    if (count_ == Capacity) {
      return QueuePut::full;
    }
    buffer_[(head_ + count_) % Capacity] = frame;
    ++count_;
    return QueuePut::stored;
  }

  // Copy the oldest frame into `out` and remove it from the queue.
  // On empty or closed, `out` is not written at all, so a rejected pop never
  // clobbers the caller's variable.
  QueueGet try_pop(Frame& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (count_ == 0) {
      return closed_ ? QueueGet::closed : QueueGet::empty;
    }
    out = buffer_[head_];
    head_ = (head_ + 1) % Capacity;
    --count_;
    return QueueGet::item;
  }

  // Mark the queue closed. Stored frames are kept so a reader can drain them;
  // only afterwards do pops report QueueGet::closed. Calling close() again is
  // harmless.
  void close() {
    std::lock_guard<std::mutex> guard(mutex_);
    closed_ = true;
  }

 private:
  std::mutex mutex_;
  std::array<Frame, Capacity> buffer_{};
  std::size_t head_{0};   // index of the oldest queued frame
  std::size_t count_{0};  // number of queued frames, 0..Capacity
  bool closed_{false};    // set by close(), never cleared
};

}  // namespace study_net

#endif  // STUDY_NET_FRAME_QUEUE_H
