# 조회와 인덱스의 계약

- recent는 A/B 양쪽에서 참여한 경기의 저장 행 ID 내림차순, 전체limit1..50개다.
- 요청키의 크기·TEXT순서·실제종료시각과 저장id순서는 다르다.
- CHECK(player_a<>player_b)가 두 SELECT집합의 비중복을 보장해 UNION ALL을 쓴다.
- 각(player,id DESC,match_key) 인덱스는 조건/정렬/반환열을 함께 지원한다.
- 인덱스가 없어도 결과는 같다. 기본키·UNIQUE·FK·CHECK는 별도 데이터 계약이다.
- SqliteResults생성자가 동일 스키마의 기존DB에 IF NOT EXISTS로 인덱스를 추가한다.
- IF NOT EXISTS는 같은 이름의 기존 인덱스 정의를 수정하지 않는다.
- query_observer는 고정된 신뢰할 수 있는 로컬 SELECT와 binder만 진단한다.
- 반환텍스트 변환은 관찰용이다. 실제 저장/읽기 타입 검사는 SqliteResults가 유지한다.
- fullscan/sort/vm 계수는 실행시간·디스크I/O가 아니다. 계획 문자열은 사용자 기능 계약이 아니다.
- 독립fixture예상값/OR/UNIONALL/인덱스전후/ANALYZE로 결과와 경로를 따로 검사한다.
- history_list는 기존 로컬 학습DB를 연다. API 인증·경기 판정·공개 조회를 구현하지 않는다.
-110 파일은 README/DESIGN/CMake/sqlite_results.h 외 바이트 보존한다.
