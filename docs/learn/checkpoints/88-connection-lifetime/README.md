# 88 연결 수명과 재시작

87-partial-send의 누적 게임·통신 진단을 유지하고 Connection을 추가한다.
소켓·파서·양방향 종료 상태를 같은 소유자가 관리한다.
본문은 [HTML 강의](../../index.html#lesson-88)에서 읽는다.

```sh
cmake -S docs/learn/checkpoints/88-connection-lifetime -B out/study88 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study88 -j3
ctest --test-dir out/study88 --output-on-failure
./out/study88/framing_probe listen 0 2
# 다른 터미널에서 LISTEN 포트 사용
./out/study88/connection_probe 실제포트
```

기대 결과: VERIFIED tick=42 count=1 send_closed=1 peer_eof=1,
CLOSED active=0 pending=0. 클라이언트가 요청 송신을 먼저 종료하고 응답을 읽는다.
Windows 다중 구성은 --config Release, CTest -C Release, Release/ 아래 exe를 사용한다.

Runtime이 Connection보다 오래 살아 있어야 한다. API는 한 호출자가 직렬로 사용한다.
연결·수신은 블로킹이고 전체 요청의 시간 제한은 없다. 송신에만 2초 예산을 적용한다.
close는 진행 중인 호출을 다른 스레드에서 취소하는 API가 아니다.
게임 wire와 다른 loopback 진단이며 인증이나 경기 재개 기능은 없다.
`python3 scripts/check_learning_connection.py`로 실제 half-close·재시작과 현재 Session을 검사한다.
