#ifndef STUDY_NET_LATENCY_SUMMARY_H
#define STUDY_NET_LATENCY_SUMMARY_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace study_net {

struct LatencySummary {
    std::size_t count;
    double mean_us;
    double p50_us;
    double p99_us;
    double max_us;
};

// Summarizes per-sample latencies in microseconds.
//
// Percentiles use the nearest-rank estimator: a 1-based rank of
// ceil(p * N / 100) is mapped to the zero-based order statistic rank - 1.
// This is a simple order-statistic definition of the p-th percentile; it is
// not a confidence interval or any inferential statement about a population.
//
// Samples are taken by value, so the caller's vector (and its order) is never
// modified. Returns std::nullopt for an empty input or when any sample is
// negative or non-finite. Zero-valued samples are valid.
inline std::optional<LatencySummary> summarize_latency(std::vector<double> samples) {
    if (samples.empty()) {
        return std::nullopt;
    }
    for (const double s : samples) {
        if (!std::isfinite(s) || s < 0.0) {
            return std::nullopt;
        }
    }

    // Private copy: sorting must not disturb the caller's sample order.
    std::sort(samples.begin(), samples.end());

    // Incremental mean of sorted nonnegative values avoids overflowing a sum,
    // including platforms where long double has the same range as double.
    long double mean = 0.0L;
    std::size_t seen = 0;
    for (const double s : samples) {
        ++seen;
        mean += (static_cast<long double>(s) - mean) / static_cast<long double>(seen);
    }

    const std::size_t n = samples.size();  // n > 0 past this point.

    // Nearest-rank quantiles computed as p*q + ceil(p*r / 100) with
    // n = 100*q + r. Splitting n this way keeps every intermediate (p <= 99,
    // r < 100, q <= n/100) small enough that no std::size_t product overflows.
    const std::size_t q = n / 100;
    const std::size_t r = n % 100;
    const std::size_t r50 = 50 * q + (50 * r + 99) / 100;
    const std::size_t r99 = 99 * q + (99 * r + 99) / 100;

    LatencySummary out;
    out.count = n;
    out.mean_us = static_cast<double>(mean);
    out.p50_us = samples[r50 - 1];
    out.p99_us = samples[r99 - 1];
    out.max_us = samples[n - 1];
    return out;
}

}  // namespace study_net

#endif  // STUDY_NET_LATENCY_SUMMARY_H
