// Immutable pre-81 BGM submission for regression reproduction.
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
