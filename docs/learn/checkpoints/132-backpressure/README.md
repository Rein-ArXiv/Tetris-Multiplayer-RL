# 132 — 제한 송신 큐와 읽기 중지·재개

131-state-machine 누적 파일을 보존하고 byte_budget.h·pending_send.h·buffered_relay.h와 계약/프로브를 추가한다.
BufferedRelay는 입장/수락을 마친 두 전송 소켓에 붙이는 양방향 바이트 전달 구성 요소다.
실습 프로브는 짝을 직접 만들고 사용한다. 인증/로비 프로브의 실행 파일을 새 서버로 바꾸지는 않는다.
프레임 내용·순서·입장 판정을 바꾸지 않는다. 반쪽 종료 대신 한쪽 실패 시 페어 전체를 닫는 정책이다.

```sh
cmake -S docs/learn/checkpoints/132-backpressure -B out/study-132 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-132 --target backpressure_contract backpressure_probe -j2
out/study-132/backpressure_probe
out/study-132/backpressure_probe --stall
ctest --test-dir out/study-132 -R "backpressure" --output-on-failure
```

기본 poll/WSAPoll, Linux에서는 --epoll. Windows 다중 구성은 --config Release/-C Release와 Release/ 경로.
누적 SDL tetris 타깃 유지. 독립 복사는 STUDY_VENDOR_DIR 지정.

목적지 큐 하나4096바이트, high1024/low256, 두 큐 공유 대기 예산8192.
Read는 상대 큐의 상태, Write는 자기 큐가 비었는지로 계산한다. 수신/송신 각각 콜백당 한 번.
한 방향의 Read를 내린 동안에도 역방향 Write를 유지한다. 오래된 배치에서는 현재 pause를 재확인한다.
Read가 내려간 상태의 HUP/ERR는 종료하여 준비성 스핀을 막는다.
송신이 진행되면 해당 목적지 큐의 정체 만기를 갱신하고, 빈 큐는 취소한다.
정상 프로브는 지속 pause를 관찰한 뒤 반대 방향1바이트를 전달하고, 수신을 열어 전체 FIFO를 확인한다.
--stall은 수신을 열지 않고 입력을 계속 제시하여 큐가 실제로 남은 상태의 만기·예산 반환을 확인한다.

검사: python3 scripts/check_learning_backpressure.py
실제 메서드 고장 대역: python3 scripts/check_learning_backpressure_root.py
STUDY_MINGW_CXX가 지정되면 Windows 교차 링크도 검사한다. 실제 실행 검증과 구분한다.
고정 배열은 큐가 비어도 공간을 차지한다. 바이트 예산은 논리적 대기량이며 RSS·커널 버퍼 상한이 아니다.
