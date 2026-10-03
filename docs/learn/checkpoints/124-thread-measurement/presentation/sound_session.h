#pragma once
// Build-time adapter: the policy sees only the common device control contract.
#include "presentation/sound_policy.h"
#if defined(STUDY_AUDIO_SDL) && defined(STUDY_AUDIO_XAUDIO2)
#error "Choose exactly one audio backend"
#endif
#if defined(STUDY_AUDIO_XAUDIO2)
#include "audio/xaudio_player.h"
namespace study_sound { using Session = BasicSession<study_audio::XAudioPlayer, true>; }
#elif defined(STUDY_AUDIO_SDL)
#include "audio/player.h"
namespace study_sound { using Session = BasicSession<study_audio::Player, true>; }
#else
namespace study_sound {
struct NoAudioDevice {};
using Session = BasicSession<NoAudioDevice, false>;
}
#endif
