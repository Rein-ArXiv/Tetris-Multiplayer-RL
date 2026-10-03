# HTTP 실패의 계약

- 한 MatchSubmission은 한 immutable key/record와 시도 횟수, 확인 상태를 소유한다.
- HttpSender는 한 번만 보낸다. 200은 영수증 검사, 응답 없음/429/5xx는 제한 재시도 후보다.
- 잘못된 응답/기타 HTTP/로컬 오류는 stopped. 자동 중단은 DB 미저장의 증거가 아니다.
- stopped/confirmed 이후는 무호출. pending request는 예산 소진과 중단에도 남는다.
- driver는 100/200ms backoff와 서버 최소 대기 중 큰 값을 기다린다.
- Retry-After는 정수 초만 지원. 중복/잘못된 값/미지원 날짜는 자동중단한다.
- 최소 대기가 예산 이상이면 기다리거나 새 전송을 하지 않는다.
- 대기 뒤 시계를 다시 확인한다. 새 시도에 남은 예산과 2초 중 작은 단계별 제한을 전달한다.
- 시도 진입 예산이며 전체 호출의 강제 취소 마감이 아니다. 늦은 유효 영수증은 수락한다.
- 실제 시계/대기는 콘솔 도구에 연결한다. UI/main tick에서 blocking loop를 돌리지 않는다.
- 재시작 복구·jitter·날짜형 Retry-After·공개 TLS 운영은 이 체크포인트의 구현 범위 밖이다.
- 실패 본문과 입력 URL을 그대로 진단 로그에 출력하지 않는다.
