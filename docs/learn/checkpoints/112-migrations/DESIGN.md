# 이관의 계약

- v1: 알려진 네 기본 테이블. v2: selected_icon_id NOT NULL DEFAULT default + 모든 플레이어의 default 소유.
- BEGIN IMMEDIATE 뒤 구조/버전을 판단하고 ALTER·소유보강·이력·헤더를 함께 커밋한다.
- known unversioned, v1, v2, SQL 덤프의 header0을 지원한다. 미래·외부·이력/구조 불일치는 거절한다.
- 테이블 정의 토큰 비교는 알려진 선언의 보수적 판별이며 SQL 의미 동등성 검사가 아니다. 추가 인덱스는 허용하며 모든 인덱스 정의를 검증하지 않는다.
- selected_icon_id에는 신규 FK가 없다. 조건부 UPDATE와 시작 검사, add_player의 플레이어+기본소유 트랜잭션으로 계약을 유지한다. 직접 SQL 쓰기도 이 조건을 지켜야 한다.
- v2 반복 실행은 사용자가 선택한 아이콘을 유지한다. application_id는 인증 수단이 아니다.
- quick_check, foreign_key_check, 선택 소유 검사는 서로 다른 조건이다.
- 이관 실패로 저장소 생성이 끝나면 연결을 닫는다. 소멸자의 ROLLBACK은 best effort이며 저장장치 실패 성공을 보장하지 않는다.
- MigrationHook과 pause 옵션은 로컬 실패 실험용이며 HTTP 요청에 노출하지 않는다.
- SQL 덤프는 일반 이력을 보존하나 헤더0일 수 있다. Backup API는 페이지/헤더를 복사한다.
- HTTP 형식·경기 행·최근 조회는 유지한다. 실제 계정 DB 스키마와는 호환되지 않는다.
