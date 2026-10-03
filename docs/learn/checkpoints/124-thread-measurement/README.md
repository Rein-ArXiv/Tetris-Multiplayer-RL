# 124 — 스레드 통신 경로의 제한된 측정

123-ranking의 게임·계정·랭킹을 유지하고 ThreadLink에 마지막 선택 인자 poll_delay만 추가한다.
기본1ms, 허용1~10ms이며 작업자 시작 전에 검증한다.

```sh
cmake -S docs/learn/checkpoints/124-thread-measurement -B out/study-124 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-124 --target thread_measure measurement_contract -j2
ctest --test-dir out/study-124 -R '^measurement_contract$' --output-on-failure
out/study-124/thread_measure 4 100 1 > out/study-124/measurement.json
```

한 프로세스: N개 TCP loopback 연결, 양 끝 각 ThreadLink → 2N 작업자와 main.
main이 요청 묶음 전송·에코·완료 확인. 한 연결당 미완료 하나, 모든 응답 후 다음 묶음.
메시지 type60/round+slot 8바이트, 전체 프레임11바이트. 운영 프로토콜이 아니다.
200ms 유휴 관찰 → 8묶음 워밍업 → 1~1000묶음 측정 → 통계·출력·정리.
묶음500ms/measure60초 마감. 모든 작업자를 join한 뒤 Runtime을 정리한다.
실패 시 부분 성공만 표본으로 남기고 incomplete/unattempted/status를 함께 출력한다.
CPU는 발생기·응답기·소켓 작업자 전체. Windows GetProcessTimes, POSIX getrusage,
Linux만 자발적/비자발적 전환 추가. 제공 불가 관찰은 null.

`thread_measure 4 10 1 drop`은 timeout, `... corrupt`는 bad_reply와 종료코드1이 정상이다.
구문/범위 오류2, 소켓 준비·워밍업 실패3. 테스트: `python3 scripts/check_learning_measurement.py`.
각 조건3회 결과는 out/learning-checkpoints/124-thread-measurement-check/observations.json.
시간값을 고정 정답으로 비교하지 않는다. closed-loop와 묶음 장벽의 한계를 함께 해석한다.
실제 서버 단독 CPU/최대 접속자 수/원격 RTT/메모리 용량 검사가 아니다.

C++17/CMake와 누적 의존성 필요. 독립 복사 시 STUDY_VENDOR_DIR 지정.
SDL 게임은 별도 폴더에 STUDY_PLATFORM=SDL, tetris 타깃을 빌드한다.
계정 실행 옵션은 `--account PORT FOLDER`, 기본 loopback18080/study-account.
다중 구성은 --config Release / -C Release / Release 실행 하위 경로를 맞춘다.
