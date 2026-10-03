# 93 스레드와 큐

92-hash-audit에 연결 이후 워커와 양방향 값 큐를 추가한다.
[HTML 강의](../../index.html#lesson-93)에서 동기화·소유권·종료 순서를 읽는다.

```sh
cmake -S docs/learn/checkpoints/93-thread-queues -B out/study93 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study93 -j3
ctest --test-dir out/study93 --output-on-failure
./out/study93/thread_probe listen 0
# 다른 터미널에서 LISTEN 포트 사용
./out/study93/thread_probe connect 포트번호
```

양쪽은 알려진 네 StateStamp를 교환하고 DONE frames=4 end=0을 출력한다.
이 값은 통신 인계 실험이며 게임 상태 해시가 아니다. 기존 hash_probe는 유지한다.
8칸 값 큐는 full을 조용한 성공으로 바꾸지 않는다. 수신 overflow는 연결 실패다.
finish_sending은 큐 소진 후 반쪽 종료, request_stop은 미전송을 포기할 수 있는 취소다.
Runtime은 ThreadLink보다 먼저 생성하고 나중에 파괴한다. main은 이동한 소켓을 만지지 않는다.
시작 accept/connect는 동기식이며 워커 취소 범위 밖이다. sleep은 지연 상한 보장이 아니다.
Windows 다중 구성은 --config Release, CTest -C Release, Release/thread_probe.exe를 사용한다.
검수: `python3 scripts/check_learning_threads.py`.
