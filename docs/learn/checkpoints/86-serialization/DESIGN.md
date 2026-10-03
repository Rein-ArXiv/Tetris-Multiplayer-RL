# 86 구현 경계

바이트 필드/메시지 문법/세션 정책을 구분한다.
ByteReader와 ByteWriter는 저장소를 차용하며 실제 extent와 수명은 호출자가 보장한다.
position<=extent, remaining=extent-position, 전체 필드 폭 확인 후 접근한다.
필드 실패는 커서·출력/저장소를 보존한다. 실패 고정 상태는 두지 않으므로 호출자는
첫 false에서 중단한다. 성공한 앞 필드까지 되돌리지 않는다.

InputBatch 코덱은 후보 객체로 메시지 전체 성공 시에만 출력을 바꾼다.
count1..16 확인 후 u16 축소, 읽은 count와 남은 payload의 정확한 일치,
모든 알려진 입력 비트와 count-1<=UINT32_MAX-first_tick을 적용한다.
이 조건과 현재 경기의 틱 거리·입력 빈도·큐 상한은 별개다.
기준 InputBatch는 소유 복사, 현재 Session의 InputBatchView는 차용 뷰다.

serialization_probe는 기존 framing_probe echo 서버를 그대로 사용한다.
0x01020304는 바이트 순서 관찰용 틱이며 실제 경기에서 허용될 틱이라는 뜻이 아니다.
같은 payload의 TYPE2 응답을 검증한 뒤 송신 방향 종료·상대 EOF를 확인한다.
게임·렌더러·오디오·기존 프레이밍 파일은 그대로 유지한다.
정상 값을 round-trip하는 검사와 독립적으로 정한 wire 고정 벡터를 함께 둔다.
