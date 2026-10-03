// audio/sdl_audio.cpp — SDL2 오디오 백엔드 (Mac/Linux/Windows 크로스플랫폼)
//
// audio/audio.cpp (XAudio2) 와 동일한 audio.h 재생·설정 API 를
// SDL_OpenAudioDevice 콜백으로 재구현.
//   기성 프레임워크의 오디오 모듈이 하던 믹싱을 여기서는 SDL_AudioSpec.callback
//   에서 직접 수행 (int32 누산 후 int16 포화 클램핑).
//
// 구조:
//   - SDL 콜백에서 BGM 보이스 + SFX 보이스 풀(8) 을 믹스.
//   - 보이스 = { sound handle, read position, active, loop }.
//   - audio_play_sound : 빈 SFX 슬롯 찾아 position=0, active=true 로 스타트.
//   - audio_play_music : BGM 보이스 교체, loop=true.
//   - 로드 시점에 SDL_AudioStream 으로 디바이스 포맷(채널/샘플레이트) 에 맞춰
//     한 번 변환해 둔다. 그래서 믹서는 리샘플링을 몰라도 되고, 콜백은 단순
//     합산만 한다. 변환을 재생 시점이 아니라 로드 시점에 하는 이유는
//     오디오 콜백 스레드에서 할당을 피하기 위해서다.

#include "audio.h"
#include "pcm_layout.h"
#include "mix_s16.h"
#include "voice_order.h"
#include <array>
#include <algorithm>
#include <limits>

#include "mp3_decode.h"
#include <memory>
#include <new>
#include <stdexcept>

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

// ─── 내부 상태 ────────────────────────────────────────────────────────────────
struct SoundData {
    std::vector<int16_t> pcm;      // 디코딩된 16-bit signed PCM (디바이스 포맷 기준)
    uint32_t channels = 0;
    uint32_t sampleRate = 0;
    bool valid = false;
};

struct Voice {
    int handle = 0;     // 0 = idle
    size_t pos = 0;     // PCM 샘플 인덱스 (int16 단위, 채널 포함)
    bool loop = false;
    bool active = false;
};

static bool            s_initialized = false;
static int             s_refCount    = 0;
static bool            s_audioOwned  = false; // successful SDL subsystem reference
static SDL_AudioDeviceID s_dev       = 0;

static std::vector<SoundData> s_sounds;

static constexpr int MAX_SFX_VOICES = 8;
static_assert(MAX_SFX_VOICES + 1 <= audio_mix::kMaxVoices, "mixer accumulator bound");
static Voice s_sfx[MAX_SFX_VOICES];
static audio_pool::VoiceOrder<MAX_SFX_VOICES> s_sfxOrder;
static Voice s_bgm;          // 단일 BGM 보이스

// 설정 토글 (렌더/오디오 전용 — SimGame/결정성과 무관).
static bool        s_musicEnabled = true;
static bool        s_sfxEnabled   = true;
static AudioHandle s_currentMusic = 0;   // 마지막으로 요청된 BGM 핸들 (off→on 복원용)

// 카테고리별 볼륨 (0.0~1.0). 믹스 시점에 샘플에 곱한다. 설정 슬라이더가 구동.
static float       s_musicVol = 1.0f;
static float       s_sfxVol   = 1.0f;

static SDL_AudioSpec s_have{};   // 디바이스 최종 포맷
static std::mutex    s_mu;       // 콜백 ↔ API 간 공유 상태 보호

