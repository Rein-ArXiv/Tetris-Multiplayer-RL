# 79 규칙 사건과 소리 요청

78-mixing에서 누적한다. FrameReport를 최대 24개 요청의 이동 전용 Batch로 변환하고,
run_session에서 한 번 소비한다. 종류별 네 슬롯은 준비한 100ms 합성 PCM을 소유한다.
서로 다른 종류는 믹싱하고 같은 종류는 해당 슬롯을 재시작한다.

```sh
cmake -S docs/learn/checkpoints/79-sound-events -B out/study79 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study79 -j3
ctest --test-dir out/study79 --output-on-failure
cd docs/learn/checkpoints/79-sound-events
../../../../out/study79/tetris
```

SDL 게임에서 사건 효과음을 연결한다. SCRIPTED는 장치 없는 사건·규칙 검사에 사용한다.
Session 준비 실패는 규칙 진행을 막지 않는다. 출발/재시작 시 소리를 중단하며,
게임오버가 발생한 마지막 틱은 소비한다. 개별 요청은 샘플 시각을 예약하지 않는다.
원본 FrameReport를 다시 투영하면 새 묶음이 생기므로 main에서 프레임당 한 번만 만든다.
