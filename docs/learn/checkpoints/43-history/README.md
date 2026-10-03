# 43차시 — 회전 이력과 연속 삭제

42-combat의 누적 예제에 T-spin/콤보 상태와 보상을 연결한다.

```sh
cmake -S docs/learn/checkpoints/43-history -B out/study-43 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-43
./out/study-43/history_demo
ctest --test-dir out/study-43 --output-on-failure
```

SDL2·pkg-config·desktop GL3.3 Core 환경에서는 STUDY_PLATFORM=SDL로 별도 빌드 후
`tetris T spin` 실행. Up 회전 후 자연 낙하 전에 Space로 고정하면 T-spin Single이다.
자연/소프트 낙하가 실제로 한 칸 진행하면 자격을 잃는다. 결정적 한 틱 조합은 history_demo 참조.
기존 조작과 시나리오를 유지한다. 콤보·T-spin 결과는 콘솔에 표시하며 새 배너는 없다.

- rotation_ready: 성공 회전/킥 설정, 성공 좌우/아래 이동 해제, 막힘/대기/하드 드롭 보존.
  고정 전 판정 뒤 다음 피스의 자격 초기화. 틱 보고 last_rotation_candidate와 수명이 다르다.
- t_spin.h: 유효 종류/방향/배치 확인 뒤 T·자격·고정 전3모서리. nullopt와 false 구별.
- history.h: 순수 Chain→ClearReward. 양수 삭제 고정만 연속 증가,0줄 고정은 초기화.
  연속1~2/3~4/5~6/7~8/9+에 보너스0/1/2/3/4. 콤보 점수·B2B·Mini 없음.
- score.h: T-spin0..3 기본점수400/800/1200/1600, 삭제 전 레벨 배율, 기존 포화 누적.
- 현재 SimGame은 콤보 없음. 무킥 회전과 하드 드롭 자격 보존; ApplyPlacement는 자격 해제.
  기준 체크포인트와 실제 코드가 같은 기능 집합이라고 가정하지 않는다.
- CPU 데모: 독립 세 판 T-spin0/1/2와 별도 Chain의1/1/1/0/1 연속 표.
- history_probe:18 실제GL화면=3사례×전후×3크기. 보드/활성/고스트만 비교한다.
  주입/점수 등 기존 렌더링이 사라진 것은 아니며 이 검사 범위와 main을 구별한다.

검증: `python3 scripts/check_learning_history.py`. 현재 게임 회귀 CTest `sim_t_spin`.
집필 환경과 미검증 범위는 REVIEW_LOG.md를 본다.
