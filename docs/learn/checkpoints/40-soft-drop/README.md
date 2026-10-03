# 40차시 누적 체크포인트 — 유지 입력과 반복 시계

39-ghost에 Down 키, 반복 타이머, 규칙 전이 연결을 추가한다.

```sh
cmake -S docs/learn/checkpoints/40-soft-drop -B out/study-40 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-40
./out/study-40/soft_drop_demo
ctest --test-dir out/study-40 --output-on-failure
```

데모: I·빈 보드·자연 간격4. Down은 1~3틱과6~12틱에 true.
행은 1,1,1,2,2,3,3,4,4,5,5,6. 출력 wait는 소프트 대기, gravity는 자연 경과다.

SDL2 개발 패키지·pkg-config·desktop OpenGL 3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/40-soft-drop -B out/study-40-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-40-sdl
./out/study-40-sdl/tetris T normal
```

Down 유지: 첫 true 틱에 시도, 이후4틱 주기. False 관찰 시 재준비.
Left/Right: 눌림당1칸. Up: 시계 회전. Escape/닫기: 종료.
기존 I four-clears와 종료 실험도 유지한다.

- soft_drop.h: Counter{remaining0,period4}. idle/waiting/due/invalid.
  범위 검사를 먼저 하고 due에서 period-1을 채운다. 이동 자체는 수행하지 않는다.
- PendingControls: Down은 최신 held 표본으로 교체, consume마다 유지.
  좌우/회전의 사건 소비와 다르다. 틱 사이 짧은 Down 탭 보존은 보장하지 않는다.
- Round: 좌우→회전→소프트→자연. due일 때 try_down, changed 신호로 메시 갱신.
  소프트 blocked면 기존 고정 경로로 가고 새 피스에 자연 중력을 쓰지 않는다.
  틱당 최대1고정. 소프트 카운터는 고정 이후에도 이어지고 자연 elapsed는 리셋한다.
- 플랫폼 Down 매핑: SDL/Win32. SCRIPTED의 기존 시나리오는 Down=false로 유지한다.
- 실제 SimGame은 cooldown3·소프트 뒤 회전·별도 Tick 호출이다. 복합 입력 순서 차이를
  강의와 비교한다. 타이머 표현 이름을 고쳤으나 기존 운영 시도 주기는 바꾸지 않았다.
- CPU: 모든12틱 유지 입력 조합, 독립 기한 모델, 범위, 제로틱/다중틱 묶음,
  고정 경계, 상태 보존. SDL 이벤트 큐 fixture와 GL 위치·색 검사를 별도로 둔다.
- soft_drop_probe: 자연 간격6/Down유지,0·1·4·5·12틱을7종·3크기에서 비교한다.
  실제 창 기본간격30과 실험의6을 구별한다. active/ghost 정점 저장소·모든 RGB를 대조한다.

재현: `python3 scripts/check_learning_soft_drop.py`.
현재 게임 회귀: CTest `sim_soft_drop`. 플랫폼 검증 범위는 REVIEW_LOG.md 참조.
