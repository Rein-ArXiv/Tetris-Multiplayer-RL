# 106 — 동시 퇴장과 참가 수명

105 누적 구현에 net/room_membership.h와 두 실행 타깃을 추가한다.

```sh
cmake -S docs/learn/checkpoints/106-room-exit -B out/study-106 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-106 --target room_membership_contract room_membership_probe -j2
ctest --test-dir out/study-106 -R '^room_membership_(contract|probe)$' --output-on-failure
```

SDL은 별도 빌드 폴더에 -DSTUDY_PLATFORM=SDL을 지정한다. 두 타깃은 창을 열지 않는다.
contract는128번의 중복 퇴장과128번의 양쪽 퇴장, 같은 Peer 재입장, 오래된 알림,
송신 실패, gate 순서, 잠금 밖 소멸, 번호 소진을 검사한다.
probe는 실제 TCP에서 게스트 퇴장→EOF→동일 코드 재입장→새 인원 통지를 수행한다.
그 뒤 소켓·parser를 수락 로비와 포워더로 넘겨 반대 클라이언트 입력 수신과
RoundPlay의 정규 상태 바이트 일치까지 연결한다.

RoomMembership은 두 슬롯의 참가 상태를 관리한다. epoch/side/member로 참가를,
revision으로 알림 상태를 구별한다. leave는 정리할 shared_ptr를 반환하며,
close는 등록 종료와 참조 인계만 수행한다. 실행 중인 사용자의 완료 대기는 호출자 책임이다.
Peer 내부 동시 I/O·close는 별도 동기화가 필요하며 probe의 소켓 조작은 단일 실행 주체다.

학습 프레임은 서비스 체크섬/ROOM_INFO 형식과 호환되지 않는다.
HDAAA·round1·seed77·역할은 fixture이며 인증·READY 상태·보상 판정은 멤버십 범위 밖이다.
EOF와 frame 읽기에2초 협조적 마감을 사용하고, 작은 loopback 송신 실패는 실습 실패로 처리한다.
EOF와 인원 통지는 방 참가의 관측이며 게임 승패의 판정이 아니다.
