#pragma once
#include "net/first_admission.h"
#include <cstdint>
namespace study_net {
// One owner calls all methods. Socket, timer and worker ownership stay outside.
class AdmissionSession {
public:
    enum class Stage { first_frame, auth, lobby, forward, closed };
    enum class Effect { none, authenticate, ready, game, closed };
    explicit AdmissionSession(std::uint64_t id) : id_(id) {}
    Stage stage() const noexcept { return stage_; }
    std::uint64_t id() const noexcept { return id_; }
    bool ready() const noexcept { return ready_; }
    bool can_read() const noexcept {
        return stage_ == Stage::first_frame || stage_ == Stage::forward ||
               (stage_ == Stage::lobby && !ready_);
    }
    const AdmissionRequest& request() const noexcept { return request_; }
    std::size_t pending_bytes() const noexcept { return parser_.pending_bytes(); }
    void close() noexcept { stage_ = Stage::closed; ready_ = false; }
    bool append(const std::uint8_t* bytes, std::size_t count) noexcept {
        if (!can_read()) return false;
        if (!parser_.append(bytes, count)) { close(); return false; }
        return true;
    }
    // Exactly one auth request per session. ID + expected stage identify it.
    bool resume_auth(std::uint64_t id, bool accepted) noexcept {
        if (id != id_ || stage_ != Stage::auth) return false;
        if (!accepted) { close(); return true; }
        stage_ = Stage::lobby;
        return true;
    }
    bool begin_forward() noexcept {
        if (stage_ != Stage::lobby || !ready_) return false;
        ready_ = false;
        stage_ = Stage::forward;
        return true;
    }
    Effect step(Frame& frame) noexcept {
        if (stage_ == Stage::closed) return Effect::closed;
        if (!can_read()) return Effect::none;
        const auto parsed = parser_.next(frame);
        if (parsed == ParseStatus::need_more) return Effect::none;
        if (parsed == ParseStatus::error) { close(); return Effect::closed; }
        switch (stage_) {
        case Stage::first_frame:
            // This vertical study pairs two queue entrants; rooms remain a separate path.
            if (!decode_admission(frame, request_) || request_.route != AdmissionRoute::queue) {
                close(); return Effect::closed;
            }
            stage_ = Stage::auth;
            return Effect::authenticate;
        case Stage::lobby:
            if (frame.type != 17 || frame.size != 1 || frame.payload[0] != 1) {
                close(); return Effect::closed;
            }
            ready_ = true;
            return Effect::ready;
        case Stage::forward:
            if (frame.type != 60 || frame.size != 8) { close(); return Effect::closed; }
            return Effect::game;
        default: return Effect::none;
        }
    }
private:
    const std::uint64_t id_;
    Stage stage_ = Stage::first_frame;
    bool ready_ = false;
    AdmissionRequest request_{};
    FrameParser parser_{}; // Unconsumed bytes survive every phase transition.
};
} // namespace study_net
