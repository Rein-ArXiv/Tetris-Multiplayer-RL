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
        fprintf(stderr, "[audio] SFX submit/start failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        return;
    }
    s_sfxHandles[slot] = handle;
    s_sfxOrder.mark_started(static_cast<size_t>(slot));
}

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
        fprintf(stderr, "[audio] Music submit/start failed: 0x%08lx\n",
                static_cast<unsigned long>(hr));
        return;
    }
    s_currentMusic = handle;
}

