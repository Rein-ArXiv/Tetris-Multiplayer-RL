#pragma once

// audio/audio.h -- platform-independent client audio API.
// Implemented by audio/audio.cpp (Windows XAudio2 + dr_mp3) or
// audio/sdl_audio.cpp (SDL callback mixer + dr_mp3), selected by the build.
// Game owns loaded handles; SimGame does not call this API.
// Control calls (including init/shutdown) are serialized on the main thread.
// Each backend synchronizes PCM reads with its worker; API calls stay serialized.

// 오디오 핸들 (초기화 세션 안의 내부 인덱스). 0 = 무효.
// 언로드한 인덱스는 같은 세션에서 재사용하지 않는다. 마지막 shutdown 뒤에는
// 모든 핸들이 만료되므로 다음 init 세션에 보관한 정수를 전달하면 안 된다.
using AudioHandle = int;

// 선택된 오디오 백엔드 초기화.
// 참조 카운팅: 여러 번 호출해도 안전 (첫 호출만 실제 초기화).
// 실패 시 false. 성공 여부와 관계없이 각 호출을 audio_shutdown과 짝짓는다.
bool audio_init();

// 선택된 오디오 백엔드 종료. 참조 카운팅: 마지막 호출만 실제 해제.
void audio_shutdown();

// MP3 파일을 PCM으로 디코딩하여 메모리에 로드.
// 반환: 핸들 (0이면 실패 -- 파일 없음 등. 게임은 계속 진행).
AudioHandle audio_load_sound(const char* filepath);

// 로드된 사운드 해제.
void audio_unload_sound(AudioHandle handle);

// SFX 재생 (fire-and-forget). 같은 사운드를 동시에 여러 번 재생 가능.
// 최대 8개. 빈 보이스를 먼저 사용하고 가득 차면 가장 먼저 시작한 재생을 교체한다.
// 주어진 음원이 해제될 때 해당 PCM을 참조하는 모든 보이스를 분리한다.
void audio_play_sound(AudioHandle handle);

// BGM 재생 (루프). 이전 BGM은 자동 정지.
void audio_play_music(AudioHandle handle);

// BGM 정지.
void audio_stop_music();

// ─── 설정 토글 (렌더/오디오 전용 — SimGame/결정성 해시와 무관) ──────────────────
// BGM on/off. off: 음악 보이스 정지. on: 마지막으로 요청한 음악을 다시 재생 시도.
// 내부에 s_musicEnabled + 마지막 음악 핸들을 기억해 on 시 자동 복원한다.
void audio_set_music_enabled(bool on);

// SFX on/off. off: audio_play_sound 가 no-op 이 된다.
void audio_set_sfx_enabled(bool on);

// ─── 볼륨 (0.0~1.0, 설정 화면 슬라이더가 구동) ─────────────────────────────────
// BGM 볼륨. 0 == 음소거. 믹스 시점에 음악 샘플에 이 게인을 곱한다.
void audio_set_music_volume(float v01);

// SFX 볼륨. 0 == 음소거. 재생되는 각 효과음에 이 게인을 곱한다.
void audio_set_sfx_volume(float v01);
