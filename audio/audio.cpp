// audio/audio.cpp -- XAudio2 + dr_mp3 구현
//
// 학습 포인트:
//   이 파일이 하는 일 = raylib의 raudio.c (miniaudio 래퍼) 를 직접 구현한 것.
//   XAudio2 그래프: Source Voice -> Mastering Voice -> 스피커
//   MP3 디코딩은 dr_mp3 (단일 헤더, public domain).
//
// 설계 원칙:
//   - 오디오 실패는 비치명적. s_initialized 가 false 이면 모든 함수는 no-op.
//   - 참조 카운팅으로 멀티플레이(두 Game 인스턴스)에서도 안전.
//   - SFX 는 fire-and-forget 보이스 풀, BGM 은 루프 재생 전용 보이스.

#define DR_MP3_IMPLEMENTATION
#include "../third_party/dr_mp3.h"

#include "audio.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Windows / XAudio2
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
static bool                      s_comOwned     = false;  // 우리가 CoInitialize 했는가?
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
static IXAudio2SourceVoice*      s_sfxVoices[MAX_SFX_VOICES] = {};
static WAVEFORMATEX              s_sfxFormats[MAX_SFX_VOICES] = {};

// ─── 내부 유틸 ──────────────────────────────────────────────────────────────────

static WAVEFORMATEX MakeWaveFormat(drmp3_uint32 channels, drmp3_uint32 sampleRate)
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

    // COM 초기화. 이미 다른 곳에서 초기화했으면 S_FALSE 반환 -- 괜찮다.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == S_OK)
    {
        s_comOwned = true;
    }
    else if (hr == S_FALSE)
    {
        // 이미 초기화됨 (다른 스레드/모듈). COM 사용 가능.
        s_comOwned = true;  // CoUninitialize 호출 필요 (S_FALSE 도 짝 맞춰야 함)
    }
    else if (hr == RPC_E_CHANGED_MODE)
    {
        // 다른 스레딩 모델로 이미 초기화됨. XAudio2 는 대부분 동작하지만 경고.
        fprintf(stderr, "[audio] COM already initialized with different threading model\n");
        s_comOwned = false;
    }
    else
    {
        fprintf(stderr, "[audio] CoInitializeEx failed: 0x%08lx\n", hr);
        s_initialized = false;
        return false;
    }

    // XAudio2 엔진 생성
    hr = XAudio2Create(&s_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        fprintf(stderr, "[audio] XAudio2Create failed: 0x%08lx\n", hr);
        if (s_comOwned) CoUninitialize();
        s_comOwned = false;
        s_initialized = false;
        return false;
    }

    // 마스터링 보이스 생성 (기본 오디오 출력 장치)
    hr = s_xaudio->CreateMasteringVoice(&s_masterVoice);
    if (FAILED(hr))
    {
        fprintf(stderr, "[audio] CreateMasteringVoice failed: 0x%08lx\n", hr);
        s_xaudio->Release();
        s_xaudio = nullptr;
        if (s_comOwned) CoUninitialize();
        s_comOwned = false;
        s_initialized = false;
        return false;
    }

    // sentinel slot (index 0 = 무효 핸들)
    s_sounds.clear();
    s_sounds.push_back(SoundData{{}, {}, false});

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
    if (!s_initialized) return 0;

    // 파일 전체를 메모리로 읽기
    FILE* f = fopen(filepath, "rb");
    if (!f)
    {
        fprintf(stderr, "[audio] Cannot open: %s\n", filepath);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0)
    {
        fprintf(stderr, "[audio] Empty file: %s\n", filepath);
        fclose(f);
        return 0;
    }

    std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
    fread(fileData.data(), 1, fileData.size(), f);
    fclose(f);

    // dr_mp3 로 디코딩 (signed 16-bit PCM)
    drmp3_config cfg = {};
    drmp3_uint64 totalFrames = 0;
    drmp3_int16* samples = drmp3_open_memory_and_read_pcm_frames_s16(
        fileData.data(), fileData.size(),
        &cfg, &totalFrames, nullptr);

    if (!samples || totalFrames == 0)
    {
        fprintf(stderr, "[audio] MP3 decode failed: %s\n", filepath);
        if (samples) drmp3_free(samples, nullptr);
        return 0;
    }

    // SoundData 구성
    SoundData sd;
    sd.format = MakeWaveFormat(cfg.channels, cfg.sampleRate);
    size_t pcmBytes = static_cast<size_t>(totalFrames) * cfg.channels * 2;  // 16-bit = 2 bytes
    sd.pcmData.resize(pcmBytes);
    memcpy(sd.pcmData.data(), samples, pcmBytes);
    sd.valid = true;

    drmp3_free(samples, nullptr);

    // 저장 및 핸들 반환
    AudioHandle handle = static_cast<AudioHandle>(s_sounds.size());
    s_sounds.push_back(std::move(sd));
    return handle;
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
            slot = i;
            break;
        }
    }

    // 모든 보이스가 바쁘면 가장 오래된(첫 번째) 보이스를 강제 중단
    if (slot == -1)
    {
        slot = 0;
        s_sfxVoices[slot]->Stop();
        s_sfxVoices[slot]->FlushSourceBuffers();
        if (!FormatMatches(s_sfxFormats[slot], sd.format))
        {
            s_sfxVoices[slot]->DestroyVoice();
            s_sfxVoices[slot] = nullptr;
        }
    }

    // 보이스가 없으면 생성
    if (!s_sfxVoices[slot])
    {
        HRESULT hr = s_xaudio->CreateSourceVoice(&s_sfxVoices[slot], &sd.format);
        if (FAILED(hr))
        {
            fprintf(stderr, "[audio] CreateSourceVoice failed: 0x%08lx\n", hr);
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
    s_sfxVoices[slot]->SubmitSourceBuffer(&buf);
    s_sfxVoices[slot]->Start();
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
        fprintf(stderr, "[audio] CreateSourceVoice (music) failed: 0x%08lx\n", hr);
        return;
    }

    // 무한 루프 버퍼 제출
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = static_cast<UINT32>(sd.pcmData.size());
    buf.pAudioData = sd.pcmData.data();
    buf.Flags      = XAUDIO2_END_OF_STREAM;
    buf.LoopCount  = XAUDIO2_LOOP_INFINITE;

    s_musicVoice->SetVolume(s_musicVol);
    s_musicVoice->SubmitSourceBuffer(&buf);
    s_musicVoice->Start();
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
    if (v01 < 0.0f) v01 = 0.0f;
    if (v01 > 1.0f) v01 = 1.0f;
    s_musicVol = v01;
    if (s_musicVoice) s_musicVoice->SetVolume(s_musicVol);  // 재생 중이면 즉시 반영
}

void audio_set_sfx_volume(float v01)
{
    if (v01 < 0.0f) v01 = 0.0f;
    if (v01 > 1.0f) v01 = 1.0f;
    s_sfxVol = v01;  // 다음 audio_play_sound 부터 적용
}
