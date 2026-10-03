#ifndef STUDY_NET_READ_HINT_H
#define STUDY_NET_READ_HINT_H
#include "net/socket.h"
namespace study_net {
enum class WaitState { ready, timeout, interrupted, error };
struct ReadHint {
    WaitState state;
    bool readable = false;
    bool hangup = false;
    bool fault = false;
    int error = 0;
};
// One poll/WSAPoll call for a connected stream socket, borrowed exclusively.
// timeout_ms must be 0..1000. Does not receive data or change socket mode.
// A ready result may indicate data, EOF or error; try_receive decides the read result.
ReadHint wait_readable(const Socket& socket, int timeout_ms) noexcept;
} // namespace study_net
#endif
