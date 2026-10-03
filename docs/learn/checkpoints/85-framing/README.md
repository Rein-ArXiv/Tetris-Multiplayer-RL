# 85 길이 접두사와 부분 수신

84-tcp-stream의 누적 게임·소켓·스트림 진단을 유지한다.
framing_probe는 반복 메시지의 경계를 복원하고 EOF 전에 프레임마다 응답한다.
본문은 [HTML 강의](../../index.html#lesson-85)에서 읽는다.

```sh
cmake -S docs/learn/checkpoints/85-framing -B out/study85 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study85 -j3
ctest --test-dir out/study85 --output-on-failure
./out/study85/framing_probe listen 0 2
# 별도 터미널: LISTEN 뒤 실제 포트 사용
./out/study85/framing_probe connect 실제포트 1
```

READ_CAP/CHUNK 범위1~16. 마지막 FRAMES 3 / VERIFIED 3 FRAMES를 확인한다.
Windows 다중 구성에서는 --config Release, CTest -C Release,
Release/framing_probe.exe를 사용한다.

기준 wire는 LEN:u16LE + TYPE:u8 + PAYLOAD, 최대payload32, 체크섬 없음.
현재 게임 wire와 직접 호환되지 않는다. loopback 한 연결의 블로킹 진단이며
게임 통합·인증·시간 제한은 포함하지 않는다.
`scripts/check_learning_framing.py`는 기준 분할·실제 통신·실제 Session 종료 경로와
C++/Python 파서를 직접 비교한다.
