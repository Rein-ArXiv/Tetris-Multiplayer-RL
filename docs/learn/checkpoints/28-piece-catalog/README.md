# 28차시 누적 체크포인트 — 블록 종류와 인스턴스

27-local-piece의 좌표 변환·그리기에 불변 종류 목록과 명령행 선택을 연결한다.
I/J/L/O/S/T/Z 중 선택하며 인수가 없으면 T다. 종류별 기본 방향을 초기 위치에
청록색으로 그린다. Grid의 주황색 세 칸과 종류별 색 정책은 바꾸지 않는다.

```sh
cmake -S docs/learn/checkpoints/28-piece-catalog -B out/study-28 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-28
./out/study-28/catalog_demo
ctest --test-dir out/study-28 --output-on-failure
```

저장소 루트·C++17·CMake 기준. CPU 데모는 SDL 개발 패키지 없이 실행한다.
창에는 SDL2 개발 패키지·pkg-config·desktop GL3.3 Core가 필요하다.

```sh
cmake -S docs/learn/checkpoints/28-piece-catalog -B out/study-28-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-28-sdl
./out/study-28-sdl/tetris I
```

종료 후 O 또는 T로 다시 실행해 비교한다. Escape/닫기로 종료한다.
다중 구성 생성기는 빌드 구성과 실행 경로를 맞춘다. 대문자 한 글자만 받는다.
`--help`는 종료 코드0, 잘못된 인수는2로 창 생성 전에 끝낸다. 실행 실패는1이다.
기존 piece_demo는 고정 배치 좌표 실험이고, 종류 선택은 catalog_demo/tetris로 확인한다.

## 파일과 계약

- simulation/catalog.h: Kind·Definition·불변 definitions, 조회와 Piece 값 복사 생성.
  ID는 L1/J2/I3/O4/S5/T6/Z7, 배열 순서는 I/J/L/O/S/T/Z다.
  find_id는 외부 int를 작은 타입으로 먼저 좁히지 않는다. find_name은 정확히 비교한다.
- simulation/piece.h: 네 로컬 칸과 기준점 값 소유. Kind는 포함하지 않는다.
  main의 selected가 선택 종류를 기억하고 run_session에는 기하 데이터만 넘긴다.
- valid_shape: 4×4 범위 확인 후 중복·상하좌우 연결성 검사. 대각선은 간선이 아니다.
- valid_catalog: ID1~7·대문자 한 바이트 이름·중복·모양 구조·20×10 안의 초기 배치.
  이름과 Kind의 의미 대응·정확한 T/J 형상·게임 충돌까지 보장하지 않는다.
- src/catalog_demo.cpp: 전 종류의 인덱스·ID·기준점·4×4 도식 출력.
- tests/catalog_contract.cpp: 종류별 독립 마스크·조회·값 복사·잘못된 표 검사.
  1820개 네 칸 집합과 각24순열의 연결성을 비트 보드 oracle과 비교한다.
- tests/catalog_real.cpp: 일곱 종류×세 화면 크기의 GL 픽셀 비교 경로.
- src/main.cpp: 이름 선택 → Piece 생성 → 창 생성 → 공통 그리기 세션.

카탈로그의 읽기 전용 정의와 Piece는 별도 값이다. T 정의는 기존 t_shape 상수로
초기화한다. 카탈로그 포인터는 정적 수명의 정의를 빌려 읽고, name의 string_view는
문자열 리터럴을 가리킨다. 함수에서 만든 임시 문자열의 view를 저장하는 구조가 아니다.

좌표 변환은 충돌 판정이 아니며, 화면에서 보드 밖 칸을 생략해도 상태는 그대로다.
이번 main은 고정 상태를 시작할 때 한 번 업로드한다. Piece를 바꾸어도 BoardCells·
Mesh·GPU 복사본은 자동 갱신되지 않는다. 키 이동에는 갱신 경로도 추가해야 한다.
종류별 규칙·회전·7-bag·난수는 후속 구현이다.

검사: `python3 scripts/check_learning_catalog.py`.
검수 환경·실제 실행 범위는 PROGRESS.md·REVIEW_LOG.md에 기록한다.
