# 80 보이스 풀과 공유 음원 수명

79-sound-events에서 누적한다. 음원 슬롯과 재생 보이스를 분리하고,
같은 PCM을 여러 보이스가 독립 커서로 읽는다. 비활성 슬롯을 먼저 쓰며
9보이스가 모두 재생 중이면 가장 오래된 성공한 시작을 교체한다.

```sh
cmake -S docs/learn/checkpoints/80-voice-pool -B out/study80 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study80 -j3
ctest --test-dir out/study80 --output-on-failure
cd docs/learn/checkpoints/80-voice-pool
../../../../out/study80/tetris
```

Player의 clip slot은 소유 PCM 자리다. play(slot)은 재생 보이스를 자동 선택한다.
같은 slot의 반복 play가 겹쳐 울릴 수 있다. replace/unload는 그 PCM을 빌리는 모든
보이스(종료된 보이스의 참조 포함)를 분리하고 잠금 밖에서 저장소를 해제한다.
cursor_frames(slot)은 아직 풀에 남아 있는 가장 최근 재생의 커서이며 모든 참조가
교체/해제되면 0이다. 영구적인 재생 기록이나 개별 보이스 제어 핸들이 아니다.
