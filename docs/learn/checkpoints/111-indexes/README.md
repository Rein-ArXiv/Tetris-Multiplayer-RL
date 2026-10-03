# 111 — 인덱스와 최근 경기 조회

110 누적 구현에 두 조회 인덱스와 recent(player,limit)를 추가한다.
반환은 양쪽 참가자 자리의 저장 행 ID 내림차순이며 실제 경기 종료 시각순이 아니다.

```sh
cmake -S docs/learn/checkpoints/111-indexes -B out/study-111 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-111 --target study_meta_db meta_submit history_list index_probe -j2
out/study-111/index_probe
ctest --test-dir out/study-111 -R "^index_query_contract$" --output-on-failure
```

서버 터미널:
```sh
out/study-111/study_meta_db 18081 out/study-111/history.db
```
호출·조회 터미널:
```sh
out/study-111/meta_submit 18081 17
out/study-111/meta_submit 18081 18
out/study-111/history_list out/study-111/history.db 101 5
out/study-111/history_list out/study-111/history.db 202 1
```

새 DB의101은 row2/key18, row1/key17.202 상한1은 row2/key18이다.
DB 상대 경로는 현재 작업 폴더 기준. 학습 DB 전용이며 실제 계정 DB와 혼용하지 않는다.
history_list는 로컬 파일을 열며 없는 파일/잘못된ID·상한은 거절한다. 공개 인증 API가 아니다.
index_probe는 메모리 DB의 진단 도구다. 계획 문구·정확한 작업 계수는 엔진/입력에 따라 달라진다.
새 타깃은 창을 사용하지 않는다. 분리한 체크포인트는 STUDY_VENDOR_DIR를 지정한다.
Windows 다중 구성에서는 --config와 실행 파일의 구성 하위 폴더를 맞춘다.
