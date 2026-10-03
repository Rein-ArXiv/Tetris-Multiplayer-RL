# 91 입력 지연과 생성 일정

90-input-exchange에 로컬 생성 일정으로 소비 상한을 정하는 DelayedLockstep을 추가한다.
[HTML 강의](../../index.html#lesson-91)에서 두 시계와 도착 시간선을 읽는다.

```sh
cmake -S docs/learn/checkpoints/91-input-delay -B out/study91 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study91 -j3
ctest --test-dir out/study91 --output-on-failure
./out/study91/delay_timeline
./out/study91/delay_probe listen 0 77 2
# 다른 터미널에서 LISTEN 포트 사용
./out/study91/delay_probe connect 포트번호
```

local_limit=next_capture_tick-1-delay. 입력 생성은 소비 대기와 독립적으로 계속한다.
상대 입력은 번호를 바꾸지 않고 반대 Side에 저장한다. exact pair가 없으면 여전히 대기.
시간선은 논리 펄스를 주입하여 D2가 두 펄스 도착 공백을 메우는 상황을 재현한다.
소켓 진단은 32개를 미리 생성하고 TYPE20의16입력 묶음 두 개를 보낸다. 벽시계 paced 경기 아님.
Config{seed,0,delay},delay0..30. DONE ticks=32-delay,pending=delay,동일 역할의 양쪽 해시 일치.
EOF는 마지막D개를 자동 소비하지 않으며 앞 입력이 누락되면 실패한다.
Windows 다중 구성은 --config Release/CTest -C Release와 Release/의 exe를 사용한다.
누적 GUI 게임은 그대로 유지된다. 블로킹 수신의 전체 시간 제한은 별도다.
`python3 scripts/check_learning_delay.py`로 기준·시간선·통신·실제main정책을 검사한다.
