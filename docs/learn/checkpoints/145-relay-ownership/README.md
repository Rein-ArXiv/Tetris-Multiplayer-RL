# 같은 판정과 두 실행 소유자

RelayChannel은 PacedMatch와 ResultHandoff를 소유한다. ThreadRelayChannel은 잠금 안에서 입력·선점·스냅샷을 직렬화하고 저장 호출은 잠금 밖에서 실행한다. LoopRelayChannels는 하나의 루프가 상태를 소유하고 Offload의 작업자가 저장한 결과를 소유 루프에서 적용한다.

```sh
cmake -S docs/learn/checkpoints/145-relay-ownership -B out/study-145 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-145 --target relay_owner_contract relay_owner_probe -j2
out/study-145/relay_owner_contract
out/study-145/relay_owner_probe out/study-145/results.db
out/study-145/relay_owner_probe out/study-145/results.db
```

Boost가 기본 경로 밖에 있으면 STUDY_BOOST_INCLUDE를 지정한다. probe는 실제 SQLite에 같은 경기 키로 저장을 반복한다. 재실행 때 저장된 지급 내역이 중복 증가하지 않아야 한다. 데모 계정 초기화는 인증 대역이며 실제 자격 확인을 대체하지 않는다.

- result_handoff.h: 한 번의 로컬 선점과 응답 확인. 분산 저장의 중복 방지는 DB의 별도 계약.
- relay_channel.h: 판정과 저장 상태의 같은 소유 경계, 선점 뒤 입력 종료.
- thread_relay_channel.h: 잠금 후 시각 관찰, 저장 호출 밖으로 값 복사.
- loop_relay_channels.h: 로컬 ID 재사용 금지, 값 캡처, 늦은 완료 시 조회, 종료 시 join→drain.
- relay_owner_contract.cpp: 실제 스레드 경쟁·느린 저장·완료 소유권·용량/예외·삭제된 채널·종료.
- relay_owner_probe.cpp: 서버 규칙 실행→두 소유 모델→실제 SQLite 결과·보상.

이 어댑터는 연결 등록/송신 코드를 소유하지 않는다. 연결의 파서는 인증된 주체로 receive를 호출하고, 소켓 송신 큐는 연결 소유자가 관리한다. 로컬 ID는 프로세스 안의 조회 키이며 재시작까지 유지할 경기 저장 키와 다르다. LoopRelayChannels는 생성한 스레드에서 파괴하고, ThreadRelayChannel은 모든 호출 스레드를 마친 뒤 파괴한다. 시계 콜백은 짧게 실행하고 채널에 재진입하지 않는다.

```sh
python3 scripts/check_learning_relay_ownership.py --boost /path/to/boost/include
```
