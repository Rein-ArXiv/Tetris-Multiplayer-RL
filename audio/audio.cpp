// audio/audio.cpp -- XAudio2 + dr_mp3 구현
//
// 학습 포인트:
//   이 파일이 하는 일 = 기성 프레임워크의 오디오 모듈(내부 믹서 라이브러리 래퍼)이
//   해 주던 일을 직접 구현한 것.
//   XAudio2 그래프: Source Voice -> Mastering Voice -> 스피커
//   MP3 디코딩은 dr_mp3 (단일 헤더, public domain).
//
// 설계 원칙:
//   - 오디오 실패는 비치명적. s_initialized 가 false 이면 모든 함수는 no-op.
//   - 참조 카운팅으로 멀티플레이(두 Game 인스턴스)에서도 안전.
//   - SFX 는 fire-and-forget 보이스 풀, BGM 은 루프 재생 전용 보이스.

#include "mp3_decode.h"
#include <new>
#include <stdexcept>

#include "audio.h"
#include "pcm_layout.h"
#include "mix_s16.h"
#include "voice_order.h"
#include "../core/once_flags.h"
#include <limits>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Windows / XAudio2
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>

// ─── 내부 상태 ──────────────────────────────────────────────────────────────────

struct SoundData
{
    std::vector<uint8_t> pcmData;  // 디코딩된 PCM 샘플 (signed 16-bit)
    WAVEFORMATEX         format;   // 채널, 샘플레이트, 비트
    bool                 valid;
};

static bool                      s_initialized  = false;
static int                       s_refCount     = 0;
static bool                      s_comOwned     = false;  // 우리가 이 스레드에서 성공한 COM 초기화 참조를 보유하는가?
static IXAudio2*                 s_xaudio       = nullptr;
static IXAudio2MasteringVoice*   s_masterVoice  = nullptr;

// 사운드 저장소. 인덱스 0 은 무효 (sentinel).
static std::vector<SoundData>    s_sounds;

// BGM 전용 보이스
static AudioHandle               s_currentMusic = 0;
static IXAudio2SourceVoice*      s_musicVoice   = nullptr;

// 설정 토글 (렌더/오디오 전용 — SimGame/결정성과 무관).
static bool                      s_musicEnabled = true;
static bool                      s_sfxEnabled   = true;
static AudioHandle               s_lastMusic    = 0;  // 마지막 요청 BGM (off→on 복원용)

// 카테고리별 볼륨 (0.0~1.0). 설정 슬라이더가 구동. 음악은 음악 보이스에
// SetVolume, SFX 는 재생 시점에 각 소스 보이스에 SetVolume 로 적용한다.
static float                     s_musicVol     = 1.0f;
static float                     s_sfxVol       = 1.0f;

// SFX 보이스 풀
static constexpr int             MAX_SFX_VOICES = 8;
static audio_pool::VoiceOrder<MAX_SFX_VOICES> s_sfxOrder;
static IXAudio2SourceVoice*      s_sfxVoices[MAX_SFX_VOICES] = {};
static WAVEFORMATEX              s_sfxFormats[MAX_SFX_VOICES] = {};
// 각 보이스가 지금 어느 핸들의 PCM 을 물고 있는지. XAudio2 는 SubmitSourceBuffer 에
// 넘긴 포인터를 재생이 끝날 때까지 그대로 참조하므로, 언로드 시 그 버퍼를
// 해제하기 전에 해당 보이스를 먼저 멈춰야 한다.
static AudioHandle               s_sfxHandles[MAX_SFX_VOICES] = {};

// Repeated failed play requests must not flood stderr. One diagnostic per
// stage and device lifetime; additional shared init calls do not reset it.
enum class PlaybackFailure { sfx_create, sfx_start, music_create, music_start, count };
static once_flags::Flags<static_cast<size_t>(PlaybackFailure::count)> s_failureNotices;
static bool first_failure(PlaybackFailure failure)
{
    return s_failureNotices.take(static_cast<size_t>(failure));
}

// ─── 내부 유틸 ──────────────────────────────────────────────────────────────────

static WAVEFORMATEX MakeWaveFormat(std::uint32_t channels, std::uint32_t sampleRate)
{
    WAVEFORMATEX wf = {};
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = static_cast<WORD>(channels);
    wf.nSamplesPerSec  = sampleRate;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = static_cast<WORD>(channels * 2);  // 16-bit = 2 bytes
    wf.nAvgBytesPerSec = sampleRate * wf.nBlockAlign;
    wf.cbSize          = 0;
    return wf;
}

