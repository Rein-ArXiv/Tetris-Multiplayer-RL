# 131 — 연결의 진행 상태와 재개

130-offload의 누적 파일을 유지하고 AdmissionSession, 단조 ID 발급기, 상태 계약/실제 루프 프로브를 추가한다.
입장 TYPE50(queue) → 워커의 통제된 인증 대역 → READY17 → 두 참가자 Forward → TYPE60 관찰 흐름이다.
인증 대역은 route만 확인한다. 실제 계정/티켓 검증과 게임 데이터 중계는 누적 릴레이와 현재 소스를 참고한다.

```sh
cmake -S docs/learn/checkpoints/131-state-machine -B out/study-131 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-131 --target state_machine_contract state_machine_probe -j2
out/study-131/state_machine_probe
ctest --test-dir out/study-131 -R "^(state_machine_contract|state_machine_probe|epoll_state_machine_probe)$" --output-on-failure
```

Windows 다중 구성은 --config Release/-C Release 및 Release/state_machine_probe.exe 경로를 사용한다.
기본 poll/WSAPoll, Linux --epoll. 별도 게임은 SDL 구성 tetris 타깃으로 빌드한다.
독립 복사 시 STUDY_VENDOR_DIR을 지정한다.

Session이 한 파서를 계속 소유한다. Auth와 READY 후 짝 대기 중에는 읽기 관심을 내리고 잔여를 보존한다.
인증/짝 전이 뒤 pump를 직접 불러 이미 받은 바이트를 처리한다. 16바이트 수신과 64바이트 파서 상한을 사용한다.
한 연결에 인증은 한 번. ID와 기대 단계로 결과를 확인한다. 재시도 확장 시 요청 세대도 필요하다.
만기는 루프가 검사하며 완료 → I/O → 만기 순서다. 작업 완료가 같은 반복의 만기보다 먼저 적용될 수 있다.
Forward의 프로브 프레임을 세고 각 접속을 종료한다. 상대 전달·재매칭·룸 구현 전체를 대체하는 서버가 아니다.
정상 종료에서는 join 후 최종 결과를 회수한다. 실패로 스코프를 떠나도 pool 소멸이 참조 대상보다 먼저 join한다.

정확성 검사: python3 scripts/check_learning_state_machine.py
실제 Room/Conn의 중첩 종료: python3 scripts/check_learning_room_lifetime.py
실제 인증 재개: python3 scripts/check_learning_auth_state.py
STUDY_MINGW_CXX 지정 시 Windows 교차 링크도 검사한다. 실행 환경 검증과 구분한다.
