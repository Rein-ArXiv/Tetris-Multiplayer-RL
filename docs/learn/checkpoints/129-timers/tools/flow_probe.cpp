#include "net/thread_link.h"
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {
bool number(const char* text, unsigned& out) {
    const auto end = text + std::strlen(text);
    const auto parsed = std::from_chars(text, end, out);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}
}

int main(int argc, char** argv) {
    using namespace study_net;
    using Clock = std::chrono::steady_clock;
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::puts("flow_probe PORT COUNT");
        return 0;
    }
    unsigned port = 0, count = 0;
    if (argc != 3 || !number(argv[1], port) || !number(argv[2], count) ||
        port == 0 || port > 65535 || count == 0 || count > 10000000) return 2;

    Runtime runtime;
    if (!runtime.ready()) return 1;
    int error = 0;
    auto socket = connect_loopback(static_cast<std::uint16_t>(port), error);
    if (!socket.valid()) return 1;
    ThreadLink link(std::move(socket));
    unsigned accepted = 0, retries = 0;
    bool finished = false;
    const auto limit = Clock::now() + std::chrono::seconds(10);
    auto admission_deadline = Clock::now() + std::chrono::milliseconds(250);
    while (Clock::now() < limit) {
        if (auto report = link.report()) {
            std::printf("END %d accepted=%u retries=%u bytes=%zu\n",
                        int(report->end), accepted, retries, link.send_stats().bytes);
            return finished && report->end == LinkEnd::complete &&
                   link.send_stats().bytes == 0 ? 0 : 1;
        }
        if (accepted < count) {
            Frame frame;
            frame.type = 40;
            frame.size = 32;
            frame.payload.fill(0x5a);
            for (unsigned i = 0; i < 4; ++i)
                frame.payload[i] = static_cast<std::uint8_t>(accepted >> (i * 8));
            const auto sent = link.send(frame);
            if (sent == QueuePut::stored) {
                ++accepted; // Advance only after admission: full retries this record.
                admission_deadline = Clock::now() + std::chrono::milliseconds(250);
            } else if (sent == QueuePut::full) {
                ++retries;
                if (Clock::now() >= admission_deadline) {
                    std::printf("ADMISSION_TIMEOUT accepted=%u bytes=%zu\n",
                                accepted, link.send_stats().bytes);
                    return 1;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                return 1;
            }
        } else if (!finished) {
            link.finish_sending();
            finished = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    std::puts("TOTAL_TIMEOUT");
    return 1;
}
