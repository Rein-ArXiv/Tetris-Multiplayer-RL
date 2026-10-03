// Immutable pre-80 pool functions for regression reproduction.
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

    // 모든 보이스가 바쁘면 가장 오래된(첫 번째) 보이스를 강제 중단
    if (slot == -1)
    {
        slot = 0;
        s_sfxVoices[slot]->Stop();
        s_sfxVoices[slot]->FlushSourceBuffers();
        s_sfxHandles[slot] = 0;
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
    // 이 보이스가 물고 있는 PCM 의 주인을 기록해 둔다. audio_unload_sound 가
    // 버퍼를 해제하기 전에 이 보이스를 멈춰야 하기 때문이다.
    s_sfxHandles[slot] = handle;
}
