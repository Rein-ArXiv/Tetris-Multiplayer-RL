# 등록소 계약

- RoomCode는32기호×5자리. uint32_t의 하위25비트를 사용하고0도AAAAA로 허용한다.
- create는 closed/owner0/중복owner/세대소진/full을 먼저 검사한다. 실패 T 보존.
- 후보를 최대32번 요청한다. 충돌이면 재시도, source nullopt면 즉시 실패, 약한 폴백 없음.
- 후보 조회와 등록은 하나의 mutex 아래 수행한다. 슬롯은 중간 구멍을 허용하고 처음 빈 칸을 쓴다.
- Next는 noexcept·비블로킹·작업량 제한·재진입 금지. T 소멸도 mutex 아래 같은 제한을 지킨다.
- T는 noexcept 이동생성/소멸만 필요하다. 기본생성·복사·이동대입은 요구하지 않는다.
- 등록 성공 때만 전역 세대번호를 소비한다. UINT64_MAX 뒤0으로 소진 표시하고 재사용하지 않는다.
- find는 값인 Handle snapshot을 반환한다. take는 code/generation/owner 모두 일치해야 자원을 인계한다.
- 세대는 이 등록소 인스턴스 내부의 오래된 요청을 구별한다. 프로세스 재시작을 넘는 영속 토큰이 아니다.
- Handle은 서버 내부 식별자이며 공개된 자료형 자체가 인증·인가를 제공하지 않는다.
- close는 남은 항목만 정리한다. 모든 API 사용자의 drain 뒤 소멸한다.
- shared_ptr payload 실험은 응답용 사본을 등록 성공 뒤 잠시 사용하고 즉시 버린다.
- FirstAdmission은 A-Z/0-9 join을 허용한다. 등록소 parse는 더 좁은32자 알파벳을 검증한다.
- 실제 서버는 unordered_map·roomInfoVersion·양쪽 참가자를 관리한다. 학습용 take 계약과는 다르다.
