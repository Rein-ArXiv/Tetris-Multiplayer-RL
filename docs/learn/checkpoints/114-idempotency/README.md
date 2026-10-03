# 114 — 멱등 정산 응답

113의v3스키마와지급정책을유지하며 /study/v2/matches가당시지급량/정책을반환한다.
v1은기존네필드영수증유지.같은키/내용은원래결과,다른내용409,조회실패는저장오류.

```sh
cmake -S docs/learn/checkpoints/114-idempotency -B out/study-114 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-114 --target study_meta_db settlement_probe idempotency_contract -j2
```

서버:
```sh
out/study-114/study_meta_db 18081 out/study-114/study.db
```
별도터미널:
```sh
out/study-114/settlement_probe 18081 17
out/study-114/settlement_probe 18081 18
out/study-114/settlement_probe 18081 17
```
새DB의첫째/셋째는row1 awards10,3 policy1.구경기는policy0/0,0.
검사는저장소루트에서 `python3 scripts/check_learning_idempotency.py`. 임시파일/프로세스만소유한다.
Windows다중구성은--config와구성폴더를맞춘다.분리한체크포인트는STUDY_VENDOR_DIR지정.
공개인증API가아니며실제계정DB와혼용하지않는다.
