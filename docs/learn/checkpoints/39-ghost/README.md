# 39차시 누적 체크포인트 — 착지 예측

38-score의 점수·큐·종료 상태를 유지하고 읽기 전용 고스트와 회색 표시를 추가한다.

```sh
cmake -S docs/learn/checkpoints/39-ghost -B out/study-39 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-39
./out/study-39/ghost_demo
ctest --test-dir out/study-39 --output-on-failure
```

T 스폰과 장애물(10,4)을 세 번 조회한다.
`active=0 ghost=8 distance=8 score=0 elapsed=0`이 세 번 출력된다.

창은 SDL2 개발 패키지·pkg-config·OpenGL 3.3 Core 환경에서 실행한다.

```sh
cmake -S docs/learn/checkpoints/39-ghost -B out/study-39-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-39-sdl
./out/study-39-sdl/tetris T normal
```

Left/Right: 한 번 눌러 한 칸, Up: 시계 회전, Escape/창 닫기: 종료.
I four-clears·initial-blocked·next-blocked·clear-rescue 실험도 유지한다.

- simulation/ghost.h: clear 출발점의 값 복사본에 try_down을 반복한다.
  blocked 직전의 유효 Piece와 거리를 값으로 반환한다. 이미 착지했으면 거리0이다.
  invalid는 nullopt. 범위가 확인된 네 셀의 최대 이동19회+막힘1회로 반복을 제한한다.
  보드·원본·시계·점수·큐를 변경하지 않는다.
- Round::ghost: 현재 보드/active에서 계산한다. 종료 후 active가 없으면 nullopt.
- renderer/ghost_view.h: 불투명 회색. 보드→고스트→청록색 active 순서.
- main: 초기 예측 업로드 후 이동/회전/낙하/고정이 포함된 틱 묶음마다 다시 구한다.
  24정점 VBO를 같은 이름으로 교체하고 기존 VAO를 재사용한다. 종료 시 표시 생략.
- CPU 검사는 종류별 독립 비트 마스크로 337680배치의 경로를 대조한다.
  Round의 관찰 상태 보존, 장애물 아래 빈 공간, 거리0, 좌표 범위도 확인한다.
- GL probe는 일곱 종류×세 배치와 표시 없는 경로를 세 크기에서 비교한다.
  전체 정점 저장소와 모든 RGB 픽셀, 겹침 순서·레터박스를 검사한다.

검사: `python3 scripts/check_learning_ghost.py`.
현재 SimGame의 별도 캐시 갱신 회귀는 루트 CTest `sim_ghost`다.
검수 환경과 실행 한계는 REVIEW_LOG.md에 보관한다.
