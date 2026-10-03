# 35차시 누적 체크포인트 — 순서 있는 킥 후보

34-rotation의 회전에 후보 검색을 추가한다. 교육용 시계 회전 정책이며 SRS나 현재 production 규칙과 같지 않다.

저장소 루트, C++17·CMake 환경:

```sh
cmake -S docs/learn/checkpoints/35-kicks -B out/study-35 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-35
./out/study-35/kicks_demo
ctest --test-dir out/study-35 --output-on-failure
```

0~6의 일곱 사례가 벽 T, 바닥 T, 바닥 I, 좌우 선택, 두 후보 실패 뒤 성공, 전체 거부, O를 보여 준다.
후보 번호는 각각1,5,6,1,2,-1,0이다. -1은 확정 없음,0은 제자리 후보다.
다중 구성 생성기는 구성과 실행 경로를 맞춘다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/35-kicks -B out/study-35-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-35-sdl
./out/study-35-sdl/tetris T
```

Up 시계 회전, 좌우 한 칸 이동, Escape/닫기 종료. 킥이 성공하면 선택 후보 번호를 출력한다.
I/J/L/O/S/T/Z 선택과 같은 종류 반복 생성, 줄 완성용 준비 보드를 유지한다.

- simulation/kick_search.h: 항상 같은 원점에 각 오프셋을 적용하고 첫 clear 반환. 입력/보드 수정 없음.
  넓은 중간 정수와 int 결과 범위 검사. 최종 점유만 검사하며 중간 이동 경로는 검사하지 않음.
- simulation/kicks.h: 현재 상태 검증, 다음 방향 셀 생성, 검색, 성공 시 셀/원점/방향 확정.
  O는제자리만. 다른종류는0/좌우1/좌우2/위1/위2의총7후보.
  출발방향0·3은왼쪽우선,1·2는오른쪽우선. 모든오프셋은시도전원점기준.
- simulation/round.h: 좌우→회전/킥→중력. 킥은중력카운터를초기화하지않는다.
  last_rotation_candidate는최근정상틱의성공후보번호. 요청없음/거부는-1,invalid/stopped는전체보존.
  새블록을생성해도그틱의결과번호는이전블록의회전사건을가리킬수있다. 다음정상틱에초기화.
- src/kicks_example.h: CPU데모입력. tests/kicks_cases.h는별도의기대결과. 게임정책과예시데이터분리.
- tests/kicks_contract.cpp: 독립비트형상과명시적오프셋표,우선순위/보존/경계/틱통합.
- tests/kicks_real.cpp: 일곱사례전후의실제저장소와픽셀비교.

검사: `python3 scripts/check_learning_kicks.py`. 현재SimGame이거부하는왼벽T를강의정책은허용함을비교한다.
검증환경·미검수범위는REVIEW_LOG.md에기록한다.
