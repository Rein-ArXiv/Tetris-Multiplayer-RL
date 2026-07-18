// audio/sdl_audio.cpp — SDL2 오디오 백엔드 (Mac/Linux/Windows 크로스플랫폼)
//
// audio/audio.cpp (XAudio2) 와 동일한 audio.h API 전체(재생 7개 + 설정 토글/
// 볼륨 4개)를 SDL_OpenAudioDevice 콜백으로 재구현.
//   raylib 대응: rAudio -> miniaudio. 여기서는 SDL_AudioSpec.callback 에서
//   직접 softwaremix 수행 (int16 합산 + 포화 클램핑).
//
// 구조:
//   - SDL 콜백에서 BGM 보이스 + SFX 보이스 풀(8) 을 믹스.
//   - 보이스 = { sound handle, read position, active, loop }.
//   - audio_play_sound : 빈 SFX 슬롯 찾아 position=0, active=true 로 스타트.
//   - audio_play_music : BGM 보이스 교체, loop=true.
//   - 포맷 변환은 하지 않음 — 디바이스 포맷(2ch/44100/S16) 과 맞지 않는
//     MP3 는 samplerate 를 그대로 출력해 약간 이상해질 수 있지만 게임 사운드
//     수준에서는 충분.  필요 시 SDL_AudioStream 도입.

#include "audio.h"

#ifndef DR_MP3_IMPLEMENTATION
  #define DR_MP3_IMPLEMENTATION
#endif
#include "../third_party/dr_mp3.h"

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
static SDL_AudioDeviceID s_dev       = 0;

static std::vector<SoundData> s_sounds;

static constexpr int MAX_SFX_VOICES = 8;
static Voice s_sfx[MAX_SFX_VOICES];
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
static void mix_voice(Voice& v, int16_t* out, int frames, int outChannels, float gain)
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

        // 카테고리 게인 적용 후 포화 합산
        for (int c = 0; c < outChannels; ++c) {
            int s = (int)((c == 0) ? l : r);
            s = (int)(s * gain);
            int acc = (int)out[f * outChannels + c] + s;
            if (acc >  32767) acc =  32767;
            if (acc < -32768) acc = -32768;
            out[f * outChannels + c] = (int16_t)acc;
        }
    }
}

static void SDLCALL audio_callback(void* /*ud*/, Uint8* stream, int len)
{
    int16_t* out = (int16_t*)stream;
    int frames   = len / (s_have.channels * (int)sizeof(int16_t));
    memset(stream, 0, (size_t)len);

    std::lock_guard<std::mutex> lk(s_mu);
    mix_voice(s_bgm, out, frames, s_have.channels, s_musicVol);
    for (int i = 0; i < MAX_SFX_VOICES; ++i)
        mix_voice(s_sfx[i], out, frames, s_have.channels, s_sfxVol);
}

// ─── init / shutdown ─────────────────────────────────────────────────────────
bool audio_init()
{
    if (s_refCount > 0) { ++s_refCount; return s_initialized; }
    ++s_refCount;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[audio] SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
        s_initialized = false;
        return false;
    }

    SDL_AudioSpec want{};
    want.freq     = 44100;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 1024;
    want.callback = audio_callback;

    s_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &s_have,
                                SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (s_dev == 0) {
        fprintf(stderr, "[audio] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_initialized = false;
        return false;
    }

    s_sounds.clear();
    s_sounds.push_back(SoundData{});  // sentinel handle 0
    for (auto& v : s_sfx) v = {};
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
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    s_sounds.clear();
    s_initialized = false;
}

// ─── 로드 / 언로드 ─────────────────────────────────────────────────────────────
AudioHandle audio_load_sound(const char* filepath)
{
    if (!s_initialized) return 0;

    FILE* f = fopen(filepath, "rb");
    if (!f) { fprintf(stderr, "[audio] open %s failed\n", filepath); return 0; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    if (sz <= 0) { fclose(f); return 0; }
    std::vector<uint8_t> raw((size_t)sz);
    const size_t nread = fread(raw.data(), 1, raw.size(), f);
    fclose(f);
    if (nread != raw.size()) {
        fprintf(stderr, "[audio] read %s failed (%zu/%zu bytes)\n",
                filepath, nread, raw.size());
        return 0;
    }

    drmp3_config cfg{};
    drmp3_uint64 frames = 0;
    drmp3_int16* samples = drmp3_open_memory_and_read_pcm_frames_s16(
        raw.data(), raw.size(), &cfg, &frames, nullptr);
    if (!samples || frames == 0) {
        fprintf(stderr, "[audio] decode %s failed\n", filepath);
        if (samples) drmp3_free(samples, nullptr);
        return 0;
    }

    SoundData sd;
    sd.channels   = cfg.channels;
    sd.sampleRate = cfg.sampleRate;
    size_t total  = (size_t)frames * cfg.channels;
    sd.pcm.assign(samples, samples + total);
    sd.valid = true;
    drmp3_free(samples, nullptr);

    std::lock_guard<std::mutex> lk(s_mu);
    AudioHandle h = (AudioHandle)s_sounds.size();
    s_sounds.push_back(std::move(sd));
    return h;
}

void audio_unload_sound(AudioHandle h)
{
    if (!s_initialized) return;
    if (h <= 0 || h >= (int)s_sounds.size()) return;

    std::lock_guard<std::mutex> lk(s_mu);
    if (s_bgm.handle == h) s_bgm = {};
    if (s_currentMusic == h) s_currentMusic = 0;  // 언로드된 핸들로 off→on 복원 금지
    for (auto& v : s_sfx) if (v.handle == h) v = {};
    s_sounds[h].pcm.clear();
    s_sounds[h].pcm.shrink_to_fit();
    s_sounds[h].valid = false;
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
    if (slot < 0) slot = 0;  // 모두 바쁘면 첫 번째를 강제 교체
    s_sfx[slot] = Voice{ h, 0, false, true };
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
    if (v01 < 0.0f) v01 = 0.0f;
    if (v01 > 1.0f) v01 = 1.0f;
    std::lock_guard<std::mutex> lk(s_mu);
    s_musicVol = v01;
}

void audio_set_sfx_volume(float v01)
{
    if (v01 < 0.0f) v01 = 0.0f;
    if (v01 > 1.0f) v01 = 1.0f;
    std::lock_guard<std::mutex> lk(s_mu);
    s_sfxVol = v01;
}
