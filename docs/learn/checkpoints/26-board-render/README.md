# 26차시 누적 체크포인트 — 보드를 화면에 그리기

25-grid를 기반으로 동일한 세 채운 칸을 실제 20×10 보드로 표시한다.
논리 화면320×240, 원점(110,20), 칸 시작 간격10, 칠하는 크기9.
행·열은 규칙에, 배치·색·정점은 표현 계층에 둔다.

```sh
cmake -S docs/learn/checkpoints/26-board-render -B out/study-26 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-26
./out/study-26/board_render_demo
ctest --test-dir out/study-26 --output-on-failure
```

저장소 루트·C++17·CMake 기준이다. CPU 경로는 SDL 개발 패키지 없이 빌드된다.
창에는 SDL2 개발 패키지·pkg-config·desktop GL3.3 Core가 필요하다.

```sh
cmake -S docs/learn/checkpoints/26-board-render -B out/study-26-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-26-sdl
./out/study-26-sdl/tetris
```

다중 구성 생성기는 구성과 실행 경로를 맞춘다. 창은 Escape/닫기로 종료한다.

## 구현과 계약

- simulation/grid.h와 src/board_example.h는 유지한다. main이 보드를 소유한다.
- renderer/board_geometry.h: 순수 CPU 사각형·입력 영역·정점 스냅샷.
- renderer/board_scene.h: 빈칸·채움 고정 색 프로그램 둘과 불투명 제출.
- src/main.cpp: 초기 Mesh 생성·VBO 복사·두 프로그램 연결, 레터박스와 마우스 변환.
- src/board_render_demo.cpp: 좌표·그룹·바이트 수를 터미널에서 확인.
- tests/board_render_contract.cpp: 전 정점·음수/끝/NaN/무한대·스냅샷·보드 불변.
- tests/board_render_real.cpp: GL 프레임버퍼의 배경·칸·간격·여백과 잘못된 그룹 거부.
- 이전 누적 데모·플랫폼·검사는 별도 타깃으로 유지한다.

칸 입력은 오른쪽·아래쪽 간격까지 포함한다. [110,210)×[20,220) 밖은 거부한다.
Mesh는 빈칸부터 채움 순서로 1200정점, 예제 경계1182/18, 데이터9600바이트다.
그룹 first/count는 정점 단위다. 전부 비거나 채워져 없는 그룹은 제출하지 않는다.
현재 보드는 고정 상태다. Grid 수정은 Mesh와 VBO를 자동 갱신하지 않으며 기존 upload는
초기 한 번만 허용한다. 동적 보드에는 별도 갱신 계약이 필요하다.
GL 패스는 기본 framebuffer·기본 쓰기 마스크·채움 모드와 depth/stencil 비활성을 전제로 한다.

통합 검사: `python3 scripts/check_learning_board_render.py`.
검수 환경과 실제 수행 결과는 PROGRESS.md·REVIEW_LOG.md에 기록한다.
