#include "net/latency_summary.h"
#include "net/probe_frame.h"
#include "net/thread_link.h"
#include "net/process_usage.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while(false)
int main() {
    using namespace study_net;
    CHECK(!summarize_latency({}));
    CHECK(!summarize_latency({-1}));
    CHECK(!summarize_latency({std::numeric_limits<double>::infinity()}));
    CHECK(!summarize_latency({std::numeric_limits<double>::quiet_NaN()}));
    CHECK(summarize_latency({0})->p99_us == 0);
    const std::vector<double> unsorted{3, 1, 2};
    const auto copy = unsorted;
    auto a = summarize_latency(unsorted);
    CHECK(a && a->count == 3 && a->mean_us == 2 && a->p50_us == 2 && a->p99_us == 3);
    CHECK(unsorted == copy);
    std::vector<double> values;
    for (unsigned n = 1; n <= 201; ++n) {
        values.push_back(n);
        auto s = summarize_latency(values);
        CHECK(s && s->p50_us == (50 * n + 99) / 100);
        CHECK(s->p99_us == (99 * n + 99) / 100 && s->max_us == n);
    }
    values.assign(99, 1); values.push_back(1000);
    a = summarize_latency(values);
    CHECK(a->p99_us == 1 && a->max_us == 1000 && std::abs(a->mean_us - 10.99) < 1e-9);
    const double max = (std::numeric_limits<double>::max)();
    a = summarize_latency({max, max});
    CHECK(a && a->mean_us == max);
    auto frame = probe_frame(8, 15);
    CHECK(matches_probe(frame, 8, 15));
    CHECK(!matches_probe(frame, 9, 15) && !matches_probe(frame, 8, 14));
    frame.size = 7; CHECK(!matches_probe(frame, 8, 15));
    frame.size = 8; frame.type = 21; CHECK(!matches_probe(frame, 8, 15));
    for (int delay : {0, 11}) {
        bool rejected = false;
        try { ThreadLink link(Socket{}, false, {}, std::chrono::milliseconds(delay)); }
        catch (const std::invalid_argument&) { rejected = true; }
        CHECK(rejected);
    }
    const auto usage = process_usage();
    CHECK(usage && std::isfinite(usage->cpu_seconds) && usage->cpu_seconds >= 0);
    std::puts("Measurement contracts: ranks, empty/invalid/extreme samples, reply identity, poll bounds passed");
}
