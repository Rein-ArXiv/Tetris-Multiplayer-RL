# 113 — 결과와 보상의 원자적 확정

112에 v3 wallets/match_rewards를 추가한다. 새 경기 BP는 승10/패3/무승부0.
구 경기에는 정책0/지급0을 기록하고 소급 지급하지 않는다. 기존 행과 선택 아이콘을 보존한다.

```sh
cmake -S docs/learn/checkpoints/113-transactions -B out/study-113 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-113 --target study_meta_db reward_probe reward_contract -j2
out/study-113/reward_probe out/study-113/rewards.db 17
out/study-113/reward_probe out/study-113/rewards.db 17
out/study-113/reward_probe out/study-113/rewards.db 18
```

새 DB에서 출력은 row1/10,3/10,3 → 동일 → row2/10,3/20,6.
reward_probe는 조회만 하지 않고 경기를 제출한다. 실습용 사본에서 사용한다.
`pause-before`와 `pause-after`는 COMMIT 전/후 Enter 대기이며 로컬 실패 실험용이다.
`python3 scripts/check_learning_transactions.py`가 임시 파일·프로세스를 관리한다.
기존 학습 DB는 서비스를 멈추고 일관된 백업 사본에서 이관한다. 실제 계정 DB와 호환되지 않는다.
Windows 다중 구성에서는 --config와 구성 하위 폴더를 맞춘다. 분리한 체크포인트는 STUDY_VENDOR_DIR를 지정한다.
