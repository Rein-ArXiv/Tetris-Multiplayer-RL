# 32차시 누적 체크포인트 — 블록 고정과 다음 생성

31-gravity를 이어 아래 이동이 거부되면 네 칸을 고정하고 선택 종류를 다시 생성한다.
줄 지우기·가방·미리보기·재시작 UI는 아직 없다. 종류별 색 대신 이진 filled/empty를 유지한다.

```sh
cmake -S docs/learn/checkpoints/32-locking -B out/study-32 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-32
./out/study-32/locking_demo
ctest --test-dir out/study-32 --output-on-failure
```

저장소 루트·C++17·CMake 기준. CPU 데모는 실제 시간을 기다리지 않는다.
기본 T는270/480/630/720/750틱에 고정하며 채운 셀은8/12/16/20/24개다.
마지막 고정 뒤 스폰이 막혀 active가 없고 최종 보드는 보존된다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/32-locking -B out/study-32-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-32-sdl
./out/study-32-sdl/tetris T
```

I/J/L/O/S/T/Z 선택(default T), 좌우 눌림·자동 낙하. 선택한 종류를 반복 생성한다.
고정 셀은 입력으로 움직이지 않는다. 종료 시 콘솔 GAME OVER 한 번, 화면에 최종 보드 유지.
Escape/닫기로 종료. 새 판은 다시 실행한다. 다중 구성 생성기는 구성과 실행 경로를 맞춘다.

- simulation/locking.h: 현재 clear·서로 다른 네 셀·아래 이동 blocked를 검사.
  보드 복사본에 네 칸을 써 모두 성공하면 대입. locked 외에는 보드 보존, Piece는 항상 보존.
  모양 연결성은 카탈로그 계약이다. 단일 스레드 상태 전이이며 동시성 원자 연산은 아니다.
- simulation/round.h: 보드·optional 활성 Piece·주기·반복 생성 종류를 비공개로 소유.
  create의 nullopt는 잘못된 종류/주기. 스폰이 막히면 active 없는 종료 Round를 반환.
  입력→낙하→고정→생성→스폰 검사. candidate 전체를 확정하며 invalid는 원본 보존.
  game_over는 마지막 고정을 남기며 이후 tick은 stopped. 종료 상태는 입력도 해석하지 않는다.
  새 블록 카운터0·interval 유지. 생성한 틱에서 다시 낙하시키지 않는다.
- src/main.cpp: 결과별 보드/활성 갱신 표식을 OR로 모아 프레임당 최종 상태를 올린다.
  보드 VBO는1200정점 고정 크기. empty_vertices는 새 메시 값으로 함께 갱신한다.
  종료하면 활성 VBO가 남아 있어도 draw를 생략. GPU 실패는 세션 종료이며 CPU 롤백 없음.
- tests/locking_oracle.h: 독립 비트마스크·bool 보드로 배치와 반복 생성의 결과를 계산.
- tests/locking_contract.cpp: 모든 단일 장애물·범위·중복·순열·재기준 좌표·종료·상태 소유 검사.
- tests/locking_real.cpp: 시작/첫 고정/둘째 고정/종료의 VBO·구간·전체 픽셀 비교.

검사: python3 scripts/check_learning_locking.py.
실제 SimGame 비교는 빈 보드 첫 고정과 preview 승격까지다. 이후 종류 정책이 다르다.
전체 검증 환경·미검수 화면 범위는 REVIEW_LOG.md에 기록한다.
