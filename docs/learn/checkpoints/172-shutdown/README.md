# 종료 요청과 사용자 수명 회수

누적 ThreadLink와 WorkerGroup을 루프백 TCP 연결에 붙이고, StopGate와 ShutdownDrain을
추가한다. READY 뒤 신호를 기다리며 종료는 신규 수락 중단 → 게이트 열기 → 콜백/캡처
소멸 확인 → peer EOF 관찰 순서다. 프로토콜 전송 완료를 약속하는 실습은 아니다.

```sh
cmake -S docs/learn/checkpoints/172-shutdown/roles -B out/study172 -DSTUDY_ROLE=SHUTDOWN -DCMAKE_BUILD_TYPE=Release
cmake --build out/study172 --config Release --target shutdown_demo
python3 scripts/check_learning_shutdown.py --binary out/study172/shutdown_demo
```

Visual Studio 생성기는 out/study172/Release/shutdown_demo.exe를 전달한다.
직접 종료 신호 없이 실행하려면 shutdown_demo --self-stop을 사용한다.
Windows 신호 검사는 콘솔 프로세스 그룹과 CTRL_BREAK_EVENT/SIGBREAK 경로를 사용한다.

- 신호 핸들러에는 lock-free를 확인한 atomic bool 기록만 둔다.
- 게이트·그룹은 가드보다 오래 산다. 대기는 일반 스레드 문맥에서만 수행한다.
- 그룹은 캡처 소멸까지 drain한다. detached 스레드 TLS 종료까지의 join은 아니다.
- 캡처 ThreadLink는 자신이 소유한 I/O 스레드를 join하므로 소켓은 해당 범위에서 닫힌다.
- 종료 가드는 일반 조기 return/예외에도 동작하나 강제 프로세스 종료에는 실행을 약속하지 않는다.
- 폴링 간격이나 테스트 기한은 모든 운영 부하의 종료 시간 보장이 아니다.

README·roles/CMakeLists 외의 누적 파일은 보존하고 net/stop_gate.h와 tools/shutdown_demo.cpp, tests/stop_gate_contract.cpp를 추가했다.
