#pragma once
#include "renderer/color_batch.h"
#include "renderer/clip_box.h"
#include <cstdint>
#include <limits>
namespace study_flush {
struct Statistics { std::uint64_t draws = 0, vertices = 0; };
// Sink supplies bool set_clip(Clip) and bool submit(const Batch&), both noexcept.
// No other rendering state may change until this frame's stream is finished.
// CPU input validation is atomic; successful earlier flushes cannot be undone.
template<class Sink>
class Stream {
public:
    Stream(Sink& sink, Clip clip) noexcept : sink_(sink), clip_(clip), good_(valid(clip)) {}
    Stream(const Stream&)=delete;
    Stream& operator=(const Stream&)=delete;
    ~Stream() = default; // No hidden GL calls: callers must explicitly finish.
    bool good() const noexcept { return good_; }
    std::size_t pending() const noexcept { return pending_.size(); }
    Statistics statistics() const noexcept { return stats_; }
    bool set_clip(Clip next) noexcept {
        if (!good_ || closed_) return false;
        if (!valid(next)) return fail();
        if (next == clip_) return true;
        if (!flush()) return false; // Submit with the OLD clip first.
        clip_ = next;
        return true;
    }
    bool append(const study_mesh::Vertex2* points, std::size_t count,
                study_batch::Color color) noexcept {
        if (!good_ || closed_) return false;
        study_batch::Batch incoming;
        if (!incoming.append(points,count,color)) return fail();
        if (incoming.size() > study_batch::Batch::capacity - pending_.size())
            if (!flush()) return false;
        if (!pending_.append_batch(incoming)) return fail();
        return true;
    }
    bool flush() noexcept {
        if (!good_ || closed_) return false;
        if (pending_.size()==0) return true;
        if (stats_.draws == std::numeric_limits<std::uint64_t>::max() ||
            pending_.size() > std::numeric_limits<std::uint64_t>::max()-stats_.vertices)
            return fail();
        if (!sink_.set_clip(clip_) || !sink_.submit(pending_)) return fail();
        ++stats_.draws;
        stats_.vertices += pending_.size();
        pending_.clear();
        return true;
    }
    bool finish() noexcept {
        if (closed_) return good_;
        if (!flush()) return false;
        closed_ = true;
        return true;
    }
private:
    bool fail() noexcept { good_=false; return false; }
    Sink& sink_;
    study_batch::Batch pending_;
    Clip clip_;
    Statistics stats_;
    bool good_ = true, closed_ = false;
};
} // namespace study_flush
