# 89 시드와 시작 조건의 협상

88-connection-lifetime에 버전·규칙 ID·시드·카운트다운·입력 지연 협상을 추가한다.
[HTML 강의](../../index.html#lesson-89)에서 순서와 실패 사례를 읽는다.

```sh
cmake -S docs/learn/checkpoints/89-seed-handshake -B out/study89 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study89 -j3
ctest --test-dir out/study89 --output-on-failure
./out/study89/seed_probe listen 0 77
# 다른 터미널에서 LISTEN 포트 사용
./out/study89/seed_probe connect 실제포트
```

양쪽 READY 출력은 role만 host/peer로 다르고 seed/countdown/delay/hash는 같다.
0과 UINT64_MAX도 시드로 전달할 수 있다. wire의 0을 실제 게임 시드로 정규화하는 책임은
누적 시뮬레이션에 있다. 카운트다운과 입력 지연은 여기서 합의만 하며 아직 실행하지 않는다.
Windows 다중 구성은 --config Release, CTest -C Release, Release/의 exe를 사용한다.

학습 wire HELLO10→OFFER11→ACK12는 현재 게임 wire와 다르다.
연결마다 협상을 한 번만 호출한다. 후속 트래픽을 위해 성공 시 연결을 유지하며,
진단 main은 초기 Round를 만들고 해시를 출력한 뒤 닫는다. 상대가 침묵하면 수신은 기다린다.
송신의 각 2초 예산은 전체 협상 시간 제한이 아니다. 인증·실제 경기 재개·자동 재시도는 없다.
`python3 scripts/check_learning_seed.py`로 코덱·통신·현재 엔진의 SEED를 검사한다.
