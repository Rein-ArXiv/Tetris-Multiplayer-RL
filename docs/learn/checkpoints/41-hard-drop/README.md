# 41차시 누적 체크포인트 — 하드 드롭과 명령 소비

40-soft-drop에 Space 입력과 즉시 착지·고정을 추가한다.

```sh
cmake -S docs/learn/checkpoints/41-hard-drop -B out/study-41 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-41
./out/study-41/hard_drop_demo
ctest --test-dir out/study-41 --output-on-failure
```

데모는 빈 판·반복 I·중력 간격30. Space 요청 하나를 세 틱에 전달한다.
첫 틱: distance18/filled4/새 row0/score0. 나머지 두 틱: distance-1/filled4/row0.

SDL2 개발 패키지·pkg-config·desktop OpenGL 3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/41-hard-drop -B out/study-41-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-41-sdl
./out/study-41-sdl/tetris T normal
```

Space 눌림: 한 번 하드 드롭. Down 유지: 4틱 주기의 소프트 시도.
Left/Right: 눌림당 한 칸. Up: 시계 회전. Escape/닫기: 종료.
normal은 지울 하단 행을 준비한 판이다. T next-blocked/clear-rescue는 거리0 고정 실험.
I four-clears도 유지한다. 다중 구성은 구성별 실행 경로를 사용한다.

- PendingControls: Space pressed는 OR로 보관하고 consume에서 지운다.
  bool은 여러 사건을 하나로 합치며 물리 입력 횟수를 전부 보존하는 큐가 아니다.
- Round: 좌우→회전→소프트 시계 갱신. hard_drop이면 project의 최신 후보 착지를 대입하고
  finish_lock. 소프트 이동·자연 중력 생략. false이면 기존 낙하 경로. 한 틱 최대1고정.
- finish_lock은 후보 상태만 고정·행 정리·점수·큐·스폰 순으로 변경한다.
  tick은 invalid면 게시하지 않고, locked/game_over면 게시한다. 스레드 원자성은 아님.
- last_hard_drop_distance: 수락된 틱에 드롭 없음 -1, 드롭 있음 0 이상.
  읽으면 지워지는 큐가 아니다. main은 고정 결과 직후 읽는다.
- main은 잠금 결과로 board_changed/piece_changed를 유지하고 최종 메시·그리기 범위를 갱신한다.
- 실제 SimGame은 원본 한 칸 초과→복원, 소프트 뒤 회전·하드 드롭, 별도 Tick이다.
  스폰 충돌을 행 삭제 전에 확인하는 차이도 강의에서 설명한다. 운영 규칙은 바꾸지 않았다.
- 규칙 검사: 5516 장애물/시계 조합, 독립 보드 필터·큐·거리0·회전/열·요청 소비·보존.
- hard_drop_probe: 7종×4보드×전후×3크기=168화면. 보드·active·ghost VBO 및 모든 RGB.
  보드/활성/고스트만 그리는 별도 실험이며 main의 점수·미리보기·종료 마커와 구별한다.

재현: `python3 scripts/check_learning_hard_drop.py`. 현재 게임 CTest `sim_hard_drop`.
검증 환경과 미검증 범위는 REVIEW_LOG.md 참조.
