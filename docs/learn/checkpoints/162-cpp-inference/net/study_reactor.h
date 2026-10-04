#ifndef STUDY_NET_STUDY_REACTOR_H
#define STUDY_NET_STUDY_REACTOR_H
#include "net/registrations.h"
#include <memory>
#include <vector>
namespace study_net {
struct ReadyEvent {
    Registration id = 0;
    bool readable = false, writable = false, error = false;
};
enum class PollState { events, timeout, interrupted, error };
struct ReadyBatch {
    PollState state = PollState::timeout;
    std::vector<ReadyEvent> events;
    bool woken = false;
    int error = 0;
};
// Single owner for watch/change/unwatch/poll/current. Borrows registered sockets:
// remove before closing. IDs belong to this reactor instance, not to a raw fd.
// Only wake() permits other threads; join wake callers before destroying reactor.
// No async-signal-handler guarantee. Runtime outlives reactor and sockets.
class StudyReactor {
 public:
    virtual ~StudyReactor() = default;
    virtual std::optional<Registration> watch(const Socket&, unsigned interest) = 0;
    virtual bool change(Registration, unsigned interest) = 0;
    virtual bool unwatch(Registration) = 0;
    virtual bool current(Registration) const = 0;
    virtual ReadyBatch poll(int timeout_ms) = 0; // 0..1000ms, requested wait not scheduling bound.
    virtual bool wake() noexcept = 0; // Notification only; authoritative work lives elsewhere.
};
std::unique_ptr<StudyReactor> make_poll_reactor();
} // namespace study_net
#endif
