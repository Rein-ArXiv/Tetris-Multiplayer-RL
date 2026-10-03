# 33차시 누적 체크포인트 — 줄 제거와 안정적인 행 압축

32-locking의 고정과 생성 사이에 clear_full_rows를 추가한다.
같은 종류 반복 생성·좌우 눌림·자연 낙하는 유지한다. 회전·점수·효과는 아직 없다.

```sh
cmake -S docs/learn/checkpoints/33-line-clear -B out/study-33 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-33
./out/study-33/lines_demo
ctest --test-dir out/study-33 --output-on-failure
```

저장소 루트·C++17·CMake 기준. CPU 데모는O와 두 줄 완성 직전 보드다.
570틱 고정으로18+4−20=2셀이 남고 표식(5,0)/(12,9)가(7,0)/(14,9)로 이동한다.
새O는행0에서 시작한다. 실제 시간을 기다리지 않고 틱을 계산한다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/33-line-clear -B out/study-33-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-33-sdl
./out/study-33-sdl/tetris O
```

O는두줄,기본T와I/J/L/S/Z는한줄완성직전보드다.입력없이두면첫고정에완성한다.
좌우로옮기면빈칸에서벗어나완성되지않을수있다.같은종류반복생성.
Escape/닫기로종료.다중구성생성기는구성과실행경로를맞춘다.

- simulation/lines.h: row_full은범위밖false. clear_full_rows는아래부터read/write.
  반복시작write>=read,남는행순서유지,빈행도남김.마지막0..write를비우고0~20반환.
  유효내부인덱스로만접근,동적할당없음,O(RC)시간/O(1)작업공간.
- simulation/round.h: 고정→active해제→줄정리→새생성/스폰검사.
  현재production의줄정리전스폰검사와규칙순서가다르다.자동동일시하지않는다.
  last_cleared는최근정상진행틱의제거수.매candidate에서0,고정시반환값저장.
  invalid/stopped는전체보존이므로값도보존한다.고정결과와함께읽어야한다.
- src/main.cpp: 각고정직후제거수를프레임합계로모아한번출력.
  board_changed는줄제거수와무관하게고정이면true.메시/VBO/empty_vertices를함께갱신.
- src/clear_example.h: O는18~19행열4~5를비워두고,나머지는19행의해당로컬칸을비움.
  두표식은생성열밖에있어처음낙하를방해하지않는다.
- tests/lines_contract.cpp: 2^20완성행배치와서로다른남는행태그,1024한행마스크,
  셀수·순서·멱등성·통합제거·clear-first스폰보존검사.
- tests/lines_real.cpp: 시작/바닥도달/제거와생성/새블록한행낙하의저장소와픽셀비교.

검사: python3 scripts/check_learning_lines.py.
현재SimGrid의종류ID보존변환과비교하며,실제SimGame의선행스폰종료차이를따로재현한다.
실제검증환경·사이트미검수범위는REVIEW_LOG.md에기록한다.
