# 90 틱별 입력 교환

89-seed-handshake에 고정 창 TickInputs와 순수 Duel 실행기 Lockstep을 추가한다.
[HTML 강의](../../index.html#lesson-90)에서 구현 순서와 실패 사례를 읽는다.

```sh
cmake -S docs/learn/checkpoints/90-input-exchange -B out/study90 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study90 -j3
ctest --test-dir out/study90 --output-on-failure
./out/study90/input_probe listen 0 77
# 다른 터미널에서 LISTEN 포트 사용
./out/study90/input_probe connect 포트번호
```

양쪽은 Config{seed,0,0}에 합의한다. 카운트다운·추가 입력 지연은 0이다.
자신의 0~11 입력을 저장하고 TYPE20으로 6~11,0~5 순서의 두 묶음을 전송한다.
WAIT next=0 후 DONE ticks=12, 양쪽 left끼리/right끼리의 해시 일치를 확인한다.
같은 기기의 두 프로세스만 연결하는 루프백 진단이며 누적 GUI 게임은 별도로 유지된다.
Windows 다중 구성은 --config Release, CTest -C Release, Release/의 exe를 사용한다.

창은 32틱, 메시지당 최대 16입력, 실험은 총12틱으로 서로 다른 제한을 가진다.
같은 pending 입력은 duplicate, 다른 값은 conflict, 이미 소비한 번호는 stale이다.
충돌/유효하지 않은 묶음/누락 EOF는 실패한다. 수신은 블로킹이며 전체 시간 제한이 없다.
송신 완료는 상대가 시뮬레이션에 적용했다는 확인 응답이 아니다.
`python3 scripts/check_learning_lockstep.py`로 순수 계약·실제 두 프로세스·현재 게임 입력 조회를 검사한다.
