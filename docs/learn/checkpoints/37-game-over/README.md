# 37차시 누적 체크포인트 — 종료 상태와 판정 순서

36-next-queue에 스폰 결과 분류, 지속 종료 사유, 종료 표시를 추가한다.

저장소 루트, C++17·CMake 환경:

```sh
cmake -S docs/learn/checkpoints/37-game-over -B out/study-37 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-37
./out/study-37/end_demo
ctest --test-dir out/study-37 --output-on-failure
```

O 데모의 세 줄은 초기 종료(cursor4), 고정 뒤 종료(cursor5), 두 줄 제거 후 진행(cursor5)이다.
세 경우 모두 다음 큐 크기는3이다. CPU 데모는 중력 주기1틱을 사용한다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3Core 환경:

```sh
cmake -S docs/learn/checkpoints/37-game-over -B out/study-37-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-37-sdl
./out/study-37-sdl/tetris O initial-blocked
./out/study-37-sdl/tetris O next-blocked
./out/study-37-sdl/tetris O clear-rescue
```

한 창을 Escape로 닫고 다음을 실행한다. 무입력으로 비교한다. 창의 중력은30틱 주기다.
기본 normal은 바닥에서 줄을 완성하는 보드. I/J/L/O/S/T/Z 종류를 선택할 수 있다.
좌우 한 칸·Up 시계 회전/킥·Escape/닫기. 다중 구성은 구성과 실행 경로를 맞춘다.

- simulation/spawn.h: clear→ready, outside/occupied→blocked, unrepresentable→invalid.
  읽기 전용 분류. 계산 오류를 패배로 바꾸지 않는다.
- simulation/round.h: EndReason으로 지속 종료 사유 보존; finished는 사유에서 계산한다.
  외부 관찰 시 active 존재 iff 진행 중. 초기 blocked도 유효한 종료 Round이다.
  고정→행 정리→큐 소비/보충→스폰 검사. 정상 종료도 전이를 확정한다.
  tick의 game_over는 사건, 이후 stopped는 모든 규칙 상태를 보존한다.
- src/spawn_example.h: 초기/고정 뒤 충돌과 행 제거 구제 사례. 라이브 규칙과 별도의 실습 보드.
- src/end_message.h: 규칙 enum을 표현 문장으로 변환한다.
- renderer/end_marker.h: 보드 왼쪽의 불투명 X,12정점. board pass의 상태를 빌리고 clear하지 않는다.
- main: 초기 active 부재를 허용한다. 미리보기와 공유하는 청록 프로그램은 항상 준비한다.
  종료 뒤에도 이벤트·렌더·프레임 대기를 처리하고 Escape/닫기에서만 루프를 종료한다.
- tests/end_contract.cpp: 독립 배치/행 압축 기대값, 초기/고정 종료, 불변식, 정지 후 전체 보존.
- tests/end_real.cpp: 126 offscreen GL 화면 비교. X 사선 경계 샘플만 두 인접 색 중 하나를 허용한다.

검사: `python3 scripts/check_learning_end.py`.
현재 SimGame은 행 제거 전 스폰 검사 정책이며 종료 원인 enum은 없다.
검사 도구는 실제 SimGame 호출로 차이를 비교한다. 실행 환경은 REVIEW_LOG.md에 기록한다.