// ─── 믹서 콜백 ────────────────────────────────────────────────────────────────
// gain: 이 보이스 카테고리(BGM/SFX)의 0~1 볼륨. 합산 전에 샘플에 곱한다.
static void mix_voice(Voice& v, int32_t* out, int frames, int outChannels, float gain)
{
    if (!v.active || v.handle <= 0) return;
    SoundData& sd = s_sounds[v.handle];
    if (!sd.valid) { v.active = false; return; }

    const int16_t* src = sd.pcm.data();
    const size_t total = sd.pcm.size();         // int16 단위
    const int    sc    = (int)sd.channels;      // 소스 채널(1 or 2)

    for (int f = 0; f < frames; ++f) {
        if (v.pos + sc > total) {
            if (v.loop) v.pos = 0;
            else { v.active = false; return; }
        }
        // 모노→스테레오 승격 혹은 스테레오→스테레오 패스스루
        int16_t l = src[v.pos];
        int16_t r = (sc >= 2) ? src[v.pos + 1] : l;
        v.pos += sc;

        // Keep the full sum until every voice has contributed.
        for (int c = 0; c < outChannels; ++c) {
            const int16_t sample = (c == 0) ? l : r;
            out[f * outChannels + c] += audio_mix::scaled_sample(sample, gain);
        }
    }
    // A buffer ending exactly at this request boundary is already reusable.
    if (!v.loop && v.pos >= total) v.active = false;
}
static void SDLCALL audio_callback(void* /*ud*/, Uint8* stream, int len)
{
    if (len <= 0) return;
    memset(stream, 0, static_cast<size_t>(len));
    const int channels = s_have.channels;
    if (channels < 1 || channels > 2) return;
    const int frames = len / (channels * static_cast<int>(sizeof(int16_t)));
    std::array<int32_t, audio_mix::kBlockFrames * 2> sum{};

    std::lock_guard<std::mutex> lk(s_mu);
    for (int offset = 0; offset < frames; ) {
        const int count = std::min(frames - offset, static_cast<int>(audio_mix::kBlockFrames));
        std::fill(sum.begin(), sum.end(), 0);
        mix_voice(s_bgm, sum.data(), count, channels, s_musicVol);
        for (int i = 0; i < MAX_SFX_VOICES; ++i)
            mix_voice(s_sfx[i], sum.data(), count, channels, s_sfxVol);
        for (int i = 0; i < count * channels; ++i) {
            const int16_t value = audio_mix::finish_sample(sum[i]);
            const size_t byteOffset = (static_cast<size_t>(offset) * channels + i) * sizeof(value);
            memcpy(stream + byteOffset, &value, sizeof(value));
        }
        offset += count;
    }
}
// ─── init / shutdown ─────────────────────────────────────────────────────────
bool audio_init()
{
    if (s_refCount > 0) { ++s_refCount; return s_initialized; }
    ++s_refCount;

    // Prepare C++ storage before acquiring OS resources.
    try {
        s_sounds.clear();
        s_sounds.push_back(SoundData{}); // sentinel handle 0
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[audio] SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
        s_initialized = false;
        return false;
    }

    s_audioOwned = true;

    SDL_AudioSpec want{};
    want.freq     = 44100;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 1024;
    want.callback = audio_callback;

    // allowed_changes = 0 — 요청한 포맷을 그대로 받는다. 장치가 44100 을
    // 지원하지 않으면 SDL 이 내부 변환기를 끼워 넣는다.
    // 예전에는 SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 를 줬는데, 그러면 48000 으로
    // 열린 장치에서 44100 짜리 MP3 가 그대로 흘러 약 8.8% 빠르게 재생됐다.
    s_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &s_have, 0);
    if (s_dev == 0) {
        fprintf(stderr, "[audio] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_audioOwned = false;
        s_initialized = false;
        return false;
    }

    for (auto& v : s_sfx) v = {};
    s_sfxOrder.reset();
    s_bgm = {};

    SDL_PauseAudioDevice(s_dev, 0);
    s_initialized = true;
    return true;
}

void audio_shutdown()
{
    if (s_refCount <= 0) return;
    --s_refCount;
    if (s_refCount > 0) return;

    if (s_dev) {
        SDL_PauseAudioDevice(s_dev, 1);
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
    }
    if (s_audioOwned) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_audioOwned = false;
    }
    for (auto& v : s_sfx) v = {};
    s_sfxOrder.reset();
    s_bgm = {};
    s_currentMusic = 0;
    s_sounds.clear();
    s_initialized = false;
}

