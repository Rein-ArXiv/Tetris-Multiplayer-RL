# 84 TCP 스트림과 수신 호출의 경계

83-sockets의 누적 게임과 한 바이트 진단을 유지한다.
새 stream_probe는 여러 송신 묶음/수신 용량으로 같은 바이트 열을 교환한다.
본문은 [HTML 강의](../../index.html#lesson-84)에서 읽는다.

```sh
cmake -S docs/learn/checkpoints/84-tcp-stream -B out/study84 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study84 -j3
ctest --test-dir out/study84 --output-on-failure
./out/study84/stream_probe listen 0 2
```

다른 터미널에서 ./out/study84/stream_probe connect 실제포트 two 를 실행한다.
서버 READ_CAP1..16, 클라이언트 MODE one/two/bytes를 바꾸어 비교한다.
마지막 TOTAL count=6 hex=41 42 43 44 45 46, VERIFIED 6을 확인한다.
Windows 다중 구성 빌드는 --config Release와 Release/stream_probe.exe를 사용한다.

한 연결당 요청 최대64바이트이며, 송신 방향 종료로 한 묶음의 끝을 알린다.
블로킹 loopback 진단으로 인증/제한시간이 없다. 게임 입력에는 아직 연결하지 않는다.
scripts/check_learning_tcp_stream.py는 조각 분할32종과 실제 두 프로세스·오류 경로를 검사한다.
