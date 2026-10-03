#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "audio/mixer.h"
#include "audio/pcm_s16.h"
#include "audio/voice_order.h"

namespace study_audio {

// Owns a Mixer and maps caller-defined owner tags onto its fixed voice slots.
// Copy and move are deleted: Mixer borrows external PCM, so duplicating or
// relocating this controller is deliberately disallowed. No device and no
// heap; the caller must serialize all access (configure/start/stop/render).
class VoicePool {
public:
    static constexpr std::size_t kNoOwner =
        std::numeric_limits<std::size_t>::max();

    VoicePool() noexcept { owners_.fill(kNoOwner); }

    VoicePool(const VoicePool&) = delete;
    VoicePool& operator=(const VoicePool&) = delete;
    VoicePool(VoicePool&&) = delete;
    VoicePool& operator=(VoicePool&&) = delete;

    // Forward to the mixer; only a successful configure clears owner metadata.
    // An invalid format leaves the pool exactly as it was.
    bool configure(std::uint32_t rate, std::uint32_t channels) noexcept {
        if (!mixer_.configure(rate, channels)) return false;
        owners_.fill(kNoOwner);
        order_.reset();
        return true;
    }

    // One owner identifies one live, immutable PCM until stop_owner(owner).
    // Start clip for owner. Returns the chosen slot, or nullopt when owner is
    // kNoOwner or the mixer rejects the clip. Chooses the first non-playing
    // slot in ascending order, otherwise steals the oldest slot. A failure
    // leaves owner metadata and the ordering untouched.
    std::optional<std::size_t> start(std::size_t owner,
                                     const Pcm16& clip) noexcept {
        if (owner == kNoOwner) return std::nullopt;

        std::size_t slot = kMaxVoices;
        for (std::size_t i = 0; i < kMaxVoices; ++i) {
            if (!mixer_.playing(i)) {
                slot = i;
                break;
            }
        }
        if (slot == kMaxVoices) slot = order_.oldest();

        if (!mixer_.start(slot, clip)) return std::nullopt;

        owners_[slot] = owner;
        order_.mark_started(slot);
        return slot;
    }

    // Detach every voice owned by owner, including finished ones, so the
    // borrowed PCM may be released. kNoOwner is ignored.
    void stop_owner(std::size_t owner) noexcept {
        if (owner == kNoOwner) return;
        for (std::size_t i = 0; i < kMaxVoices; ++i) {
            if (owners_[i] == owner) {
                mixer_.stop(i);
                owners_[i] = kNoOwner;
            }
        }
    }

    // Silence every voice and forget all owner metadata and ordering.
    void stop_all() noexcept {
        mixer_.stop_all();
        owners_.fill(kNoOwner);
        order_.reset();
    }

    // True when any slot owned by owner is currently active.
    bool playing(std::size_t owner) const noexcept {
        if (owner == kNoOwner) return false;
        for (std::size_t i = 0; i < kMaxVoices; ++i) {
            if (owners_[i] == owner && mixer_.playing(i)) return true;
        }
        return false;
    }

    // Cursor of the most recently started slot owned by owner, searching the
    // newest-first ordering and including finished voices. 0 when none match.
    std::size_t cursor_frames(std::size_t owner) const noexcept {
        if (owner == kNoOwner) return 0;
        const std::array<std::size_t, kMaxVoices>& ordered = order_.ordered();
        for (std::size_t k = kMaxVoices; k > 0; --k) {
            const std::size_t slot = ordered[k - 1];
            if (owners_[slot] == owner) return mixer_.cursor_frames(slot);
        }
        return 0;
    }

    // Owner currently attached to voice, or nullopt for a bad/empty slot.
    std::optional<std::size_t> owner_of(std::size_t voice) const noexcept {
        if (voice >= kMaxVoices) return std::nullopt;
        if (owners_[voice] == kNoOwner) return std::nullopt;
        return owners_[voice];
    }

    // Diagnostic passthrough; slots are ephemeral, not durable identities.
    bool voice_playing(std::size_t voice) const noexcept {
        return mixer_.playing(voice);
    }

    void render(void* out, std::size_t bytes) noexcept {
        mixer_.render(out, bytes);
    }

private:
    static constexpr std::size_t kMaxVoices = audio_mix::kMaxVoices;

    Mixer mixer_;
    std::array<std::size_t, kMaxVoices> owners_{};
    audio_pool::VoiceOrder<kMaxVoices> order_;
};

}  // namespace study_audio
