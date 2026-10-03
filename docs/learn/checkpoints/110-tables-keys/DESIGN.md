# 테이블·타입·소유 계약

- players와 icons는 개체, player_icons는 다대다 소유 관계, matches는 완료 기록이다.
- uint64의 전체 wire 범위를 정규 십진 TEXT로 보존한다. 식별자/round/ticks는 양수다.
- 행 id는 SQLite signed INTEGER PRIMARY KEY의 양수값이다. 요청 키와 생성 주체가 다르다.
- NULL winner는 무승부이며 HTTP 학습 wire는0/1/2를 유지한다. 타입 오류와 null은 다르다.
- 새 연결에서 foreign_keys=ON을 설정/조회한다. 소유는 사용자 삭제에 cascade, 경기 참조는 삭제를 막는다.
- NOT NULL, UNIQUE, CHECK, REFERENCES는 각각 부재·중복·값·참조를 검사한다.
- Statement는 연결을 빌리고 문장을 소유한다. SQLITE_TRANSIENT는 입력 바이트를 복사한다.
- SqliteResults는 연결과 mutex를 소유한다. 논리 작업 전체를 한 잠금으로 처리한다.
- 같은 키/같은 내용은 최초 행, 다른 내용은 conflict. 외부 연결 경합은 storage_error 후 재시도될 수 있다.
- 부재는 nullopt, SQL/변환 실패는 예외다. put은 저장 오류를 storage_error로 변환한다.
- schema.h는 새 실습 DB 생성용이다. IF NOT EXISTS는 마이그레이션이나 기존 스키마 검증이 아니다.
- seed_demo의 고정 선수/아이콘은 트랜잭션으로 준비한다. 보상과 계정 인증은 포함하지 않는다.
- 기본 SQLite 저널과 synchronous=FULL을 사용한다. 재시작 검사로 전원 장애 복구를 주장하지 않는다.
-109 누적 파일은 README/DESIGN/CMake 외 바이트 보존한다.
