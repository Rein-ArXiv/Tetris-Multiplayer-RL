# 96 라운드와 입력 생산 상태

95-backpressure에 RoundGate·TYPE41 코덱·RoundPlay를 추가한다.
[HTML 강의](../../index.html#lesson-96)에서 상태 전이와 소속 검사를 읽는다.

```sh
cmake -S docs/learn/checkpoints/96-round-inputs -B out/study96 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study96 -j3
ctest --test-dir out/study96 --output-on-failure
./out/study96/round_timeline
python3 scripts/check_learning_round_inputs.py
```

round_timeline은 실제 두 DelayedLockstep에 프레임을 직접 전달한다. 상대 서버/창은 필요 없다.
round_contract는 코덱·상태·출력 보존·두 게임 상태·늦은 프레임·틱 소진을 검사한다.
TYPE41은 학습용 확장이고 실제 게임 INPUT4와 호환되지 않는다. prepare 전 번호/seed/역할 합의는
호출자의 책임이다. HASH/SEED 프로토콜이 자동 변경되는 것은 아니다.
Windows 다중 구성에서는 --config Release, CTest -C Release를 사용한다.
