# 115 — RP·XP·BP와 레벨

경기·BP 지급에 RP/XP를 같은 트랜잭션으로 연결한다. v1/v2 HTTP 응답은 그대로 유지한다.
새 DB: BP10/3, XP100/50, RP0시작·바닥0. 무승부는 변화 없음.
v3 학습 DB는 v4로 이관하며 과거 BP는 보존하고 과거 RP/XP는 소급 지급하지 않는다.
실제 계정 DB와 혼용하지 않는다.

```sh
cmake -S docs/learn/checkpoints/115-progression -B out/study-115 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-115 --target study_meta_db progression_probe progression_contract -j2
out/study-115/progression_probe out/study-115/progression.db 17
out/study-115/progression_probe out/study-115/progression.db 18
out/study-115/progression_probe out/study-115/progression.db 17
```

첫 키17에서 A RP16·XP100·BP10, B RP0·XP50·BP3. 키18 뒤 키17 재시도는 지급을 늘리지 않는다.
`python3 scripts/check_learning_progression.py`는 임시 파일로 계약을 검사한다.
Windows 다중 구성은 --config Release와 Release 실행 경로를 맞춘다.
체크포인트 단독 복사는 STUDY_VENDOR_DIR를 지정한다.
