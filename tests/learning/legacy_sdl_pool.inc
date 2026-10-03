// Immutable pre-80 pool functions for regression reproduction.
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
}

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
