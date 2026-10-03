# 참가 이력과 정리 순서 계약

- epoch는 방 등록 수명, side는 자리, member는 한 번의 참가, revision은 알림 상태를 식별한다.
- 모든 슬롯 검사는 state mutex 안에서 수행하고 검사와 제거를 분리하지 않는다.
- 같은 ticket의 반복 퇴장은 한 번만 제거한다. 서로 다른 두 퇴장은 마지막 하나만 empty를 얻는다.
- leave/close는 소유 참조를 반환해 자원 해제가 state 잠금 밖에서 가능하게 한다.
- 다른 shared_ptr나 Notice가 남아 있으면 Peer는 더 오래 살 수 있다.
- join/deliver가 중첩 잠금을 잡을 때 gate→state 순서를 지킨다.
- deliver는 gate를 잡은 채 현재 버전·수신자·endpoint·인원을 검사하고 state를 풀어 callback을 호출한다.
- callback은 noexcept·유한 시간·비재진입 계약을 따르고 반환 bool을 그대로 전달한다.
- leave/close는 callback 중에도 진행할 수 있다. 알림 순서를 보장하며 수신 순간의 최신성은 보장하지 않는다.
- shared_ptr 수명 유지가 Peer 내부의 I/O/close를 직렬화하지는 않는다.
- close는 참가 등록을 끝내지만 진행 중인 작업의 완료를 기다리지 않는다. 객체 파괴 전에 모든 사용자를 drain한다.
- 번호는 wrap하지 않는다. revision 소진은 상태 변경 거절 뒤 close로 정리한다.
- 멤버십 ticket은 내부 식별자이며 클라이언트 인증 수단이 아니다.
- probe의 논리적 멤버십 close는 두 소켓을 다음 단계로 인계한다. Socket::reset과 구별한다.

105 누적 파일은 README/DESIGN/CMake 외 수정하지 않는다.
