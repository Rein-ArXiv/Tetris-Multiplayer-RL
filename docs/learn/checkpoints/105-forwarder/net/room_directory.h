#ifndef STUDY_NET_ROOM_DIRECTORY_H_
#define STUDY_NET_ROOM_DIRECTORY_H_

// Room code registry and host-payload ownership for the multiplayer service.
//
// This layer is deliberately small: it maps a short room code to a host-owned
// payload and returns a RoomHandle value. It performs no waiting, no guest
// READY/game-start handshake, and no authentication. Room codes are only a
// guest-facing address. Handles identify server-owned entries; they are neither
// authentication credentials nor secrets, and clients must not choose their fields.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace study_net {

using RoomCode = std::array<char, 5>;

inline constexpr std::size_t kRoomCodeLength = 5;
inline constexpr char kRoomCodeAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
inline constexpr std::size_t kRoomCodeAlphabetSize = 32;

// The alphabet deliberately drops I/O/0/1 to avoid look-alike confusion, which
// is exactly what makes 32 symbols fit the five 5-bit groups below.
static_assert(sizeof(kRoomCodeAlphabet) - 1 == kRoomCodeAlphabetSize,
              "room code alphabet must contain exactly 32 symbols");

// Packs five 5-bit groups from the low bits of `word` into a code. All-zero
// input is legal and yields "AAAAA".
inline RoomCode code_from_word(std::uint32_t word) noexcept {
    RoomCode code{};
    for (std::size_t i = 0; i < kRoomCodeLength; ++i) {
        code[i] = kRoomCodeAlphabet[word & 31u];
        word >>= 5;
    }
    return code;
}

// Parses exactly five alphabet characters, with no case folding or trimming.
// Anything else (wrong length, lowercase, excluded letters) yields nullopt.
inline std::optional<RoomCode> parse_room_code(std::string_view text) noexcept {
    if (text.size() != kRoomCodeLength) {
        return std::nullopt;
    }
    RoomCode code{};
    for (std::size_t i = 0; i < kRoomCodeLength; ++i) {
        std::size_t index = kRoomCodeAlphabetSize;
        for (std::size_t j = 0; j < kRoomCodeAlphabetSize; ++j) {
            if (kRoomCodeAlphabet[j] == text[i]) {
                index = j;
                break;
            }
        }
        if (index == kRoomCodeAlphabetSize) {
            return std::nullopt;
        }
        code[i] = text[i];
    }
    return code;
}

inline std::string room_code_text(const RoomCode& code) {
    return std::string(code.data(), code.size());
}

// Server-minted identity for one live room. Only the server chooses it;
// generation distinguishes a reused code from a previous, deleted room.
struct RoomHandle {
    RoomCode code{};
    std::uint64_t generation = 0;
    std::uint64_t owner = 0;
};

// A live entry owns its payload in the directory until take() transfers it out.
template <class T>
struct RoomEntry {
    RoomHandle handle{};
    T value;
};

enum class RoomCreate {
    created,
    closed,
    invalid_owner,
    duplicate_owner,
    generation_exhausted,
    full,
    source_failed,
    collision_limit,
};

struct RoomCreated {
    RoomCreate status = RoomCreate::source_failed;
    std::optional<RoomHandle> handle;
};

