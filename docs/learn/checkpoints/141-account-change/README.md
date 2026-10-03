# 원자적 자격 교체 체크포인트

누적 계정 서비스에 backup·rotate·recover를 추가한다. player ID·재화·보유 아이콘은 유지하고 account_keys의 접근 해시·복구 해시·인증 세대·최신 요청 영수증을 함께 교체한다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/141-account-change -B out/study-141 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-141 --target study_account_db account_change_contract -j2
out/study-141/account_change_contract out/study-141/change-test.db
out/study-141/study_account_db 0 out/study-141/account-service.db
```

계약 검사에는 새 DB 경로를 사용한다. Boost 헤더가 기본 경로에 없으면 STUDY_BOOST_INCLUDE를 지정한다. SDL 구성도 같은 계정 타깃을 사용할 수 있다.

## 책임

- meta/account_change_schema.h: 기존 해시 계정에 복구·세대·영수증 추가.
- meta/account_change.h: 형식 검사, 최신 요청 재전송, 현재 비밀 재사용 거절, 원자 교체와 오류 분류.
- meta/sqlite_results.h: 같은 연결 호출의 직렬화와 현재 세대 검사.
- meta/account_service.cpp: loopback HTTP 입력과 결과 변환.
- tests/account_change_contract.cpp: 독립 연결 경쟁·중간 예외·트리거 실패·후보 중복·계정 보존.

서비스 경로는 /study/v1/account/backup, rotate, recover이다. JSON의 credential, next_token, next_recovery는 모두 문자열이다. 새 후보는 호출자가 CSPRNG로 생성하고 전송 전 보존한다. 공개 서버의 TLS·IP 예산·운영 저널 저장은 이 loopback 실습의 역할과 구분한다.

```sh
python3 scripts/check_learning_account_change.py --boost /path/to/boost/include
```

안정적인 학습 스니펫과 현재 서버 레퍼런스의 핵심 비교 기준은 계정 보존·원자 교체·최신 요청 재전송이다. 실제 서버의 테이블 및 HTTP 경로는 다를 수 있다.
