# 42차시 누적 체크포인트 — 공격과 가비지

41-hard-drop에 일반 공격 표·대기 가비지·두 판의 증가분 교환을 추가한다.

```sh
cmake -S docs/learn/checkpoints/42-combat -B out/study-42 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-42
./out/study-42/combat_demo
ctest --test-dir out/study-42 --output-on-failure
```

CPU 두 판 데모: 첫 틱 왼쪽 O 2줄 공격1/오른쪽 I 고정→오른쪽 pending1,주입0.
둘째 틱 오른쪽만 고정→pending0,주입1,holeCursor1. 신규 공격0,오른쪽 점유17셀.

SDL2 개발 패키지·pkg-config·desktop OpenGL 3.3 Core 환경:

```sh
cmake -S docs/learn/checkpoints/42-combat -B out/study-42-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-42-sdl
./out/study-42-sdl/tetris T garbage
```

창은 한 판에3줄을 미리 대기시키는 주입 관찰 실험이다. 두 판 CPU 연결과 구분한다.
Space 하드 드롭,Down 유지 소프트,Up 회전,Left/Right 눌림 이동,Escape/닫기 종료.
기존 normal/initial-blocked/next-blocked/clear-rescue/I four-clears 유지.

- combat.h: 정상0..4줄 공격{0,0,1,2,4}; 대기는0..20포화,음수거부.
  insert는 새 보드에 행 이동/구멍을 구성하며 원본불변,상단 점유 손실 보고.
- Round: 고정→행 정리/점수/공격→대기 주입→큐 소비/보충→최종 스폰.
  공격 총량uint64포화. 구멍은{4,8,1}의 스크립트이며 실제 양수 주입당1회 진행.
  garbage_overflow는 상단 셀 소실 종료,spawn_blocked와 구분. 종료에도 최종 보드 게시.
- Duel: 두 판을 먼저 진행한 뒤 누적 증가분을 교차 전달. 새 공격은 다음 고정부터.
  감소한 총량은 거부,좁히기 전20상한. 종료한 상대는 전달 무시,기준값은 갱신.
  한쪽 invalid이면 후보 둘 다 미게시. 한 스레드 값 계약,스레드 원자성은 아님.
  승패판정은 호출자 책임이며 이 클래스 자체는 상대 AI/네트워크가 아니다.
- 실제 SimGame과 비교: int 총량/대기 포화,주입 시20상한,id9가비지,별도 RNG,
  행 삭제 전 스폰검사와 진행 중일 때만 주입. 경계 정책을 동일하다고 가정하지 않는다.
- 현재 코드 수정: 누적 signed overflow 방지,실제 주입량 보고,상단 셀 소실 종료.
  일반 해시 기준 보존은 바뀐 경계의 호환성을 보장하지 않는다. 참여 구성요소 규칙을 맞춘다.
- 42210행/구멍/표식 변환,비대칭/동시공격·전달순서·후보보존·구멍순환을 검사한다.
- combat_probe:7종×5사례×전후×3크기=210화면. board/active/ghost VBO 및 모든 RGB.
  점수·미리보기·종료 마커는 이 GL 실험에서 제외하고 main에 유지한다.

재현: `python3 scripts/check_learning_combat.py`. 현재 게임 CTest `sim_combat`.
검수 환경은 REVIEW_LOG.md 참조.
