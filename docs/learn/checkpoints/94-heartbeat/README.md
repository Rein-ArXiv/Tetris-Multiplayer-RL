# 94 하트비트

93-thread-queues에 순수 타이머와 선택형 워커 PING/PONG을 추가한다.
[HTML 강의](../../index.html#lesson-94)에서 요청 대응과 장애 감지 범위를 읽는다.

```sh
cmake -S docs/learn/checkpoints/94-heartbeat -B out/study94 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study94 -j3
ctest --test-dir out/study94 --output-on-failure
./out/study94/heartbeat_probe listen 0
# 다른 터미널에서 LISTEN 포트 사용
./out/study94/heartbeat_probe connect 포트번호
```

TYPE30/31은 8바이트 LE u64 비영 토큰. 기본 interval/suspect/timeout=1000/2000/3000ms.
실험은40/150/350ms로 왕복을 관찰한 뒤 약600ms에 TYPE32 종료 표시를 보내고 half-close한다.
양쪽 END 0, confirmations>=3, done=1이 예상 결과다. 게임 승패·상태 인증이 아니다.
main을 멈춰도 I/O 워커 제어 응답은 진행할 수 있다. 일반 프레임 큐는 계속 유한하다.
수신 PING의 응답도1칸 제한이며 넘치면실패. 시각 역행·중복·늦은 응답은 갱신하지 않는다.
Windows 다중 구성은 --config Release / CTest -C Release / Release/의 실행 파일을 쓴다.
시작 accept/connect는 동기식이다. 검수는 `python3 scripts/check_learning_heartbeat.py`.
