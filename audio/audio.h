#pragma once

// audio/audio.h -- XAudio2 오디오 인터페이스
//
// raylib의 InitAudioDevice / LoadSound / PlaySound / LoadMusicStream 을 대체한다.
// 구현: audio/audio.cpp (XAudio2 + dr_mp3)
//
// 학습 포인트:
//   raylib::InitAudioDevice()는 내부적으로 miniaudio(ma_device)를 초기화한다.
//   우리는 XAudio2 COM 인터페이스를 직접 사용한다.
//   XAudio2 오디오 그래프: Source Voice -> Mastering Voice -> 스피커

// 오디오 핸들 (내부 인덱스). 0 = 무효.
using AudioHandle = int;

// XAudio2 엔진 초기화 (CoInitializeEx + XAudio2Create + CreateMasteringVoice).
// 참조 카운팅: 여러 번 호출해도 안전 (첫 호출만 실제 초기화).
// 실패 시 false 반환 -- 이후 모든 audio_* 호출은 no-op으로 동작.
bool audio_init();

// XAudio2 엔진 종료. 참조 카운팅: 마지막 호출만 실제 해제.
void audio_shutdown();

// MP3 파일을 PCM으로 디코딩하여 메모리에 로드.
// 반환: 핸들 (0이면 실패 -- 파일 없음 등. 게임은 계속 진행).
AudioHandle audio_load_sound(const char* filepath);

// 로드된 사운드 해제.
void audio_unload_sound(AudioHandle handle);

// SFX 재생 (fire-and-forget). 같은 사운드를 동시에 여러 번 재생 가능.
void audio_play_sound(AudioHandle handle);

// BGM 재생 (루프). 이전 BGM은 자동 정지.
void audio_play_music(AudioHandle handle);

// BGM 정지.
void audio_stop_music();

// ─── 설정 토글 (렌더/오디오 전용 — SimGame/결정성 해시와 무관) ──────────────────
// BGM on/off. off: 음악 보이스 정지. on: 마지막으로 재생한 음악을 다시 재생.
// 내부에 s_musicEnabled + 마지막 음악 핸들을 기억해 on 시 자동 복원한다.
void audio_set_music_enabled(bool on);

// SFX on/off. off: audio_play_sound 가 no-op 이 된다.
void audio_set_sfx_enabled(bool on);

// ─── 볼륨 (0.0~1.0, 설정 화면 슬라이더가 구동) ─────────────────────────────────
// BGM 볼륨. 0 == 음소거. 믹스 시점에 음악 샘플에 이 게인을 곱한다.
void audio_set_music_volume(float v01);

// SFX 볼륨. 0 == 음소거. 재생되는 각 효과음에 이 게인을 곱한다.
void audio_set_sfx_volume(float v01);
