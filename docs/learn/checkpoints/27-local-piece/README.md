# 27차시 누적 체크포인트 — 로컬 모양과 배치 기준점

26-board-render를 기반으로 보드와 별개의 T 모양을 덧그린다.
기본 로컬 칸(0,1),(1,0),(1,1),(1,2), 기준점(4,3).
보드 좌표(4,4),(5,3),(5,4),(5,5)를 청록색으로 표시한다.

```sh
cmake -S docs/learn/checkpoints/27-local-piece -B out/study-27 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-27
./out/study-27/piece_demo
ctest --test-dir out/study-27 --output-on-failure
```

저장소 루트·C++17·CMake 기준. CPU 데모에는 SDL 개발 패키지가 필요 없다.
창에는 SDL2 개발 패키지·pkg-config·desktop GL3.3 Core가 필요하다.

```sh
cmake -S docs/learn/checkpoints/27-local-piece -B out/study-27-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-27-sdl
./out/study-27-sdl/tetris
```

다중 구성 생성기는 구성과 실행 경로를 맞춘다. Escape/닫기로 종료한다.
기준점은 src/piece_example.h에서 바꾸고 다시 빌드한다. 키 이동은 후속 구현이다.

## 파일과 계약

- simulation/piece.h: Shape·LocalCell·Origin·Piece 값 소유, inclusive local_bounds,
  int64 덧셈 후 int 범위 검사하는 to_board. 전체 네 좌표 또는 nullopt.
- renderer/board_geometry.h: 공통 quad_vertices 추출. 보드/블록이 같은 변환 사용.
- renderer/piece_geometry.h: 보드 안 칸만 정점화. 최대24정점, 실제 count만 사용.
- renderer/piece_scene.h: 청록색 불투명 overlay. 보드 다음에 제출, 중간 clear 없음.
- src/main.cpp: Grid와 Piece 별도 소유, 고정 상태 초기 업로드, 기존 레터박스 유지.
- src/piece_demo.cpp: 정상·위쪽부분·완전밖·아래쪽부분·int 범위초과 예제.
- tests/piece_contract.cpp: 경계주변 번역/상대차이/원점보상/복사/극단값/실제정점.
- tests/piece_real.cpp: 실제 GL 보드+블록 픽셀 비교, 겹침은 시각적으로 허용.

변환은 모양 유효성·충돌·이동 허용을 판정하지 않는다. 밖인 좌표는 정상 값일 수 있다.
표시에서 칸을 생략해도 원본 Piece/Grid를 수정하지 않는다. 전부 밖이면 count0으로
VBO 생성과 드로우를 생략한다. 모양/좌표 변경은 Mesh·GPU 저장소를 자동 갱신하지 않는다.
현재 VBO는 최초 업로드만 지원하며 동적 게임에는 갱신 경로가 추가로 필요하다.
정점/GL 코드의 독점 패스와 기본 framebuffer/write mask/depth/stencil 전제는 유지한다.

검사: `python3 scripts/check_learning_piece.py`.
검수 환경·실제 실행 범위는 PROGRESS.md·REVIEW_LOG.md에 기록한다.
