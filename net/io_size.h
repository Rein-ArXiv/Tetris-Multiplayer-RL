#pragma once
#include <cstddef>
#include <limits>

namespace net {
// Winsock send takes int, while callers keep total buffer lengths as size_t.
// Bound before narrowing; a large buffer is offered in several calls.
inline constexpr int io_chunk_size(std::size_t remaining) noexcept {
    constexpr auto maximum = static_cast<std::size_t>((std::numeric_limits<int>::max)());
    return static_cast<int>(remaining > maximum ? maximum : remaining);
}
} // namespace net
