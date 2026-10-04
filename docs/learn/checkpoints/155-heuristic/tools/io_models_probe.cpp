#include "net/posted_receive.h"
#include "net/probe_frame.h"
#include "net/stream.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>

using namespace study_net;
using Clock = std::chrono::steady_clock;
using Millis = std::chrono::milliseconds;
namespace {
void require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
struct Pair { Socket sender, receiver; };
Pair make_pair() {
    int error = 0;
    std::uint16_t port = 0;
    auto listener = listen_loopback(0, port, error);
    require(listener.valid(), "listen");
    auto sender = connect_loopback(port, error);
    require(sender.valid(), "connect");
    auto receiver = accept_one(listener, error);
    require(receiver.valid(), "accept");
    require(set_nonblocking(sender, true, error) && set_nonblocking(receiver, true, error), "nonblocking");
    return {std::move(sender), std::move(receiver)};
}
void send_bytes(Socket& socket, const std::uint8_t* bytes, std::size_t size) {
    const auto report = send_bounded(socket, bytes, size, Clock::now() + std::chrono::seconds(2));
    require(report.outcome == SendOutcome::complete, "send");
}
ReadCompletion collect(PostedReceiver& receiver) {
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    while (Clock::now() < deadline) {
        if (auto result = receiver.take()) return *result;
        std::this_thread::sleep_for(Millis(1));
    }
    throw std::runtime_error("collect deadline");
}
void readiness() {
    auto pair = make_pair();
    require(wait_readable(pair.receiver, 0).state == WaitState::timeout, "initial timeout");
    require(wait_readable(pair.receiver, -1).state == WaitState::error, "negative wait rejected");
    require(wait_readable(pair.receiver, 1001).state == WaitState::error, "large wait rejected");
    require(wait_readable(Socket{}, 0).state == WaitState::error, "invalid handle rejected");
    EncodedFrame encoded;
    require(encode_frame(probe_frame(9, 3), encoded), "frame");
    send_bytes(pair.sender, encoded.bytes.data(), 2); // Only the length header.
    const auto hint = wait_readable(pair.receiver, 1000);
    require(hint.state == WaitState::ready, "header hint");
    require(wait_readable(pair.receiver, 0).state == WaitState::ready, "hint did not consume");
    FrameParser parser;
    std::size_t consumed = 0;
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (consumed < 2) {
        require(Clock::now() < deadline, "header deadline");
        std::uint8_t bytes[2];
        const auto read = try_receive(pair.receiver, bytes, 2 - consumed);
        if (read.state == ReceiveState::would_block || read.state == ReceiveState::interrupted) {
            wait_readable(pair.receiver, 20); continue;
        }
        require(read.state == ReceiveState::progress && parser.append(bytes, read.count), "header read");
        consumed += read.count;
    }
    std::uint8_t scratch[16];
    require(try_receive(pair.receiver, scratch, sizeof(scratch)).state == ReceiveState::would_block,
            "stale hint does not reserve bytes");
    Frame frame;
    require(parser.next(frame) == ParseStatus::need_more, "header is not frame");
    std::puts("READINESS: notification -> read header -> stale hint yields would_block; parser needs body");
    send_bytes(pair.sender, encoded.bytes.data() + 2, encoded.size - 2);
    int error = 0; require(shutdown_send(pair.sender, error), "half close");
    bool got_frame = false, eof = false;
    while (!eof) {
        require(Clock::now() < deadline, "readiness deadline");
        const auto ready = wait_readable(pair.receiver, 20);
        require(ready.state != WaitState::error, "wait");
        if (ready.state != WaitState::ready) continue;
        const auto read = try_receive(pair.receiver, scratch, sizeof(scratch));
        if (read.state == ReceiveState::progress) {
            require(parser.append(scratch, read.count), "append");
            ParseStatus parsed;
            while ((parsed = parser.next(frame)) == ParseStatus::frame) {
                require(!got_frame && matches_probe(frame, 9, 3), "complete frame");
                got_frame = true;
            }
            require(parsed == ParseStatus::need_more, "parser error");
        } else if (read.state == ReceiveState::eof) eof = true;
        else require(read.state != ReceiveState::error, "receive");
    }
    require(got_frame && parser.pending_bytes() == 0, "data before EOF");
    std::puts("READINESS: buffered body consumed before EOF; one validated application frame");
}
void completion() {
    auto pair = make_pair();
    PostedReceiver receiver(std::move(pair.receiver));
    require(!receiver.post(0, 0, Millis(100)), "zero capacity rejected");
    require(!receiver.post(0, 17, Millis(100)), "large capacity rejected");
    require(!receiver.post(0, 1, Millis(0)), "zero timeout rejected");
    require(!receiver.post(0, 1, Millis(5001)), "large timeout rejected");
    require(receiver.post(41, 2, Millis(1000)), "post header");
    require(!receiver.post(42, 2, Millis(1000)), "overlap rejected");
    EncodedFrame encoded;
    require(encode_frame(probe_frame(9, 3), encoded), "encode");
    send_bytes(pair.sender, encoded.bytes.data(), 2);
    auto first = collect(receiver);
    require(first.id == 41 && first.state == CompletionState::data && first.count > 0 && first.count <= 2,
            "first completion");
    require(!receiver.take(), "one collection");
    FrameParser parser;
    require(parser.append(first.bytes.data(), first.count), "completion append");
    Frame frame;
    require(parser.next(frame) == ParseStatus::need_more, "I/O completion != frame completion");
    const auto saved = first.bytes;
    send_bytes(pair.sender, encoded.bytes.data() + 2, encoded.size - 2);
    int error = 0; require(shutdown_send(pair.sender, error), "half close");
    bool got_frame = false, eof = false;
    unsigned operations = 0;
    for (std::uint64_t id = 42; !eof && operations < 16; ++id, ++operations) {
        require(receiver.post(id, 16, Millis(1000)), "post remainder");
        const auto result = collect(receiver);
        require(result.id == id, "completion identity");
        if (result.state == CompletionState::data) {
            require(parser.append(result.bytes.data(), result.count), "append remainder");
            ParseStatus parsed;
            while ((parsed = parser.next(frame)) == ParseStatus::frame) {
                require(!got_frame && matches_probe(frame, 9, 3), "completion frame");
                got_frame = true;
            }
            require(parsed == ParseStatus::need_more, "parse state");
        } else { require(result.state == CompletionState::eof, "completion eof"); eof = true; }
    }
    require(got_frame && eof && parser.pending_bytes() == 0 && first.bytes == saved,
            "independent result ownership");
    std::puts("COMPLETION: owned buffer -> submit -> collect bytes/status/id; first completion is only part of frame");
    std::puts("COMPLETION: one pending operation; resubmission preserves previously collected bytes; EOF collected");
}
void cancellation() {
    auto pair = make_pair();
    PostedReceiver receiver(std::move(pair.receiver));
    require(receiver.post(1, 16, Millis(1000)), "post cancelled");
    receiver.request_cancel();
    const auto cancelled = collect(receiver);
    require(cancelled.id == 1 && cancelled.state == CompletionState::cancelled && cancelled.count == 0,
            "cancel collected");
    require(receiver.post(2, 16, Millis(10)), "post timeout");
    const auto timed = collect(receiver);
    require(timed.id == 2 && timed.state == CompletionState::timeout && timed.count == 0, "timeout");
    const std::uint8_t byte = 77;
    send_bytes(pair.sender, &byte, 1);
    require(receiver.post(3, 1, Millis(1000)), "post data");
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (!receiver.ready() && Clock::now() < deadline) std::this_thread::sleep_for(Millis(1));
    require(receiver.ready(), "completion ready");
    require(!receiver.post(4, 1, Millis(1000)), "uncollected result remains pending");
    receiver.request_cancel(); // Already completed: does not rewrite its result.
    const auto completed = collect(receiver);
    require(completed.state == CompletionState::data && completed.bytes[0] == 77, "completion wins");
    std::puts("LIFETIME: cancel requested then collected; timeout separate; completed result survives late cancellation");
    { auto second = make_pair(); PostedReceiver pending(std::move(second.receiver));
      require(pending.post(9, 16, Millis(5000)), "destructor pending"); }
    std::puts("LIFETIME: destructor requests cancellation and waits before socket/buffer destruction");
}
void truncated() {
    auto pair = make_pair();
    PostedReceiver receiver(std::move(pair.receiver));
    EncodedFrame encoded;
    require(encode_frame(probe_frame(9, 3), encoded), "encode truncated");
    send_bytes(pair.sender, encoded.bytes.data(), 2);
    int error = 0; require(shutdown_send(pair.sender, error), "truncated half close");
    FrameParser parser;
    bool eof = false;
    for (std::uint64_t id = 1; id <= 4 && !eof; ++id) {
        require(receiver.post(id, 16, Millis(1000)), "truncated post");
        const auto result = collect(receiver);
        if (result.state == CompletionState::data) {
            require(parser.append(result.bytes.data(), result.count), "truncated append");
            Frame frame;
            require(parser.next(frame) == ParseStatus::need_more, "truncated parser");
        } else { require(result.state == CompletionState::eof, "truncated eof"); eof = true; }
    }
    require(eof && parser.pending_bytes() == 2, "EOF still requires protocol interpretation");
    std::puts("PROTOCOL: successful data completion followed by EOF can still leave a truncated frame");
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::puts("io_models_probe (loopback readiness and worker-backed completion study)"); return 0;
    }
    if (argc != 1) return 2;
    Runtime runtime;
    if (!runtime.ready()) return 3;
    try { readiness(); completion(); cancellation(); truncated(); }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
