# 50 — 시드 RNG와 값 소유 공급기

49-seven-bag의 게임에 SeededBagSource를 연결합니다. core/rng.h는 루트 엔진의
기준 복사본입니다. SeededBagSource가 엔진과 가방을 소유하고, PieceSource의 variant가
정해진 패턴/시드 가방 중 하나를 담습니다. Round는 공급기를 복사해 소유합니다.

기존 종류·상황 인자는 그대로 사용할 수 있습니다. `tetris --seed 42`는 빈 보드에서
같은 난수 가방 순서로 시작하며 기존 FrameRunner·렌더 경로를 사용합니다. 가비지 홀은
기존 고정 실습 패턴입니다. 시드는 십진수0~UINT64_MAX이며 부호·공백·여분 글자는 거부합니다.

```sh
cmake -S docs/learn/checkpoints/50-seeded-rng -B out/study-50 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-50 --target rng_demo rng_contract
./out/study-50/rng_demo 1
ctest --test-dir out/study-50 -R '^rng_contract$' --output-on-failure
```

시드1: TZOJSLI | LOITSJZ. Round는 현재T/미리보기ZOJ로 시작합니다.

```sh
cmake -S docs/learn/checkpoints/50-seeded-rng -B out/study-50-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-50-sdl --target tetris
./out/study-50-sdl/tetris --seed 42
```

화면 실행에는 SDL과 OpenGL3.3 Core가 필요합니다. rng_demo는 창 없이 CPU에서 실행됩니다.
시드0은 엔진에서88172645463393265, 게임 공급기에서869193496018642825로 정규화합니다.
같은 미래에는 엔진 상태와 남은 가방 순서가 함께 필요합니다. 엔진 출력은 내부 상태와 다릅니다.
