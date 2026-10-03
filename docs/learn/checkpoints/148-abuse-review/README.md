# 검증된 경기의 운영 관찰

규칙상 가능한 경기와 사람의 의도를 구분한다. 누적 서버 재현으로 경기 fixture를 저장한 뒤
최근 저장 행을 제한해서 읽는다. 계정쌍을 정규화하고 반복 상대·일방적 승패·짧은 경기라는
관찰을 표시한다. 이 실습은 자동 제재나 실제 게임 경제 정책을 적용하지 않는다.

- `meta/review_policy.h`: 순수 분류, 불가능한 카운터 거절, 오버플로 없는 비교.
- `meta/review_sample.h`: 내림차순 행 검증, 계정쌍 집계, 정책/표본/누락 범위 보존.
- `meta/review_reader.h`: 학습 스키마의 SQLite 읽기 전용 연결, 필요한 필드만 단일 SELECT.
- `tools/review_results.cpp`: 운영자 로컬 CLI. 식별자도 공개 보고서에 무분별하게 싣지 않는다.
- `tools/review_fixture.cpp`: 실제 AuthoritativeMatch로 만든 결과. 정책 검사의 고정 실습 조건.

## 실행

```sh
cmake -S docs/learn/checkpoints/148-abuse-review -B out/study-148 \
  -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-148 --target review_contract review_results review_fixture -j2
out/study-148/review_contract out/study-148/contracts-new.db
out/study-148/review_fixture out/study-148/fixture.db
# 아래 값은 이 실습에서 바꾸어 볼 표본/분류 정책이다.
out/study-148/review_results out/study-148/fixture.db 3 3 2 2 1000
out/study-148/review_results out/study-148/fixture.db 8 3 2 2 1000
python3 scripts/check_learning_abuse_review.py --boost /path/to/boost/include
```

계약 검사는 새 DB를 받는다. 같은 fixture 생성 명령을 다시 실행해도 같은 경기 키는
추가 보상을 만들지 않는다. Boost가 기본 경로 밖에 있으면 STUDY_BOOST_INCLUDE를 지정한다.
관찰 도구의 인자는 DB, 행 상한, 반복 경기 수, 편향 승리 수, 짧은 경기 수, 짧은 틱 상한이다.

현재 게임 DB와 학습 DB는 스키마가 다르다. reader는 application_id와 스키마 버전을
대조하여 다른 DB를 거절한다. reader의 행 순서는 저장 순서이며 실제 경기 발생 시각이나
최근 며칠이라는 뜻이 아니다. 제한된 표본에 신호가 없어도 전체 이용 이력이 정상이라고
결론내리지 않는다. 실제 서비스 운영에 적용하려면 시간/정책 버전/검토·보존 절차를 정한다.

SQLite 읽기 전용은 이 도구의 쓰기를 막는다. 다른 프로세스의 DB 갱신이나 읽는 동안의
잠금 비용까지 없애지는 않는다. WAL/백업/서비스 부하 정책은 별도로 유지한다.
