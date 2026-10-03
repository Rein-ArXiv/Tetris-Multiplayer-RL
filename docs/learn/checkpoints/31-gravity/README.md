# 31차시 누적 체크포인트 — 틱으로 진행하는 자연 낙하

30-collision을 이어 자연 낙하·호스트 시계·pending 좌우 입력을 추가한다.
활성 Piece는 Grid와 별개다. 기본 30틱마다 한 셀 아래로 시도하고 막혀도 아직 고정하지 않는다.

```sh
cmake -S docs/learn/checkpoints/31-gravity -B out/study-31 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-31
./out/study-31/gravity_demo
ctest --test-dir out/study-31 --output-on-failure
```

저장소 루트·C++17·CMake 기준. CPU 실행은 시계를 기다리지 않으며 30틱마다 결과를 출력한다.
T 기준점은 240틱에 행8,270틱에도 행8(blocked)이다. (10,4)의 새 장애물 앞에서 멈춘다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/31-gravity -B out/study-31-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-31-sdl
./out/study-31-sdl/tetris T
```

I/J/L/O/S/T/Z(default T) 선택 유지. 눌렀다 놓는 좌우 이동과 자동 낙하.
T가 장애물에 멈추면 오른쪽 두 칸으로 벗어나 낙하를 재개한다. Escape/닫기로 종료.
다중 구성 생성기는 구성과 실행 경로를 맞춘다.

- simulation/gravity.h: try_down의 후보 검사, Counter, tick의 주기/결과.
  moved/blocked는 주기를 소비해 elapsed=0; invalid는 Piece·Counter 모두 보존한다.
  Grid는 항상 읽기 전용. 좌우 이동은 카운터를 초기화하지 않는다.
- timing/fixed_clock.h: 정수 나노초×60 위상에서 1e9당 한 틱, 나머지 보존.
  100ms 초과분은 버린다. 곱셈/정수변환보다 먼저 제한. 비유한/음수 초는 상태 보존 실패.
  플랫폼 dt의 초→나노초 변환은 소수 부분을 버린다. 측정/표현 정밀도까지 보장하지 않는다.
- simulation/pending_horizontal.h: 소비 전 edge를 OR로 합친다. 반복 눌림은 합쳐지고,
  양방향은 취소한다. 완전한 사건 큐가 아니다. 0틱이면 보존, 여러 틱이면 첫 틱에서 소비.
- src/main.cpp: 입력→낙하 순서를 매 틱 유지. 마지막 위치로 GPU를 프레임당 최대 한 번 갱신.
  시계·입력·카운터는 run_session 수명. 그릴 크기가 없어도 규칙은 진행한다.
- src/gravity_example.h: 원래 세 고정 셀에 (10,4)를 더한다. 기존 데모의 보드 함수는 유지.
- tests/gravity_contract.cpp: 독립 비트마스크 기대값·모든 단일 장애물·정수경계·시간·입력순서.
- tests/gravity_real.cpp: 같은 VBO/VAO로 시작·중간·장애물 도달·재시도 위치의 픽셀 비교.

검사: `python3 scripts/check_learning_gravity.py`.
현재 SimGame과의 비교는 고정 전 빈 보드 낙하에 한정한다. 현재 게임은 막히면 LockBlock을 호출한다.
시계 어댑터는 현재 float 누산기와 다른 학습 구현이다. 테스트/실화면 수행 범위는 REVIEW_LOG.md에 기록한다.
