# 38차시 누적 체크포인트 — 득점과 누적 상태

37-game-over에 일반 제거 점수·레벨·중력 전이와 일곱 선분 숫자 표시를 추가한다.

C++17·CMake 환경의 저장소 루트:

```sh
cmake -S docs/learn/checkpoints/38-score -B out/study-38 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-38
./out/study-38/score_demo
ctest --test-dir out/study-38 --output-on-failure
```

CPU 데모는 column5에 세로 I 네 개를 넣어 총16줄을 제거한다.
점수1000/2000/3000/5000, 줄수4/8/12/16, 레벨1/1/2/2.
초기 간격1틱, 첫 레벨 상승 뒤29틱이다. 초기 interval 인자는 영구 속도 고정 옵션이 아니다.

SDL2 개발 패키지·pkg-config·desktop OpenGL3.3Core 환경:

```sh
cmake -S docs/learn/checkpoints/38-score -B out/study-38-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-38-sdl
./out/study-38-sdl/tetris I four-clears
```

새 I마다 Up 한 번으로 세로 회전하고 좌우 이동 없이 기다린다. 창은 초기30틱 간격이다.
four-clears는 I 전용 반복 공급이다. O normal은2줄/300점, 기존 종료 실험도 유지한다.
상단 연두 숫자는총점, 콘솔은득점·누적줄·레벨·간격. Escape/닫기 종료.
다중 구성은 구성과 실행 경로를 맞춘다.

- score.h: normal_points(0~4줄,1~20레벨), Totals{points,lines}, award,
  파생level(10줄마다+1,최대20), 포화덧셈, gravity_interval. 옛레벨로득점후합계갱신.
- Round: 초기완성행이있으면nullopt. 정리된보드+서로다른4셀고정이최대4줄을보장한다.
  새 점수·행수·레벨/중력변경을후보에반영하고고정/큐/스폰과함께확정한다.
  score()/total_lines()/level()은읽기조회. last_awarded()는최근수락tick의실제저장증가량.
  다음수락tick에서0으로초기화,invalid/stopped는원래전체값보존. 최종게임오버고정도득점한다.
- score_view.h:10진자릿수를역순추출후정방향표시.0은do-while로한자리,최대20자리.
  840정점용량과사용prefixcount분리. 일반폰트가아닌고정7선분도형이다.
- main:값이바뀔때840정점VBO동일크기교체,매화면엔사용prefix만draw. 시뮬레이션수정없음.
- score_contract.cpp:점수표/레벨경계/포화/연속4회고정/정지보존/초기계약/표시길이검사.
- score_real.cpp:5게임상태+9숫자경계×3크기의전체저장소와RGB비교.

검사: `python3 scripts/check_learning_score.py`.
실제 SimGame은 int 포화·T-spin 표·별도 level 필드를 사용한다. 운영 점수 회귀는
루트 CTest sim_score에서 검사한다. 검수 환경과 한계는 REVIEW_LOG.md에 기록한다.
