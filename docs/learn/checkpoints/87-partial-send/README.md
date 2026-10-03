# 87 부분 송신과 종료 정책

86-serialization의 누적 게임·코덱·프레이밍 진단에 논블로킹 송신 정책을 추가한다.
본문은 [HTML 강의](../../index.html#lesson-87)에서 읽는다.

```sh
cmake -S docs/learn/checkpoints/87-partial-send -B out/study87 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study87 -j3
ctest --test-dir out/study87 --output-on-failure
./out/study87/framing_probe listen 0 2
# 다른 터미널에서 LISTEN 포트 사용
./out/study87/serialization_probe 실제포트
```

정상 출력은 PAYLOAD 04 03 02 01 03 00 01 00 10과
VERIFIED first_tick=16909060 count=3 masks=01,00,10이다.
Windows 다중 구성 빌드는 --config Release, 검사 -C Release,
Release/ 아래 exe를 사용한다.

serialization_probe는 모드 설정 성공을 확인하고 2초 마감시간으로 송신한다.
실패하면 SEND 줄에 종료 사유·이미 수락된 길이·OS 오류를 남기고 소켓을 정리한다.
송신 성공 뒤 블로킹 수신 모드로 복귀한다. 모드 변경은 두 방향에 영향을 주므로
이 진단처럼 소켓을 독점 소유해야 한다. connect와 응답 수신에는 시간 제한이 없다.
TYPE1 요청/TYPE2 응답은 현재 게임 wire와 호환되지 않는 loopback 진단이다.

send_contract는 가상 시간·정해진 진행/실패로 정책을 검사한다.
`scripts/check_learning_partial_send.py`는 실제 역압과 수락된 접두사,
현재 엔진의 전체 송신 마감시간까지 검사한다. 게임·렌더러·오디오는 유지한다.
