# 97 원자적 관측과 비교 기록 보관

96-round-inputs에 HashMailbox와 두 실행 타깃을 추가한다.
[HTML 강의](../../index.html#lesson-97)에서 원자적 쌍과 최신 관측/소비 차이를 읽는다.

```sh
cmake -S docs/learn/checkpoints/97-hash-observation -B out/study97 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study97 -j3
ctest --test-dir out/study97 --output-on-failure
./out/study97/observation_demo
python3 scripts/check_learning_hash_observation.py
```

observation_demo는 합법적인 두 atomic의 섞인 관측을 단계 제어로 재현한 뒤
latest 조회와 모든 쌍 소비의 차이를 보여 준다. mailbox_contract는 두 생산자와
reader/consumer를 포함해 20,000개의 쌍을 검사한다. 창은 Period부터 시작한다.
Windows 다중 구성에서는 --config Release와 CTest -C Release를 사용한다.
