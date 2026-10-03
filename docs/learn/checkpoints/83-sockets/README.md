# 83 소켓: 프로세스 사이의 바이트 통로

82-audio-failure의 누적 게임을 유지하고 독립 socket_probe를 추가한다.
게임 입력 통합 전 소켓의 소유권·주소·연결·대기와 오류를 관찰하는 단계다.
학습 본문은 [HTML 강의](../../index.html#lesson-83)에서 읽는다.

```sh
cmake -S docs/learn/checkpoints/83-sockets -B out/study83 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study83 -j3
ctest --test-dir out/study83 --output-on-failure
./out/study83/socket_probe listen 0
```

다른 터미널에서 `./out/study83/socket_probe connect 실제포트`를 실행한다.
서버 출력은 RECEIVED 42 ECHOED 42, 클라이언트 출력은 SENT 42 RECEIVED 42다.
Windows 다중 구성 빌드는 --config Release와 Release/socket_probe.exe 경로를 사용한다.
누적 창 게임은 STUDY_PLATFORM=SDL로 별도 구성하며 실행 대상은 tetris다.

블로킹 IPv4 loopback 전용이며 제한 시간·인증은 없다. 포트0은 서버의 자동 할당 요청이다.
클라이언트는 출력된 실제 포트를 사용한다. 실행 오류1, 명령 오류2, 성공0을 반환한다.
검사 스크립트는 scripts/check_learning_sockets.py이며 프로세스 검사에는 시간 제한을 둔다.
