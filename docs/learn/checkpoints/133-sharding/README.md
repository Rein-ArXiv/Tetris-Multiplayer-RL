# 133 — 경기 소유권을 넘기는 샤딩

132-backpressure 파일을 보존하고 transfer_inbox.h·sharded_relay.h 및 계약/프로브를 추가한다.
RelayMatch는 입장을 마친 두 소켓·목적지별 PendingSend·절대 만기를 소유한다.
MatchLoop는 한 경기의 두 등록을 소유하고 루프 경계에서만 attach/detach한다.
등록 ID는 각 Reactor에 속하며 경기 ID와 다르다. 오래된 배치는 원래 루프에서만 처리한다.
프로브는 직접 만든 네 경기의 소유권을 두 실제 작업 스레드에 라운드로빈으로 넘긴다.
인증/매칭/프레임 해석은 누적 별도 프로브에 남아 있고, 여기서는 입장 이후 바이트 전달을 다룬다.

```sh
cmake -S docs/learn/checkpoints/133-sharding -B out/study-133 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-133 --target sharding_contract sharding_probe -j2
out/study-133/sharding_probe
ctest --test-dir out/study-133 -R sharding --output-on-failure
```

기본 poll/WSAPoll, Linux에서는 --epoll. Windows 다중 구성은 --config Release/-C Release와 Release/ 경로.
이 실습은 완료 요청을 제출하지 않는 준비성 백엔드만 사용한다. 운영 서버 Windows IOCP의 포트 결합과 구분한다.
누적 SDL tetris 타깃 유지. 독립 복사는 STUDY_VENDOR_DIR 지정.

TransferInbox의 정해진 슬롯은 성공 때만 unique_ptr을 소비한다. 닫힘/가득 참은 호출자 소유권을 유지한다.
mutex가 공개/회수와 마지막 close를 직렬화한다. wake는 공개 뒤 요청하며 주기적 회수로 보완한다.
실습 우편함4개·루프8경기·송신 큐4096바이트(high1024/low256)·공유 예약65536바이트.
경기 객체를 이동하지 않고 포인터만 옮기므로 큐의 공유 카운터 참조와 객체 주소를 보존한다.
공유 예산은 우편함 대기 중에도 반환하지 않는다. 절대 만기도 유지하여 인계로 정체 유예를 연장하지 않는다.

attach의 부분 등록 실패는 첫 등록을 철회하고 호출자에게 경기를 남긴다.
등록 해제가 실패하여 소유 관계를 확정할 수 없으면 backend를 먼저 파괴하고 그 루프의 경기를 정리한다.
이 루프는 이후 재사용할 수 없다. 서버 전체 자동 복구 정책을 구현한 것은 아니다.
실제 서버는 앞단의 등록 해제 실패 시 그 경기를 앞단에서 중단한다.

프로브는 앞단의 오래된 배치를 무시하고, 이미 보류한1024바이트 뒤에 등록 공백 중 보낸64바이트를
순서대로 전달한다. 네 경기 각각의 반대 방향1바이트도 확인한다. 수락2개/샤드와 join 후 예산0을 확인한다.
프레임 검증·실행 중 경기 재분배·절대 처리시간 보장은 범위 밖이다.

검사: python3 scripts/check_learning_sharding.py
현재 서버: python3 scripts/check_learning_sharding_root.py
서버 검사는 실제 RelayLoop를 계측하여 Reactor만 통제한다. 라이브 프로세스 회귀는
python/tests/test_relay_meta_smoke.py의 test_reactor_shards_preserve_pair_streams_and_release_counts다.
STUDY_MINGW_CXX가 지정되면 Windows 교차 링크도 검사하며 실제 실행과 구분한다.
