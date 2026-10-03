# 34차시 누적 체크포인트 — 회전 중심과 후보 확정

33-line-clear에 시계 회전·방향 상태·Up 입력을 추가한다. 킥과 반시계 회전은 아직 없다.

저장소 루트에서 C++17·CMake로 실행한다.

```sh
cmake -S docs/learn/checkpoints/34-rotation -B out/study-34 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-34
./out/study-34/rotation_demo
ctest --test-dir out/study-34 --output-on-failure
```

T의 방향0→1→2→3→0과 각 로컬 셀을 출력한다. 바닥에서 거부된 회전은 방향0·행18을 유지한다.
CPU 데모는 SDL/GL에 연결하지 않는다. 다중 구성 생성기는 구성과 실행 경로를 맞춘다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3 Core 환경에서:

```sh
cmake -S docs/learn/checkpoints/34-rotation -B out/study-34-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-34-sdl
./out/study-34-sdl/tetris T
```

좌우는 한 칸 이동, Up은 한 번 시계 회전, Escape/닫기는 종료다. 누른 채 유지해도 자동 반복하지 않는다.
I/J/L/O/S/T/Z를 선택할 수 있다. 같은 종류를 반복 생성한다. 회전·이동하지 않으면 첫 고정으로
준비된 줄이 완성된다. 회전하면 빈칸에 맞지 않을 수 있다. O는 방향만 바뀌어 화면 모양은 같다.

- rotation_math.h: 두 배 중심, 넓은 중간 정수, 셀 격자/표현 범위 검사, 행 우선 정렬.
- rotation.h: 종류·방향으로 기본 형상에서 생성, 현재 캐시 일치 검사, 후보 충돌 검사 후 확정.
- round.h: 좌우→회전→중력→고정/제거/생성. quarter는 private, 생성 시0. Step::changed는 이동과 회전을 포함한다.
- pending_controls.h: Up 눌림을 시뮬레이션 틱까지 보존하고 한 번 소비한다. 여러 눌림은 하나로 합쳐진다.
- main.cpp: 회전 성공으로 활성 메시를 갱신한다. 보드 변경은 고정으로 판단한다.
- tests/rotation_oracle.h: 독립 28개 비트 형상으로 예상 점유를 만든다.
- tests/rotation_contract.cpp: 경계·단일 장애물·네 번 회전·입력 순서·생성 초기화 검사.
- tests/rotation_real.cpp: 7종×5상태×3크기, GPU 저장소와 전체 픽셀 비교.
- tests/rotation_input.cpp: SDL 이벤트로 Up 눌림/유지/반복/해제/초점 상실 계약 검사.

검사: `python3 scripts/check_learning_rotation.py`.
현재 코드의 28개 좌표 표·실제 SimGame 회전 결과·실패 시 과거 회전 이력 보존과 비교한다.
검증 환경과 미검수 범위는 REVIEW_LOG.md에 기록한다.