// ─── 로드 / 언로드 ─────────────────────────────────────────────────────────────
AudioHandle audio_load_sound(const char* filepath)
{
    if (!s_initialized || !filepath || !*filepath) return 0;

    try {
        auto pcm = audio_mp3::load(filepath);
        if (!pcm) {
            fprintf(stderr, "[audio] decode %s failed: %s\n",
                    filepath, audio_mp3::error_name(pcm.error));
            return 0;
        }

        // SDL_AudioStreamPut takes an int byte count; validate the PCM layout
        // against that API limit before narrowing.
        const size_t frames = pcm.channels != 0
            ? pcm.samples.size() / pcm.channels : 0;
        const auto layout = audio_pcm::layout_s16(
            frames, pcm.channels, pcm.rate,
            static_cast<size_t>(std::numeric_limits<int>::max()));
        if (!layout) {
            fprintf(stderr, "[audio] unsupported PCM layout: %s\n", filepath);
            return 0;
        }

        SoundData sd;

        if ((int)pcm.channels == s_have.channels &&
            (int)pcm.rate == s_have.freq)
        {
            // Already device format: move the decoded samples in.
            sd.pcm = std::move(pcm.samples);
            sd.channels   = pcm.channels;
            sd.sampleRate = pcm.rate;
            sd.valid      = true;
        }
        else
        {
            // Channel count or rate differs: run through an SDL converter.
            std::unique_ptr<SDL_AudioStream, decltype(&SDL_FreeAudioStream)> conv(
                SDL_NewAudioStream(
                    AUDIO_S16SYS, (Uint8)pcm.channels, (int)pcm.rate,
                    AUDIO_S16SYS, (Uint8)s_have.channels, s_have.freq),
                &SDL_FreeAudioStream);
            if (!conv) {
                fprintf(stderr, "[audio] SDL_NewAudioStream failed for %s: %s\n",
                        filepath, SDL_GetError());
                return 0;
            }

            const int inBytes = static_cast<int>(layout->bytes);
            if (SDL_AudioStreamPut(conv.get(), pcm.samples.data(), inBytes) != 0 ||
                SDL_AudioStreamFlush(conv.get()) != 0) {
                fprintf(stderr, "[audio] resample failed for %s: %s\n",
                        filepath, SDL_GetError());
                return 0;  // unique_ptr frees the stream on scope exit.
            }
            // Input PCM is no longer needed once the converter consumed it.
            std::vector<int16_t>().swap(pcm.samples);

            const int outBytes = SDL_AudioStreamAvailable(conv.get());
            const int frameBytes = s_have.channels * static_cast<int>(sizeof(int16_t));
            if (outBytes <= 0 || frameBytes <= 0 || outBytes % frameBytes != 0) {
                fprintf(stderr, "[audio] resample produced incomplete PCM for %s\n", filepath);
                return 0;
            }
            sd.pcm.resize((size_t)outBytes / sizeof(int16_t));
            const int got = SDL_AudioStreamGet(conv.get(), sd.pcm.data(), outBytes);
            if (got != outBytes) {
                fprintf(stderr, "[audio] resample read incomplete for %s (%d/%d)\n",
                        filepath, got, outBytes);
                return 0;
            }

            sd.channels   = (uint32_t)s_have.channels;
            sd.sampleRate = (uint32_t)s_have.freq;
            sd.valid      = true;
        }

        std::lock_guard<std::mutex> lk(s_mu);
        if (s_sounds.size() >= static_cast<size_t>(std::numeric_limits<int>::max())) {
            fprintf(stderr, "[audio] sound handle limit reached: %s\n", filepath);
            return 0;
        }
        AudioHandle h = (AudioHandle)s_sounds.size();
        s_sounds.push_back(std::move(sd));
        return h;
    } catch (const std::bad_alloc&) {
        fprintf(stderr, "[audio] allocation failed: %s\n", filepath);
        return 0;
    } catch (const std::length_error&) {
        fprintf(stderr, "[audio] PCM storage limit: %s\n", filepath);
        return 0;
    }
}
void audio_unload_sound(AudioHandle h)
{
    if (!s_initialized) return;
    if (h <= 0 || h >= (int)s_sounds.size()) return;

    // Disconnect every reader under the lock; release storage after unlocking.
    std::vector<int16_t> retired;
    {
        std::lock_guard<std::mutex> lk(s_mu);
        if (s_bgm.handle == h) s_bgm = {};
        if (s_currentMusic == h) s_currentMusic = 0;
        for (auto& v : s_sfx) if (v.handle == h) v = {};
        retired.swap(s_sounds[h].pcm);
        s_sounds[h].valid = false;
    }
}
// ─── 재생 ─────────────────────────────────────────────────────────────────────
void audio_play_sound(AudioHandle h)
{
    if (!s_initialized) return;
    if (!s_sfxEnabled) return;
    if (h <= 0 || h >= (int)s_sounds.size() || !s_sounds[h].valid) return;

    std::lock_guard<std::mutex> lk(s_mu);
    int slot = -1;
    for (int i = 0; i < MAX_SFX_VOICES; ++i) {
        if (!s_sfx[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = static_cast<int>(s_sfxOrder.oldest());
    s_sfx[slot] = Voice{ h, 0, false, true };
    s_sfxOrder.mark_started(static_cast<size_t>(slot));
}

void audio_play_music(AudioHandle h)
{
    if (!s_initialized) return;
    if (h <= 0 || h >= (int)s_sounds.size() || !s_sounds[h].valid) return;

    std::lock_guard<std::mutex> lk(s_mu);
    s_currentMusic = h;                       // off→on 복원용으로 항상 기억
    s_bgm = s_musicEnabled ? Voice{ h, 0, true, true } : Voice{};
}

void audio_stop_music()
{
    if (!s_initialized) return;
    std::lock_guard<std::mutex> lk(s_mu);
    s_bgm = {};
    s_currentMusic = 0;
}

void audio_set_music_enabled(bool on)
{
    std::lock_guard<std::mutex> lk(s_mu);
    if (s_musicEnabled == on) return; // Setting a state is not a replay command.
    s_musicEnabled = on;
    if (!s_initialized) return;
    if (on) {
        // 마지막으로 요청된 음악을 다시 재생.
        if (s_currentMusic > 0 && s_currentMusic < (int)s_sounds.size()
            && s_sounds[s_currentMusic].valid)
            s_bgm = Voice{ s_currentMusic, 0, true, true };
    } else {
        s_bgm = {};   // 핸들(s_currentMusic)은 유지 — on 시 복원.
    }
}

void audio_set_sfx_enabled(bool on)
{
    s_sfxEnabled = on;
}

void audio_set_music_volume(float v01)
{
    v01 = audio_mix::normalize_gain(v01);
    std::lock_guard<std::mutex> lk(s_mu);
    s_musicVol = v01;
}

void audio_set_sfx_volume(float v01)
{
    v01 = audio_mix::normalize_gain(v01);
    std::lock_guard<std::mutex> lk(s_mu);
    s_sfxVol = v01;
}
