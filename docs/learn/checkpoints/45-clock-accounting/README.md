# 45차시 · 누산기와 시간 보존

44-frame-loop의 게임 코드에 CPU 진단 도구와 시간 보존 검사를 추가한 누적 체크포인트입니다.
FixedClock과 FrameRunner의 동작은 유지합니다.

```sh
cmake -S docs/learn/checkpoints/45-clock-accounting -B out/study-45 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-45 --target clock_trace clock_accounting
./out/study-45/clock_trace
ctest --test-dir out/study-45 -R '^clock_accounting$' --output-on-failure
```

- clock_trace: 프레임별 입력 나노초·이전 위상·틱 수·나머지를 출력합니다.
- clock_accounting: 수락 시간 누적합에서 독립적으로 계산한 몫·나머지와 대조합니다.
- 위상은 나노초×60 단위입니다. 초 입력의 정수 나노초 변환은 별도의 양자화 경계입니다.
- tetris 및 기존 도구·검사는 그대로 포함합니다. 화면의 내용은 변경하지 않았습니다.
