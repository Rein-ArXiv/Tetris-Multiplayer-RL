# 30차시 누적 체크포인트 — 배열 범위와 점유 충돌

29-horizontal-move의 후보 확정 조건에 Grid 점유 검사를 추가한다.
기존 주황색 세 셀은 보존하며 활성 Piece는 별도로 움직인다. 이제 주황색 셀과
겹치는 이동은 거부한다. 이름 선택 I/J/L/O/S/T/Z(default T)는 유지한다.

```sh
cmake -S docs/learn/checkpoints/30-collision -B out/study-30 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-30
./out/study-30/collision_demo
ctest --test-dir out/study-30 --output-on-failure
```

저장소 루트·C++17·CMake 기준. CPU 데모에는 SDL 장치가 필요 없다.
T 기준점 열3은 clear, 열2는 occupied, 열-1/8은 outside다.
왼쪽·idle·오른쪽·왼쪽·왼쪽 입력의 열은3→3→3→4→3→3이다.

창에는 SDL2 개발 패키지·pkg-config·desktop OpenGL3.3 Core가 필요하다.

```sh
cmake -S docs/learn/checkpoints/30-collision -B out/study-30-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-30-sdl
./out/study-30-sdl/tetris T
```

좌우 키를 눌렀다 놓으면 한 칸씩 움직이며 같은 프레임의 양쪽 눌림 edge는 취소한다.
T의 처음 왼쪽 이동은 (1,2)의 장애물로 거부된다. 오른쪽 한 칸과 되돌아오기는 허용된다.
Escape/닫기로 종료한다. 다중 구성 생성기는 빌드 구성과 실행 경로를 맞춘다.

## 구현 경계

- simulation/collision.h: classify(const Grid&,const Piece&)의 읽기 전용 배치 조회.
  좌표 계산 실패→unrepresentable, 모든 경계 검사→outside, 점유 검사→occupied,
  모두 통과→clear. 범위와 점유가 섞이면 셀 목록 순서와 무관하게 outside가 우선이다.
- study_collision::try_shift(board,current,direction): 현재 clear·방향-1/0/1을 검사한 뒤
  candidate에 기존 경계 helper를 적용하고 점유까지 통과해야 current에 대입한다.
  idle/blocked/invalid는 원본과 Grid를 보존한다. 현재부터 겹치면 idle입력도invalid다.
- simulation/movement.h의 경계 전용 helper는 그대로 남는다. 기존 movement_demo와
  movement_probe도 경계 연산 관찰용이다. 새 main/collision_demo는 점유를 포함한다.
- Grid::get/is_empty는 범위 밖을 빈칸으로 취급하지 않는다. 그 함수의 방어 검사와
  classify의 거부 이유 우선순위는 서로 다른 책임이다.
- src/main.cpp: 시작 배치 clear 확인, 새 충돌 함수를 호출하고 moved일 때만 기존
  좌표/정점/동일 크기 VBO 갱신을 수행한다. 24정점·192바이트 계약은 유지한다.
- tests/collision_contract.cpp: 모든 장애물 위치·여러 종류/경계·실패 보존·24순열
  우선순위·빈 모서리·자기 충돌·잘못된 현재 상태·목적지와 경로의 차이.
- tests/collision_input.cpp: SCRIPTED 한 번 눌림은 blocked, hold/release는idle.
- tests/collision_real.cpp: 같은 VBO/VAO로 장애물 앞·오른벽·되돌아오는 이동 후
  실제 저장소/픽셀을 독립된 셀 마스크와 비교한다.

classify는 단일 배치만 검사한다. 목적지가 clear여도 중간 이동 경로가 clear라는
뜻은 아니다. 모양 등록/연결성/회전/게임 종료는 별도 책임이다. 활성 Piece를 먼저
Grid에 기록하지 않는다. 보드에 고정하는 상태 전이와 시간에 따른 낙하는 후속 구현이다.

검사: `python3 scripts/check_learning_collision.py`.
실제 수행한 환경·검수 범위는 PROGRESS.md·REVIEW_LOG.md에 기록한다.
