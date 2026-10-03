# 138 — 비동기 TLS: 취소와 객체 수명

누적 게임과 동기 WSS 진단기를 유지하고 비동기 일회 왕복 도구를 추가한다.
단계는 resolve→connect→TLS→Upgrade→write→read다. 접속 단계의 취소, 전체 시도의
마감, 늦은 완료, 버퍼 보존과 객체 해제를 독립적인 관찰값으로 검사한다.

- `net/async_wss_attempt.h`: 한 실행 스레드에서 구동하는 상태와 소유권 계약.
- `tools/async_wss_probe.cpp`: 외부 소유권을 놓고 같은 문맥의 별도 작업도 관찰하는 진단기.
- `tests/async_context_contract.cpp`: stop/restart, 미실행 캡처 파괴, 취소 완료의 수명 검사.

```sh
cmake -S docs/learn/checkpoints/138-async-tls -B out/study-138 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-138 --target async_wss_probe async_context_contract -j2
ctest --test-dir out/study-138 -R '^async_context_contract$' --output-on-failure
python3 scripts/check_learning_async_tls.py --boost /path/to/boost/include
```

C++17·Boost.Beast 헤더·OpenSSL 개발 라이브러리·스레드 라이브러리가 필요하다.
별도 Boost 경로는 CMake에 `-DSTUDY_BOOST_INCLUDE=/path/to/boost/include`로 전달한다.
검사 도구는 현재 게이트웨이와 네이티브 클라이언트를 빌드하고 임시 CA·인증서를 사용한다.
외부 서비스, 실제 사용자 계정·키가 필요 없다. 임시 프로세스와 인증서는 검사 후 회수한다.
`--sanitize`를 더하면 실제 비동기 왕복 행렬도 ASan/UBSan 빌드로 검사한다.
`--snippets-only`는 발췌·현재 심볼·누적 파일 보존만 확인한다.

단독 도구의 인수는 다음과 같다.

```text
async_wss_probe HOST PORT CA.pem MODE DEADLINE_MS
```

MODE는 `none`, `immediate`, `resolve`, `connect`, `tls`, `upgrade`, `write`, `read`다.
대상은 `/play`에서 실험용 바이트를 그대로 돌려주는 WSS 경로다. 메타 HTTPS API나
실제 경기 프로토콜에 보내는 진단기가 아니다. 목적지·마감은 실험자가 명시한다.

출력의 `end`와 `stage`는 헤더의 enum 선언으로 해석한다. `reports`는 최종 결과 기록,
`late`는 논리 종료 뒤 완료, `destroyed`와 `expired`는 객체 해제, `unrelated`는
같은 문맥의 다른 작업 진행이다. 성공·취소·마감·오류는 서로 다른 결과로 남긴다.
콜백 수는 실행 흐름의 관찰값이며 서비스 정책의 고정 수치가 아니다.

Attempt의 모든 메서드는 한 실행 스레드에서 호출한다. io_context와 TLS context는
모든 작업보다 오래 살아야 한다. 핸들러의 shared_ptr는 수명을 지키며 멤버 접근을
자동 동기화하지 않는다. 최종 결과를 정한 뒤에도 pending I/O의 버퍼를 보존한다.
finish는 공유 io_context를 멈추지 않고 취소 완료를 처리하게 한다.
마감은 처리 가능한 시점에 논리 결과를 정하며 OS 취소 완료까지의 엄격한 시간 상한이 아니다.

이 도구의 종료는 소켓을 끊는 경로다. WebSocket Close와 TLS close_notify를 교환하는
정상 종료 상태 기계는 포함하지 않는다. 현재 게임 클라이언트는 별도의 전용 worker를
외부 소유자가 join하는 설계이며 두 수명 방식의 차이를 강의에서 비교한다.

누적 그래픽 게임은 SDL 구성의 tetris 타깃이다. Windows 다중 구성은 빌드에
`--config Release`, CTest에 `-C Release`, 실행 경로에 `Release/`를 사용한다.
라이브러리는 대상 OS·아키텍처에 맞춰 준비한다.
