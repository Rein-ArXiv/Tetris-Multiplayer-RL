# 29차시 누적 체크포인트 — 좌우 이동과 화면 갱신

28-piece-catalog에 좌우 눌림 입력·후보 검사·상태 확정·GPU 저장소 갱신을 추가한다.
종류 선택 I/J/L/O/S/T/Z(default T)는 유지한다. 모든 점유 칸이 보드 안인지 검사하며,
주황색 셀과의 점유 충돌은 다음 구현이다. 현재는 주황색 칸 위로 지나갈 수 있다.

```sh
cmake -S docs/learn/checkpoints/29-horizontal-move -B out/study-29 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-29
./out/study-29/movement_demo
ctest --test-dir out/study-29 --output-on-failure
```

저장소 루트·C++17·CMake 기준. CPU 데모는 SDL 없이 실행한다.
왼쪽 네 번·idle·오른쪽 한 번의 기준점 열은 3→2→1→0→0→0→1이다.
SCRIPTED 입력 검사에서는 한 번 눌림과 계속 누름을 구별한다.

창에는 SDL2 개발 패키지·pkg-config·desktop GL3.3 Core가 필요하다.

```sh
cmake -S docs/learn/checkpoints/29-horizontal-move -B out/study-29-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-29-sdl
./out/study-29-sdl/tetris T
```

좌우 키는 한 번 눌렀다 놓을 때 한 칸이다. 같은 프레임의 두 눌림 edge는 취소한다.
단순히 키를 계속 누르고 있는 bool 둘을 의미하지 않는다. 실제 게임의 LEFT→RIGHT
순차 처리와는 다른 정책이다. Escape/닫기로 종료한다. 다중 구성 생성기는 구성과
실행 경로를 맞춘다. --help와 인수 오류는 그래픽 초기화 전에 처리한다.

## 구현 경계

- simulation/movement.h: horizontal_intent, inside_board, try_shift.
  방향 -1/0/1과 현재 네 칸 경계를 검사한다. 후보를 값 복사하고 int64에서 열을 더해
  표현 범위·후보 경계를 확인한 뒤에만 원본에 대입한다.
  idle/moved/blocked/invalid를 구분하며, moved 이외에는 모든 필드를 보존한다.
- 기준점 자체는 보드 밖이어도 된다. 모양 등록·연결성·점유는 검사하지 않는다.
  숨겨진 스폰 행이나 바깥 상태에서 보드 안으로 복구하는 이동은 이번 계약 밖이다.
- renderer/vertex_buffer.{h,cpp}: replace_same_size는 같은 이름·정점 수로 저장소를
  다시 정의한다. DynamicDraw는 힌트다. 인수 오류는 GL 호출 없이 거부한다.
  GL 오류 후 저장소 원복은 보장하지 않으며, main은 그리기를 중단하고 정리한다.
- src/main.cpp: 세션이 Piece 값을 소유. 입력 → 이동 → 성공 시 좌표/정점/버퍼 갱신 →
  유효한 layout에서 보드와 블록 그리기. 모든 칸이 안에 있어 정점 수는24다.
  GPU 갱신은 layout 유효성 분기보다 먼저여서 최소화 상태에서도 복사본을 방치하지 않는다.
- tests/movement_contract.cpp: 상태·방향 조합, 실패 보존, 극단 기준점, 의미 경계.
- tests/buffer_replace_contract.cpp: 이름/복사/거부 시 무호출/GL 실패/정리 계약.
- tests/movement_input.cpp: SCRIPTED 눌림·유지·해제 입력의 이동 횟수.
- tests/movement_real.cpp: 같은 VBO/VAO를 유지하며 좌우 이동 후 저장소와 실제 픽셀 대조.

기존 catalog_demo/piece_demo는 앞선 데이터 관찰 도구로 남는다.
종류별 팔레트·낙하·점유 충돌·고스트·키 반복·고정 틱은 아직 추가하지 않았다.

검사: `python3 scripts/check_learning_movement.py`.
검수 환경·실제 실행 범위는 PROGRESS.md·REVIEW_LOG.md에 기록한다.
