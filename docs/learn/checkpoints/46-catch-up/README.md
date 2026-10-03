# 46차시 · 긴 프레임과 따라잡기

45-clock-accounting의 기존 구현은 보존하고 정책 비교용 CPU 도구를 추가합니다.

```sh
cmake -S docs/learn/checkpoints/46-catch-up -B out/study-46 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-46 --target budget_trace budget_contract
./out/study-46/budget_trace
ctest --test-dir out/study-46 -R '^budget_contract$' --output-on-failure
```

CatchUpClock은 정수 나노초에서 최대 6틱을 배정하고 미처리/폐기 분량을 보고합니다.
clamp_elapsed는 100ms 초과 입력을 자르고, keep_backlog는 전체 틱을 보관하며,
discard_backlog는 예산을 넘은 전체 틱을 버리고 한 틱 미만의 나머지를 유지합니다.
기존 tetris 실행기는 계속 FixedClock과 FrameRunner를 사용합니다.
