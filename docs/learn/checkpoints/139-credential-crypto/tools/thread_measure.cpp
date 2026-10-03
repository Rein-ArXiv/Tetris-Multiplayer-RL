#include "net/thread_link.h"
#include "net/probe_frame.h"
#include "net/latency_summary.h"
#include "net/process_usage.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace study_net;
using Clock = std::chrono::steady_clock;
using Millis = std::chrono::milliseconds;
struct Pair {
    std::unique_ptr<ThreadLink> client, echo;
};
struct Measurement {
    std::size_t started = 0;
    std::vector<double> samples_us;
    const char* status = "ok";
};

// Main is the sole controller of every link. At most one request per pair.
Measurement measure(std::vector<Pair>& pairs, unsigned rounds, unsigned first_round,
                    const char* mode) {
    Measurement result;
    result.samples_us.reserve(pairs.size() * rounds);
    std::vector<Clock::time_point> sent(pairs.size());
    std::vector<bool> done(pairs.size()), echoed(pairs.size());
    const auto total_deadline = Clock::now() + std::chrono::seconds(60);
    for (unsigned r = 0; r < rounds; ++r) {
        if (Clock::now() >= total_deadline) {
            result.status = "total_timeout"; return result;
        }
        std::fill(done.begin(), done.end(), false);
        std::fill(echoed.begin(), echoed.end(), false);
        const auto deadline = Clock::now() + Millis(500);
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            const auto frame = probe_frame(first_round + r, static_cast<std::uint32_t>(i));
            sent[i] = Clock::now(); // Before enqueue: includes local queue waiting.
            ++result.started;
            if (pairs[i].client->send(frame) != QueuePut::stored) {
                result.status = "enqueue_failed"; return result;
            }
        }
        std::size_t pending = pairs.size();
        while (pending != 0) {
            if (Clock::now() >= deadline || Clock::now() >= total_deadline) {
                result.status = "timeout"; return result;
            }
            for (std::size_t i = 0; i < pairs.size(); ++i) {
                auto& pair = pairs[i];
                if (pair.client->report() || pair.echo->report()) {
                    result.status = "link_ended"; return result;
                }
                Frame frame;
                if (pair.echo->receive(frame) == QueueGet::item) {
                    if (echoed[i] || !matches_probe(frame, first_round + r,
                                                   static_cast<std::uint32_t>(i))) {
                        result.status = "bad_request"; return result;
                    }
                    echoed[i] = true;
                    if (std::strcmp(mode, "drop") != 0) {
                        if (std::strcmp(mode, "corrupt") == 0) frame.payload[0] ^= 1;
                        if (pair.echo->send(frame) != QueuePut::stored) {
                            result.status = "echo_enqueue_failed"; return result;
                        }
                    }
                }
                if (pair.client->receive(frame) == QueueGet::item) {
                    if (done[i] || !matches_probe(frame, first_round + r,
                                                 static_cast<std::uint32_t>(i))) {
                        result.status = "bad_reply"; return result;
                    }
                    const auto received = Clock::now();
                    if (received >= deadline || received >= total_deadline) {
                        result.status = "timeout"; return result;
                    }
                    result.samples_us.push_back(
                        std::chrono::duration<double, std::micro>(received - sent[i]).count());
                    done[i] = true;
                    --pending;
                }
            }
            // This coordinator delay is part of application-observed latency.
            if (pending) std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    return result;
}

struct Window {
    Clock::time_point wall = Clock::now();
    std::optional<ProcessUsage> usage = process_usage();
};
void print_window(const char* name, const Window& begin, const Window& end) {
    const double seconds = std::chrono::duration<double>(end.wall - begin.wall).count();
    std::cout << '"' << name << "\":{\"wall_seconds\":" << seconds
              << ",\"cpu_seconds\":";
    if (begin.usage && end.usage) {
        const double cpu = end.usage->cpu_seconds - begin.usage->cpu_seconds;
        std::cout << cpu << ",\"cpu_core_equivalents\":" << cpu / seconds;
    } else std::cout << "null,\"cpu_core_equivalents\":null";
    const auto counter = [&](const char* key, auto member) {
        std::cout << ",\"" << key << "\":";
        if (begin.usage && end.usage && ((*begin.usage).*member) && ((*end.usage).*member))
            std::cout << *((*end.usage).*member) - *((*begin.usage).*member);
        else std::cout << "null";
    };
    counter("voluntary_switches", &ProcessUsage::voluntary_switches);
    counter("involuntary_switches", &ProcessUsage::involuntary_switches);
    std::cout << '}';
}
unsigned parse(const char* s, unsigned low, unsigned high) {
    unsigned value = 0;
    const auto end = s + std::strlen(s);
    const auto r = std::from_chars(s, end, value);
    if (r.ec != std::errc{} || r.ptr != end || value < low || value > high)
        throw std::invalid_argument("argument range");
    return value;
}
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::cout << "thread_measure PAIRS(1..16) ROUNDS(1..1000) POLL_MS(1..10) [echo|drop|corrupt]\n";
        return 0;
    }
    if (argc != 4 && argc != 5) return 2;
    try {
        const unsigned count = parse(argv[1], 1, 16), rounds = parse(argv[2], 1, 1000);
        const unsigned poll_ms = parse(argv[3], 1, 10);
        const char* mode = argc == 5 ? argv[4] : "echo";
        if (std::strcmp(mode, "echo") && std::strcmp(mode, "drop") && std::strcmp(mode, "corrupt"))
            return 2;
        Runtime runtime;
        if (!runtime.ready()) throw std::runtime_error("runtime");
        std::vector<Pair> pairs; // Destroyed (joins) before Runtime.
        pairs.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            int error = 0;
            std::uint16_t port = 0;
            auto listener = listen_loopback(0, port, error);
            if (!listener.valid()) throw std::runtime_error("listen");
            auto client = connect_loopback(port, error);
            if (!client.valid()) throw std::runtime_error("connect");
            auto echo = accept_one(listener, error);
            if (!echo.valid()) throw std::runtime_error("accept");
            Pair pair;
            pair.client = std::make_unique<ThreadLink>(std::move(client), false, BeatTiming{}, Millis(poll_ms));
            pair.echo = std::make_unique<ThreadLink>(std::move(echo), false, BeatTiming{}, Millis(poll_ms));
            pairs.push_back(std::move(pair));
        }
        // No application traffic; the main thread sleeps while all links poll.
        const Window idle_begin;
        std::this_thread::sleep_for(Millis(200));
        const Window idle_end;
        const auto warmup = measure(pairs, 8, 0, "echo");
        if (std::strcmp(warmup.status, "ok")) throw std::runtime_error("warmup");
        const Window active_begin;
        const auto result = measure(pairs, rounds, 8, mode);
        const Window active_end;
        const auto summary = summarize_latency(result.samples_us);
        for (auto& pair : pairs) { pair.client->request_stop(); pair.echo->request_stop(); }
        pairs.clear(); // Cleanup outside the measured windows; join every worker.
        std::cout.imbue(std::locale::classic());
        std::cout << std::setprecision(12)
                  << "{\"scope\":\"one_process_loopback_echo\",\"pairs\":" << count
                  << ",\"workers\":" << 2 * count << ",\"rounds\":" << rounds
                  << ",\"poll_ms\":" << poll_ms << ",\"warmup_rounds\":8,\"frame_bytes\":11"
                  << ",\"status\":\"" << result.status << "\",\"planned\":" << count * rounds
                  << ",\"started\":" << result.started << ",\"completed\":" << result.samples_us.size()
                  << ",\"incomplete\":" << result.started - result.samples_us.size()
                  << ",\"unattempted\":" << count * rounds - result.started << ',';
        print_window("idle", idle_begin, idle_end); std::cout << ',';
        print_window("active", active_begin, active_end);
        const double wall = std::chrono::duration<double>(active_end.wall - active_begin.wall).count();
        std::cout << ",\"completed_per_second\":" << result.samples_us.size() / wall
                  << ",\"latency\":";
        if (summary) std::cout << "{\"count\":" << summary->count << ",\"mean_us\":" << summary->mean_us
            << ",\"p50_us\":" << summary->p50_us << ",\"p99_us\":" << summary->p99_us
            << ",\"max_us\":" << summary->max_us << '}';
        else std::cout << "null";
        std::cout << ",\"samples_us\":[";
        for (std::size_t i = 0; i < result.samples_us.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << result.samples_us[i];
        }
        std::cout << "]}\n";
        return std::strcmp(result.status, "ok") ? 1 : 0;
    } catch (const std::invalid_argument&) { std::cerr << "Invalid measurement arguments\n"; return 2; }
      catch (const std::exception&) { std::cerr << "Measurement setup/warmup failed\n"; return 3; }
}
