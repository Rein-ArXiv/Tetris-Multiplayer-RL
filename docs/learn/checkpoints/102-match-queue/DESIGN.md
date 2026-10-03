# 매칭 큐의 계약

- PairQueue<T, Capacity>는 [0,count) 슬롯만 채운다. T는 예외 없는 이동·소멸을 제공한다.
- 등록 거절의 우선순위는 closed → id0 → pending duplicate → full. 실패하면 입력 T 보존.
- ID는 연결 요청을 구별한다. 오래된 요청이 참조하는 ID를 호출자가 재사용하지 않는다.
- 등록·취소·생존 검사·선택·종료는 하나의 mutex로 직렬화한다. FIFO는 성공한 삽입 순서다.
- cancel은 제거한 Entry의 소유권을 반환한다. 매칭이 먼저 꺼냈다면 nullopt다.
- 모든 pending 항목을 Poll한 뒤 생존자 앞의 두 명을 이동한다. Poll=false 항목은 정리한다.
- Poll은 noexcept, 작업량 제한, 블로킹 I/O/대기와 큐 재진입 금지. T 소멸에도 같은 잠금 제약.
- try_pair는 상대를 기다리지 않지만 mutex 획득에서 기다릴 수 있다.
- 빈 큐는 predicate wait, 한 항목은 50ms wait_for. socket 입력은 CV 알림을 발생시키지 않는다.
- 주기적 재검사는 실행 흐름·잠금 재획득이 진행되어야 한다. 50ms 응답 상한 계약은 없다.
- 배열 중간 삭제는 생존자를 당겨 순서를 보존한다. 한 삭제 O(N), 다수 삭제 최악 O(N²).
- close는 남은 항목 파괴와 알림만 담당한다. 모든 사용자 drain/join 뒤 큐를 파괴한다.
- 꺼낸 Pair의 소켓·parser는 큐와 독립적으로 살아 있다. 전체 서버 종료는 다음 소유자도 정리한다.
- 소켓 실험은 TYPE51 취소·파싱/수신 오류를 제거 사유로 쓴다. 다른 완성 메시지는 소비한다.
- 현재 Matchmaker는 deque·PlayerInfo·세션 lease·서버 seed/UUID를 사용하며 거절된 연결을 닫는다.
  학습용 큐의 실패 입력 보존 API와 동일하지 않다. 실제 대기 상한은 1024다.