// Fixed-capacity registry of live rooms. Slots are sparse: a room can be taken
// out of the middle without disturbing the others, so unlike a dense queue the
// array is allowed to have holes.
//
// Operations on registry state lock the internal mutex. A returned
// RoomHandle is a snapshot, so a later find()+take() pair can fail if another
// operation removes the room first.
//
// Lifetime: the caller must drain every API user before destroying the object;
// T destruction also runs under the mutex: no blocking or registry reentry.
// The destructor does not synchronize with concurrent callers. The fixed slots
// bound only this container - each T may still own resources.
template <class T, std::size_t Capacity>
class RoomDirectory {
public:
    static_assert(Capacity > 0, "RoomDirectory needs at least one fixed slot");
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "RoomDirectory must move T out of a slot without throwing");
    static_assert(std::is_nothrow_destructible_v<T>,
                  "RoomDirectory destroys T while holding its mutex");

    // first_generation == 0 means generations are already exhausted.
    explicit RoomDirectory(std::uint64_t first_generation = 1) noexcept
        : next_generation_(first_generation) {}

    // The container is a fixed, non-copyable resource; draining is the caller's
    // responsibility because live entries own host payloads.
    RoomDirectory(const RoomDirectory&) = delete;
    RoomDirectory& operator=(const RoomDirectory&) = delete;
    RoomDirectory(RoomDirectory&&) = delete;
    RoomDirectory& operator=(RoomDirectory&&) = delete;
    ~RoomDirectory() = default;

    // Attempts to admit `value` for `owner`.
    //
    // `next` supplies a fresh 32-bit candidate word and is invoked while the
    // internal mutex is held. It must therefore be non-blocking, bounded, and
    // free of reentry into this directory - including from any T destructor it
    // might trigger. Returning nullopt aborts with source_failed and there is
    // no fallback source. Entropy policy belongs to the caller: the real
    // service wires up a proper source separately from this deterministic test
    // seam.
    //
    // On any failure `value` is left untouched and no generation is consumed.
    template <class Next>
    [[nodiscard]] RoomCreated create(std::uint64_t owner, T& value, Next&& next) {
        static_assert(
            std::is_nothrow_invocable_r_v<std::optional<std::uint32_t>, Next&>,
            "next must be a noexcept callable returning optional<uint32_t>");

        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_) {
            return RoomCreated{RoomCreate::closed, std::nullopt};
        }
        if (owner == 0) {
            return RoomCreated{RoomCreate::invalid_owner, std::nullopt};
        }
        if (has_owner_(owner)) {
            return RoomCreated{RoomCreate::duplicate_owner, std::nullopt};
        }
        if (next_generation_ == 0) {
            return RoomCreated{RoomCreate::generation_exhausted, std::nullopt};
        }
        if (count_ >= Capacity) {
            return RoomCreated{RoomCreate::full, std::nullopt};
        }

        for (std::uint32_t attempt = 0; attempt < kRoomCreateAttempts; ++attempt) {
            std::optional<std::uint32_t> word = std::invoke(next);
            if (!word.has_value()) {
                return RoomCreated{RoomCreate::source_failed, std::nullopt};
            }

            const RoomCode candidate = code_from_word(*word);
            if (code_in_use_(candidate)) {
                continue;  // Retry against every live code, not just one slot.
            }

            const std::size_t slot = first_empty_slot_();
            const std::uint64_t generation = next_generation_;
            const RoomHandle handle{candidate, generation, owner};

            entries_[slot].emplace(RoomEntry<T>{handle, std::move(value)});
            ++count_;

            // Never wrap back to 1, which would alias the first generation.
            next_generation_ = (generation == std::numeric_limits<std::uint64_t>::max())
                                   ? 0
                                   : generation + 1;

            return RoomCreated{RoomCreate::created, handle};
        }

        return RoomCreated{RoomCreate::collision_limit, std::nullopt};
    }

    // Returns only a handle snapshot, never access to T. Preexisting aliases
    // (such as shared_ptr copies) still follow the payload ownership contract.
    [[nodiscard]] std::optional<RoomHandle> find(const RoomCode& code) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& slot : entries_) {
            if (slot.has_value() && slot->handle.code == code) {
                return slot->handle;
            }
        }
        return std::nullopt;
    }

    // Removes and returns the entry only when code, generation, and owner all
    // match. A stale cleanup from an old room therefore cannot evict a reused
    // code, even if the same owner re-registered it.
    [[nodiscard]] std::optional<RoomEntry<T>> take(const RoomHandle& handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : entries_) {
            if (!slot.has_value()) {
                continue;
            }
            const RoomHandle& current = slot->handle;
            if (current.code != handle.code ||
                current.generation != handle.generation ||
                current.owner != handle.owner) {
                continue;
            }

            std::optional<RoomEntry<T>> result{std::move(slot.value())};
            slot.reset();
            --count_;
            return result;
        }
        return std::nullopt;
    }

    // Idempotent shutdown: marks the directory closed and destroys all live
    // entries. It does not join callers already inside another method.
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) {
            return;
        }
        closed_ = true;
        for (auto& slot : entries_) {
            slot.reset();
        }
        count_ = 0;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    [[nodiscard]] bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

private:
    static constexpr std::uint32_t kRoomCreateAttempts = 32;

    bool has_owner_(std::uint64_t owner) const {
        for (const auto& slot : entries_) {
            if (slot.has_value() && slot->handle.owner == owner) {
                return true;
            }
        }
        return false;
    }

    bool code_in_use_(const RoomCode& code) const {
        for (const auto& slot : entries_) {
            if (slot.has_value() && slot->handle.code == code) {
                return true;
            }
        }
        return false;
    }

    // Precondition: count_ < Capacity, so an empty slot always exists.
    std::size_t first_empty_slot_() const {
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (!entries_[i].has_value()) {
                return i;
            }
        }
        return Capacity;
    }

    std::array<std::optional<RoomEntry<T>>, Capacity> entries_{};
    std::size_t count_ = 0;
    mutable std::mutex mutex_;
    bool closed_ = false;
    std::uint64_t next_generation_ = 1;
};

}  // namespace study_net

#endif  // STUDY_NET_ROOM_DIRECTORY_H_
