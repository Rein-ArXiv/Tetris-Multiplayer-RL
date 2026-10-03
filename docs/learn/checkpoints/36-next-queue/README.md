# 36차시 누적 체크포인트 — 다음 큐와 값 소유권

35-kicks에 세 칸 FIFO와 고정 순서 공급자를 추가한다. 난수/7-bag는 아직 구현하지 않는다.

저장소 루트, C++17·CMake 환경:

```sh
cmake -S docs/learn/checkpoints/36-next-queue -B out/study-36 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-36
./out/study-36/queue_demo
ctest --test-dir out/study-36 --output-on-failure
```

O에서 시작하면 현재O/다음S,T,Z, 첫 고정 뒤 현재S/다음T,Z,I, 둘째 뒤 현재T/다음Z,I,J다.
호출자 공급자의 cursor는0을 유지한다. Round가 값 복사본을 진행한다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/36-next-queue -B out/study-36-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-36-sdl
./out/study-36-sdl/tetris O
```

오른쪽 세 모양은 위부터 다음 순서다. Up 시계 회전/킥, 좌우 한 칸, Escape/닫기 종료.
선택한 종류부터 I,J,L,O,S,T,Z 카탈로그를 순환한다. 첫 보드는 해당 종류가 줄을 완성하도록 준비된다.
다중 구성 생성기는 구성과 실행 경로를 맞춘다.

- next_queue.h: 세 슬롯을 모두 쓰는 head+size 원형 FIFO. peek/pop은Kind 값 반환.
  unknown/full push, empty pop, 범위 밖 peek는 실패하며 상태를 유지한다.
- scripted_source.h: 검증한 최대7종 패턴을 값 소유. cycle(first),repeating(kind),next().
  큐는 순서를 생성하지 않고 공급자는 큐의 저장 방식을 모른다.
- round.h: source 인자 오버로드는 공급자를 복사하고 현재1+다음3을 준비한다.
  고정→행정리→앞종류준비→pop/보충→스폰검사. 스폰충돌도정상소비1회후종료.
  invalid/stopped는공급자/큐포함전체보존. kind()는활성또는최근스폰시도종류.
  Kind인자오버로드는이전독립실험의같은종류반복을위해repeating공급자로위임한다.
  실제main은cycle공급자를사용한다. quarter와gravity초기화규칙유지.
- next_preview.h: peek으로기본형상3개를72정점에복사. 읽기로소비하지않음.
  main은고정후같은VBO를갱신. 보드의좌표나활성회전을미리보기위치로사용하지않음.
- queue_contract.cpp: std::deque독립모델,감김/가득참/조회/복사/승격/실패소비/정지보존.
- queue_real.cpp: 7시작종류×4상태×3크기의활성/보드/미리보기저장소와전체픽셀비교.

검사: `python3 scripts/check_learning_queue.py`.
현재SimGame의조회무소비·값승격·삭제/보충은실제하드드롭호출로비교한다.
검증환경과미검수범위는REVIEW_LOG.md에기록한다.
