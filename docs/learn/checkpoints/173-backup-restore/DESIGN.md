# 조회 결과와 표시 수명

- 122 누적기능유지.랭킹패널은실제tetris메뉴에통합,프레임에서HTTP대기하지않음.
- careers에서RP DESC와canonical decimal TEXT ID의length ASC/BINARY ASC로정렬한다.
- uint64를SQLite signed64로cast하지않는다.양수/정규십진표현을읽을때도확인한다.
- 상위3명고정,공개ID/RP만반환.전체조회DONE까지성공한경우만목록공개.
- 한SELECT스냅샷.여러조회사이에성적갱신가능.LIMIT은출력상한이며작업량상한이아님.
- 전용ranking인덱스는없다.큰데이터성능/조회계획/인덱스설계는별도측정필요.
- GET /study/v1/ranking은본문/params거절.200[]는성공빈목록,실패503.
- 1024바이트수신상한.배열/최대3행객체/정확필드/중복키/수치범위/ID중복/순서검사.
- 계정/랭킹은common Job<Worker>로future와Worker수명만공유하며화면정책은별도Controller.
- Worker보다future먼저파괴.한UI스레드소유,한job만허용,ready에서한번take.
- 랭킹View idle/busy/ready/failed.재조회시이전목록숨김,실패는empty-success와구별.
- 서버목적지실행중고정.패널닫기는서비스취소가아니며숨겨져도결과수집.
- uint64ID는두ASCII단락조각으로전체숫자보존.실제앞조각폭을더해같은baseline에그림.
- 메뉴/랭킹글자는공유atlasrevision.결과/밀도변경시양쪽Labels를reset후다시준비.
- 계정서비스와경기서비스는같은DB파일에연결가능.C++mutex는각프로세스연결만보호.
- 신뢰하는loopback경기실습API.조회나사용자요약을보상권위로쓰지않음.
