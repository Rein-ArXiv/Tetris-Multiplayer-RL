#ifndef STUDY_NET_PROCESS_USAGE_H
#define STUDY_NET_PROCESS_USAGE_H
#include <cstdint>
#include <optional>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#endif
namespace study_net {
struct ProcessUsage {
    double cpu_seconds = 0;
    std::optional<std::uint64_t> voluntary_switches;
    std::optional<std::uint64_t> involuntary_switches;
};
// Cumulative PROCESS counters: both socket endpoints and coordinator included.
inline std::optional<ProcessUsage> process_usage() {
    ProcessUsage result;
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return std::nullopt;
    const auto seconds = [](FILETIME v) {
        const std::uint64_t ticks = (std::uint64_t(v.dwHighDateTime) << 32) |
                                    v.dwLowDateTime;
        return static_cast<double>(ticks) * 1e-7;
    };
    result.cpu_seconds = seconds(kernel) + seconds(user);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return std::nullopt;
    result.cpu_seconds = static_cast<double>(usage.ru_utime.tv_sec) +
        usage.ru_utime.tv_usec * 1e-6 + static_cast<double>(usage.ru_stime.tv_sec) +
        usage.ru_stime.tv_usec * 1e-6;
#ifdef __linux__
    result.voluntary_switches = static_cast<std::uint64_t>(usage.ru_nvcsw);
    result.involuntary_switches = static_cast<std::uint64_t>(usage.ru_nivcsw);
#endif
#endif
    return result;
}
} // namespace study_net
#endif
