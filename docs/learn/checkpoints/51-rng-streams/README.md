# 51 — 조각·가비지·표현 난수 분리

50의 값 소유 공급기를 유지하고 HoleSource를 Round에 추가합니다. create_seeded는
같은 요청 시드에서 조각과 가비지 엔진을 따로 만듭니다. 가비지 시드는 게임 시드0
정규화 후0x9E3779B97F4A7C15와 XOR하며, 결과0은 엔진 자체의0정책으로 처리합니다.

기존 create와 고정 실습은{4,8,1}홀을 유지합니다. 양수로 누적한 묶음을 실제로
삽입할 때에만 한 번 next를 호출합니다. 잠금 전1줄+2줄은3줄 묶음 하나입니다.

AccentNoise는 run_session이 소유하고 잠금 보고마다 배경의 작은 색 변화를 고릅니다.
난수와 색은 표현 경계에만 있으며 Round는 이를 받지 않습니다. 색은 잠금 사이에 유지합니다.

```sh
cmake -S docs/learn/checkpoints/51-rng-streams -B out/study-51 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-51 --target streams_demo streams_contract
./out/study-51/streams_demo
ctest --test-dir out/study-51 -R '^streams_contract$' --output-on-failure
```

표현0/1/37회 모두 현재Z·구멍9·주입3과 같은 두 규칙 RNG 상태를 얻습니다.
공유 엔진 반례의 다음 규칙 인덱스는5/1/2로 달라집니다.

```sh
cmake -S docs/learn/checkpoints/51-rng-streams -B out/study-51-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-51-sdl --target tetris
./out/study-51-sdl/tetris --seed 1 garbage
```

Space로 고정하면 대기3줄을 같은 홀로 주입합니다. garbage를 생략하면 시드 가방의
기본 경기입니다. 기존`tetris T garbage`는 고정 홀 실습입니다. 화면에는SDL/OpenGL3.3 Core가
필요하며 CPU검사는창없이실행됩니다. 별도상태가통계적독립성을증명하지는않습니다.
