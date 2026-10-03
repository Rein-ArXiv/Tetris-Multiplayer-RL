// audio/xaudio_player.cpp - XAudio2 backend implementation.
//
// Every control entry point runs on the initiating main thread and no user
// callbacks are registered. The worker reads immutable PCM; DestroyVoice
// synchronizes the end of those reads before this owner releases storage.

#include "audio/xaudio_player.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace study_audio {

namespace {

// One XAUDIO2_BUFFER is bounded by the API ceiling and by its 32-bit
// AudioBytes field; a single clip must satisfy both.
constexpr std::size_t kMaxBufferBytes =
    static_cast<std::size_t>(XAUDIO2_MAX_BUFFER_BYTES);

}  // namespace

XAudioPlayer::~XAudioPlayer() { close(); }

bool XAudioPlayer::open(std::uint32_t rate, std::uint32_t channels) noexcept {
    // Refuse a second open and reject an unusable format before touching COM: a
    // bad request must not acquire (and then have to release) an apartment
    // reference. layout_s16 is the shared source of truth for legal formats.
    if (engine_ != nullptr || master_ != nullptr) return false;
    if (!audio_pcm::layout_s16(1, channels, rate, Pcm16::kTeachingBudgetBytes)) {
        return false;
    }

    // CoInitializeEx maintains a per-thread balance, not a process-wide flag.
    // S_OK/S_FALSE mean this call added a reference that close() must undo, so
    // com_owned_ records that debt. RPC_E_CHANGED_MODE means the caller already
    // owns an apartment in another mode: leave it untouched (com_owned_ stays
    // false) and still attempt engine creation.
    const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (initialized == S_OK || initialized == S_FALSE) {
        com_owned_ = true;
    } else if (initialized == RPC_E_CHANGED_MODE) {
        com_owned_ = false;
    } else {
        return false;
    }

    if (FAILED(::XAudio2Create(&engine_, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        close();
        return false;
    }
    if (FAILED(engine_->CreateMasteringVoice(&master_, channels, rate))) {
        close();
        return false;
    }

    // Derive the source format only from the validated request.
    format_.wFormatTag = WAVE_FORMAT_PCM;
    format_.nChannels = static_cast<WORD>(channels);
    format_.nSamplesPerSec = rate;
    format_.wBitsPerSample = 16;
    format_.nBlockAlign = static_cast<WORD>(channels * sizeof(std::int16_t));
    format_.nAvgBytesPerSec =
        static_cast<DWORD>(rate * channels * sizeof(std::int16_t));
    format_.cbSize = 0;

    order_.reset();
    return true;
}

bool XAudioPlayer::replace(Pcm16&& clip, std::size_t slot) noexcept {
    // Validate everything before mutating any state.
    if (engine_ == nullptr) return false;
    if (slot >= kVoiceCount) return false;
    if (clip.channels() == 0 || clip.samples().empty()) return false;
    if (clip.rate() != format_.nSamplesPerSec) return false;
    if (clip.channels() != static_cast<std::uint32_t>(format_.nChannels)) {
        return false;
    }

    const std::size_t bytes = clip.samples().size() * sizeof(std::int16_t);
    if (bytes > kMaxBufferBytes) return false;
    if (bytes > static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }

    // Any voice still referencing the old clip must die first; finished voices
    // are also discarded so replacement leaves no cached owner metadata.
    stop_owner(slot);
    clips_[slot].emplace(std::move(clip));
    return true;
}

bool XAudioPlayer::play(std::size_t slot) noexcept {
    if (engine_ == nullptr) return false;
    if (slot >= kVoiceCount) return false;
    if (!clips_[slot].has_value()) return false;
    const Pcm16& clip = *clips_[slot];

    // Prefer a free voice: an unallocated one, or one whose submitted buffer
    // has already been consumed. Every configured voice shares format_, so any
    // reused idle voice is compatible with any clip.
    std::size_t target = kVoiceCount;
    for (std::size_t i = 0; i < kVoiceCount; ++i) {
        if (voices_[i].ptr == nullptr) {
            target = i;
            break;
        }
        XAUDIO2_VOICE_STATE state{};
        voices_[i].ptr->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued == 0) {
            target = i;
            break;
        }
    }

    // Pool is full: steal the least recently started voice.
    if (target == kVoiceCount) {
        target = order_.oldest();
        destroy_slot(target);
    }

    if (voices_[target].ptr == nullptr) {
        if (FAILED(engine_->CreateSourceVoice(&voices_[target].ptr, &format_))) {
            voices_[target].ptr = nullptr;
            voices_[target].owner = kNoOwner;
            return false;
        }
    }

    // The descriptor lives only for this call. The PCM it points at outlives
    // the submission because clips_ owns it until unload()/replace()/close(),
    // each of which first destroys the voices that reference it.
    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes =
        static_cast<UINT32>(clip.samples().size() * sizeof(std::int16_t));
    buffer.pAudioData = reinterpret_cast<const BYTE*>(clip.samples().data());
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    if (FAILED(voices_[target].ptr->SubmitSourceBuffer(&buffer))) {
        destroy_slot(target);
        return false;
    }
    if (FAILED(voices_[target].ptr->Start(0))) {
        destroy_slot(target);
        return false;
    }

    voices_[target].owner = slot;
    order_.mark_started(target);
    return true;
}

void XAudioPlayer::stop() noexcept {
    for (std::size_t i = 0; i < kVoiceCount; ++i) destroy_slot(i);
    order_.reset();
}

void XAudioPlayer::unload(std::size_t slot) noexcept {
    if (slot >= kVoiceCount) return;
    stop_owner(slot);
    clips_[slot].reset();
}

void XAudioPlayer::close() noexcept {
    // Idempotent and safe from any partial open(): stop voices, drop PCM, then
    // tear the device down in reverse creation order before balancing COM.
    stop();

    for (auto& clip : clips_) clip.reset();

    if (master_ != nullptr) {
        master_->DestroyVoice();
        master_ = nullptr;
    }
    if (engine_ != nullptr) {
        engine_->Release();
        engine_ = nullptr;
    }
    if (com_owned_) {
        ::CoUninitialize();
        com_owned_ = false;
    }

    format_ = WAVEFORMATEX{};
}

void XAudioPlayer::destroy_slot(std::size_t index) noexcept {
    if (index >= kVoiceCount) return;
    VoiceEntry& entry = voices_[index];
    if (entry.ptr != nullptr) {
        entry.ptr->DestroyVoice();
        entry.ptr = nullptr;
    }
    entry.owner = kNoOwner;
}

void XAudioPlayer::stop_owner(std::size_t owner) noexcept {
    for (std::size_t i = 0; i < kVoiceCount; ++i) {
        if (voices_[i].ptr != nullptr && voices_[i].owner == owner) {
            destroy_slot(i);
        }
    }
}

}  // namespace study_audio
