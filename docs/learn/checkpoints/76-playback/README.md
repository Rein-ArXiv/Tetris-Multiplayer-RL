# 76 재생 수명

음원 Pcm16과 비소유 Voice, SDL 장치 소유자 Player를 추가한다. 누적 게임 main은 유지한다.

```sh
cmake -S docs/learn/checkpoints/76-playback -B out/study76 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study76 -j3
ctest --test-dir out/study76 --output-on-failure
./out/study76/playback_probe docs/learn/checkpoints/76-playback/assets/rotate.mp3
```

예상 `submitted frames=16128`. 출력 버퍼로의 제출 완료이며 스피커 청취 완료 판정이 아니다.
장치 없는 환경은 `SDL_AUDIODRIVER=dummy`로 실행한다. CTest의 재생 검사는 dummy를 사용한다.
SCRIPTED 빌드는 장치 없는 Voice 검사까지 포함한다. SDL 전용 타깃은 study_playback, playback_probe, playback_contract다.

- audio/voice.h: 프레임 커서, PCM 대여, 완전한 프레임 복사와 0채움.
- audio/player.h/.cpp: 주소가 고정된 장치 소유자, 실패 정리와 콜백 동기화.
- tools/playback_probe.cpp: 디코드·소유권 인계·제출 관찰·종료.
- tests/voice_contract.cpp: 순수 복사/커서 검사.
- tests/playback_contract.cpp: 실제 SDL dummy 콜백과 자원 수명 검사.
