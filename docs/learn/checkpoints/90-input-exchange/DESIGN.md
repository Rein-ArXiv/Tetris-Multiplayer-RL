# 90 입력 기록과 규칙 소비

- TickInputs: host/peer 각각 optional<uint8_t> 32칸. nullopt와 중립0 구분.
- next_: 다음 소비 순번 u64. wire u32 최댓값 소비 후 +1로 소진 표현, 순환 없음.
- 창 범위 검사 후 put. 최초 기록 보존, 동일 pending은 duplicate, 충돌은 거절.
- put_batch는 후보 전체 검사 후 적용. 일부 유효 항목도 실패 시 원본에 반영 안 함.
- peek는 두 입력만 읽고 consume은 한 칸 이동. O(32) shift는 명시적인 학습 선택.
- Lockstep: host→left,peer→right 정렬, 모든 호출 단일 소유자 직렬. no I/O.
- Duel 후보가 성공하면 상태와 한 쌍을 확정. waiting/finished/invalid/exhausted는 소비 안 함.
- input_probe: HELLO/OFFER/ACK 후 TYPE20 payload 코덱 재사용. 총12틱 범위 검사.
  미래 묶음을 먼저 보내서 수신 최대 번호와 연속 입력 존재의 차이를 드러낸다.
- bounded two-frame send then receive is only a small diagnostic. Continuous games must
  interleave send/read. Per-send budget does not bound blocking receive or entire exchange.
- Same seed/config/rules plus canonical ordered input records are prerequisites for replay.
  End hashes are diagnostic outputs, not peer authentication or authoritative result validation.
- Current game uses maps/local-remote perspective/inputDelay. Its exact-pair helper shares
  the presence contract; it does not inherit this checkpoint's whole-batch/window policies.
