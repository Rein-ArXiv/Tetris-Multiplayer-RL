# 25차시 누적 체크포인트 — 보드 상태와 연속 배열

20행10열의 Grid를 렌더링과 분리한다. Cell은 empty/filled 두 상태이며
행·열의 유효성 검사 후 단일 std::array<Cell,200>을 읽거나 쓴다.

```sh
cmake -S docs/learn/checkpoints/25-grid -B out/study-25 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-25
ctest --test-dir out/study-25 --output-on-failure
./out/study-25/grid_demo
```

저장소 루트·C++17·CMake 기준이다. 이 CPU 경로에는 SDL 개발 패키지가 필요 없다.
창은 SDL용 별도 빌드 폴더에서 구성한다. SDL2 개발 패키지·pkg-config·GL3.3 Core가 필요하다.
다중 구성 생성기에서는 구성 이름과 실행 경로를 맞춘다.

```sh
cmake -S docs/learn/checkpoints/25-grid -B out/study-25-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-25-sdl
./out/study-25-sdl/tetris
```

## 구현 지도

- simulation/grid.h: Cell·Position·Grid. contains/index_of/position_of/get/set/is_empty/clear/cells.
- src/board_example.h: (0,0),(1,2),(19,9)의 세 칸을 채운 애플리케이션 예제.
- src/grid_demo.cpp: 문자 보드, 인덱스 왕복, optional 존재 여부, 독립 복사본 출력.
- src/main.cpp: main이 보드 소유, GL 세션이 const 참조로 읽어 점유 개수 출력.
- CMakeLists.txt: SDL/GL과 무관한 study_grid·grid_demo·grid_contract 연결.
- tests/grid_contract.cpp: 200개 대응·좌표/값 경계·실패시 상태 보존·복사/참조.
- 이전 renderer/platform/관찰 도구는 유지한다. 창 그림은 아직 레터박스의 두 사각형이다.
  보드 상태를 화면의 칸으로 바꾸는 작업은26차시에서 추가한다.

## 계약

인덱스 계산 전에 행과 열을 각각 검사한다. (0,10)은 인덱스10에 접어 넣지 않는다.
get의 nullopt는 밖, 값이 있는 Cell::empty는 빈칸이다. set은 실패하면 보드를 바꾸지 않는다.
Grid의 기본 값 복사는 독립 원소를 소유한다. cells()의 const 참조는 읽기용 대여이며
소유자의 변경을 관찰하고 소유자보다 오래 사용할 수 없다. const 참조는 스냅샷이 아니다.
여러 스레드의 동시 변경이나 외부 직렬화 형식은 제공하지 않는다.

통합 검사는 저장소 루트의 `python3 scripts/check_learning_grid.py`다.
CPU 도구에 GL/SDL 링크가 없는지, 현재 SimGrid의 별도 셀 규약, UBSan 및 잘못된
선형 경계 검사 대조군을 확인한다. 검수 환경 기록은 PROGRESS.md·REVIEW_LOG.md에 둔다.