static bool FormatMatches(const WAVEFORMATEX& a, const WAVEFORMATEX& b)
{
    return a.nChannels      == b.nChannels
        && a.nSamplesPerSec == b.nSamplesPerSec
        && a.wBitsPerSample == b.wBitsPerSample;
}

// ─── 공개 API ───────────────────────────────────────────────────────────────────

bool audio_init()
{
    // 참조 카운팅: 이미 초기화되었으면 카운트만 증가
    if (s_refCount > 0)
    {
        ++s_refCount;
        return s_initialized;
    }
    ++s_refCount;

    s_failureNotices.reset();

    // Allocate the sentinel before owning COM or audio graph resources.
    try {
        s_sounds.clear();
        s_sounds.push_back(SoundData{{}, {}, false});
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }

    // 호출 스레드의 COM 초기화. 같은 모델로 이미 초기화됐다면 S_FALSE.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == S_OK)
    {
        s_comOwned = true;
    }
    else if (hr == S_FALSE)
    {
        // 호출한 이 스레드가 같은 모델로 이미 초기화됨. 이번 성공도 해제와 짝짓는다.
        s_comOwned = true;  // CoUninitialize 호출 필요 (S_FALSE 도 짝 맞춰야 함)
    }
    else if (hr == RPC_E_CHANGED_MODE)
    {
        // 호출 스레드의 모델을 바꾸지 않고 엔진 생성을 시도한다. 성공 여부는 아래에서 검사.
        fprintf(stderr, "[audio] COM already initialized with different threading model\n");
        s_comOwned = false;
    }
    else
    {
        fprintf(stderr, "[audio] CoInitializeEx failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        s_initialized = false;
        return false;
    }

    // XAudio2 엔진 생성
    hr = XAudio2Create(&s_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        fprintf(stderr, "[audio] XAudio2Create failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        if (s_comOwned) CoUninitialize();
        s_comOwned = false;
        s_initialized = false;
        return false;
    }

    // 마스터링 보이스 생성 (기본 오디오 출력 장치)
    hr = s_xaudio->CreateMasteringVoice(&s_masterVoice);
    if (FAILED(hr))
    {
        fprintf(stderr, "[audio] CreateMasteringVoice failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        s_xaudio->Release();
        s_xaudio = nullptr;
        if (s_comOwned) CoUninitialize();
        s_comOwned = false;
        s_initialized = false;
        return false;
    }


    s_sfxOrder.reset();
    s_initialized = true;
    return true;
}

void audio_shutdown()
{
    if (s_refCount <= 0) return;
    --s_refCount;
    if (s_refCount > 0) return;  // 다른 Game 인스턴스가 아직 살아있음

    // BGM 정지
    audio_stop_music();

    // SFX 보이스 풀 해제
    for (int i = 0; i < MAX_SFX_VOICES; ++i)
    {
        if (s_sfxVoices[i])
        {
            s_sfxVoices[i]->DestroyVoice();
            s_sfxVoices[i] = nullptr;
        }
        s_sfxFormats[i] = {};
        s_sfxHandles[i] = 0;
    }

    // 사운드 데이터 해제
    s_sounds.clear();

    // 마스터링 보이스 → XAudio2 엔진 순서대로 해제
    if (s_masterVoice)
    {
        s_masterVoice->DestroyVoice();
        s_masterVoice = nullptr;
    }

    if (s_xaudio)
    {
        s_xaudio->Release();
        s_xaudio = nullptr;
    }

    if (s_comOwned)
    {
        CoUninitialize();
        s_comOwned = false;
    }

    s_initialized = false;
}

AudioHandle audio_load_sound(const char* filepath)
{
    if (!s_initialized || !filepath || !*filepath) return 0;

    try {
        auto pcm = audio_mp3::load(filepath);
        if (!pcm) {
            fprintf(stderr, "[audio] MP3 decode failed: %s (%s)\n",
                    filepath, audio_mp3::error_name(pcm.error));
            return 0;
        }

        // Frame to byte count is checked against the API limit and size_t range.
        const size_t frames = pcm.channels != 0
            ? pcm.samples.size() / pcm.channels : 0;
        const auto layout = audio_pcm::layout_s16(
            frames, pcm.channels, pcm.rate, XAUDIO2_MAX_BUFFER_BYTES);
        if (!layout) {
            fprintf(stderr, "[audio] Unsupported PCM layout: %s\n", filepath);
            return 0;
        }

        // SoundData store
        SoundData sd;
        sd.format = MakeWaveFormat(pcm.channels, pcm.rate);
        const size_t pcmBytes = layout->bytes;
        sd.pcmData.resize(pcmBytes);
        memcpy(sd.pcmData.data(), pcm.samples.data(), pcmBytes);
        sd.valid = true;

        if (s_sounds.size() >= static_cast<size_t>(std::numeric_limits<int>::max())) {
            fprintf(stderr, "[audio] Sound handle limit reached: %s\n", filepath);
            return 0;
        }
        // Store and return handle
        AudioHandle handle = static_cast<AudioHandle>(s_sounds.size());
        s_sounds.push_back(std::move(sd));
        return handle;
    } catch (const std::bad_alloc&) {
        fprintf(stderr, "[audio] allocation failed: %s\n", filepath);
        return 0;
    } catch (const std::length_error&) {
        fprintf(stderr, "[audio] PCM storage limit: %s\n", filepath);
        return 0;
    }
}
void audio_unload_sound(AudioHandle handle)
{
    if (!s_initialized) return;
    if (handle <= 0 || handle >= static_cast<int>(s_sounds.size())) return;

    // BGM 이 이 핸들을 사용 중이면 정지
    if (s_currentMusic == handle)
        audio_stop_music();
    // 음악이 토글 off 상태(s_currentMusic == 0)로 언로드되는 경우에도
    // 언로드된 핸들로의 off→on 복원은 막는다.
    if (s_lastMusic == handle)
        s_lastMusic = 0;

    // 이 핸들의 PCM 을 재생 중인 SFX 보이스를 먼저 멈춘다. 이 단계가 없으면
    // 아래 pcmData 해제가 XAudio2 가 아직 읽고 있는 메모리를 날려버린다
    // (효과음이 울리는 중에 Game 이 소멸하는 재시작 경로에서 실제로 발생).
    for (int i = 0; i < MAX_SFX_VOICES; ++i)
    {
        if (!s_sfxVoices[i] || s_sfxHandles[i] != handle) continue;
        // DestroyVoice waits until this voice can no longer read the PCM.
        // A bounded polling timeout is not proof that the buffer is unused.
        s_sfxVoices[i]->DestroyVoice();
        s_sfxVoices[i] = nullptr;
        s_sfxFormats[i] = {};
        s_sfxHandles[i] = 0;
    }

    s_sounds[handle].pcmData.clear();
    s_sounds[handle].pcmData.shrink_to_fit();
    s_sounds[handle].valid = false;
}

void audio_play_sound(AudioHandle handle)
{
    if (!s_initialized) return;
    if (!s_sfxEnabled) return;
    if (handle <= 0 || handle >= static_cast<int>(s_sounds.size())) return;
    if (!s_sounds[handle].valid) return;

    const SoundData& sd = s_sounds[handle];

    // 보이스 풀에서 idle 보이스 찾기
    int slot = -1;
    for (int i = 0; i < MAX_SFX_VOICES; ++i)
    {
        if (!s_sfxVoices[i])
        {
            // 빈 슬롯 — 보이스 생성
            slot = i;
            break;
        }
        XAUDIO2_VOICE_STATE state;
        s_sfxVoices[i]->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued == 0)
        {
            // idle — 포맷 일치 확인
            if (FormatMatches(s_sfxFormats[i], sd.format))
            {
                slot = i;
                break;
            }
            // 포맷 불일치 — 파괴 후 재생성
            s_sfxVoices[i]->DestroyVoice();
            s_sfxVoices[i] = nullptr;
            s_sfxHandles[i] = 0;
            slot = i;
            break;
        }
    }

    // A full pool steals the oldest successful start, not a fixed index.
    if (slot == -1)
    {
        slot = static_cast<int>(s_sfxOrder.oldest());
        // Stop/Flush may still be pending on the audio thread. End the old
        // borrow before overwriting the only handle that tracks its PCM.
        s_sfxVoices[slot]->DestroyVoice();
        s_sfxVoices[slot] = nullptr;
        s_sfxFormats[slot] = {};
        s_sfxHandles[slot] = 0;
    }

    // 보이스가 없으면 생성
    if (!s_sfxVoices[slot])
    {
        HRESULT hr = s_xaudio->CreateSourceVoice(&s_sfxVoices[slot], &sd.format);
        if (FAILED(hr))
        {
            if (first_failure(PlaybackFailure::sfx_create))
                fprintf(stderr, "[audio] CreateSourceVoice failed: 0x%08lx\n", static_cast<unsigned long>(hr));
            return;
        }
        s_sfxFormats[slot] = sd.format;
    }

    // 버퍼 제출 및 재생
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = static_cast<UINT32>(sd.pcmData.size());
    buf.pAudioData = sd.pcmData.data();
    buf.Flags      = XAUDIO2_END_OF_STREAM;

    s_sfxVoices[slot]->SetVolume(s_sfxVol);
    HRESULT hr = s_sfxVoices[slot]->SubmitSourceBuffer(&buf);
    if (SUCCEEDED(hr)) hr = s_sfxVoices[slot]->Start();
    if (FAILED(hr))
    {
        // Start can fail after submission, so release the borrow explicitly.
        s_sfxVoices[slot]->DestroyVoice();
        s_sfxVoices[slot] = nullptr;
        s_sfxFormats[slot] = {};
        s_sfxHandles[slot] = 0;
        if (first_failure(PlaybackFailure::sfx_start))
            fprintf(stderr, "[audio] SFX submit/start failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        return;
    }
    s_sfxHandles[slot] = handle;
    s_sfxOrder.mark_started(static_cast<size_t>(slot));
}

// 실제 BGM 보이스를 생성·시작한다 (s_musicEnabled 검사는 호출부가 한다).
static void start_music_voice(AudioHandle handle)
{
    if (!s_initialized) return;
    if (handle <= 0 || handle >= static_cast<int>(s_sounds.size())) return;
    if (!s_sounds[handle].valid) return;

    const SoundData& sd = s_sounds[handle];

    // 새 소스 보이스 생성
    HRESULT hr = s_xaudio->CreateSourceVoice(&s_musicVoice, &sd.format);
    if (FAILED(hr))
    {
        if (first_failure(PlaybackFailure::music_create))
            fprintf(stderr, "[audio] CreateSourceVoice (music) failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        return;
    }

    // 무한 루프 버퍼 제출
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = static_cast<UINT32>(sd.pcmData.size());
    buf.pAudioData = sd.pcmData.data();
    buf.Flags      = XAUDIO2_END_OF_STREAM;
    buf.LoopCount  = XAUDIO2_LOOP_INFINITE;

    s_musicVoice->SetVolume(s_musicVol);
    hr = s_musicVoice->SubmitSourceBuffer(&buf);
    if (SUCCEEDED(hr)) hr = s_musicVoice->Start();
    if (FAILED(hr))
    {
        // Submission may already have lent the PCM to the worker.
        s_musicVoice->DestroyVoice();
        s_musicVoice = nullptr;
        s_currentMusic = 0;
        if (first_failure(PlaybackFailure::music_start))
            fprintf(stderr, "[audio] Music submit/start failed: 0x%08lx\n",
                static_cast<unsigned long>(hr));
        return;
    }
    s_currentMusic = handle;
}

void audio_play_music(AudioHandle handle)
{
    if (!s_initialized) return;
    if (handle <= 0 || handle >= static_cast<int>(s_sounds.size())) return;
    if (!s_sounds[handle].valid) return;

    // 기존 BGM 정지
    audio_stop_music();

    // off→on 복원을 위해 항상 마지막 요청 핸들을 기억하고,
    // 음악이 켜져 있을 때만 실제로 시작한다.
    s_lastMusic = handle;
    if (s_musicEnabled) start_music_voice(handle);
}

void audio_stop_music()
{
    if (s_musicVoice)
    {
        s_musicVoice->Stop();
        s_musicVoice->FlushSourceBuffers();
        s_musicVoice->DestroyVoice();
        s_musicVoice = nullptr;
    }
    s_currentMusic = 0;
    // 명시적 정지는 off→on 복원 대상도 지운다 (SDL 백엔드와 동일 의미).
    // 토글 off 경로(audio_set_music_enabled)는 여길 거치지 않아 복원이 유지된다.
    s_lastMusic = 0;
}

void audio_set_music_enabled(bool on)
{
    s_musicEnabled = on;
    if (!s_initialized) return;
    if (on) {
        // 마지막으로 요청된 음악을 다시 재생 (아직 재생 중이 아니면).
        if (!s_musicVoice && s_lastMusic > 0) start_music_voice(s_lastMusic);
    } else {
        // 음악 보이스만 정지. s_lastMusic 은 유지 — on 시 복원.
        if (s_musicVoice)
        {
            s_musicVoice->Stop();
            s_musicVoice->FlushSourceBuffers();
            s_musicVoice->DestroyVoice();
            s_musicVoice = nullptr;
        }
        s_currentMusic = 0;
    }
}

void audio_set_sfx_enabled(bool on)
{
    s_sfxEnabled = on;
}

void audio_set_music_volume(float v01)
{
    v01 = audio_mix::normalize_gain(v01);
    s_musicVol = v01;
    if (s_musicVoice) s_musicVoice->SetVolume(s_musicVol);  // 재생 중이면 즉시 반영
}

void audio_set_sfx_volume(float v01)
{
    v01 = audio_mix::normalize_gain(v01);
    s_sfxVol = v01;  // 다음 audio_play_sound 부터 적용
}
