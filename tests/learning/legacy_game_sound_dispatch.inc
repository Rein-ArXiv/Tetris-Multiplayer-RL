void Game::SubmitInput(uint8_t inputMask)
{
    sim.SubmitInput(inputMask);
    if (sim.rotateSoundEvent)  { audio_play_sound(sndRotate);  sim.rotateSoundEvent  = false; }
    // drop 전용 에셋(Sounds/drop.mp3)이 없으면 무음 대신 rotate 로 대체해
    // 피드백을 유지한다 (audio_play_sound(0) 은 no-op). 핸들 alias 가 아니라
    // 재생 시점 fallback 이므로 소멸자의 이중 unload 가 없다.
    if (sim.dropSoundEvent)    { audio_play_sound(sndDrop ? sndDrop : sndRotate); sim.dropSoundEvent = false; }
}

void Game::Tick()
{
    sim.Tick();
    if (sim.clearSoundEvent)   { audio_play_sound(sndClear);   sim.clearSoundEvent   = false; }
    // garbage 전용 에셋이 없으면 clear 로 대체 (위 drop 과 동일 이유).
    if (sim.garbageSoundEvent) { audio_play_sound(sndGarbage ? sndGarbage : sndClear); sim.garbageSoundEvent = false; }
}

void Game::MoveBlockDown()
{
    sim.MoveBlockDown();
}

