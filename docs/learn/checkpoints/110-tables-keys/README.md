# 110 — 테이블과 키

109 누적 코드와 메모리 서비스는 보존하고 SQLite 파일 저장 서비스를 추가한다.
새 실습 DB 경로를 사용하며 실제 계정 DB와 혼용하지 않는다.

```sh
cmake -S docs/learn/checkpoints/110-tables-keys -B out/study-110 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-110 --target study_meta_db meta_submit sqlite_tables_contract -j2
ctest --test-dir out/study-110 -R "^sqlite_tables_contract$" --output-on-failure
```

서버 터미널:
```sh
out/study-110/study_meta_db 18081 out/study-110/lesson.db
```
호출자 터미널:
```sh
out/study-110/meta_submit 18081 17
out/study-110/meta_submit 18081 17
out/study-110/meta_submit 18081 18
```

같은 DB 경로로 서비스를 재시작해도 같은 키/내용은 최초 행 번호를 반환한다.
상대 경로는 실행한 폴더 기준이다. 새 타깃은 SDL 창 없이 실행된다.
SQLite C 코드는 저장소 third_party에서 링크한다. 분리한 체크포인트는
STUDY_VENDOR_DIR에 해당 폴더를 지정한다. Windows 다중 구성은 --config와 실행 폴더를 맞춘다.
study_meta는 메모리 비교용 타깃, study_meta_db가 파일 저장용이다.
127.0.0.1 고정의 로컬 실습이며 인증/구매/BP 지급 API는 없다.
