# 121 — 미확인 경기 제출의 파일 보관

```sh
cmake -S docs/learn/checkpoints/121-save-uncertainty -B out/study-121 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-121 --target meta_outbox meta_submit study_meta_db outbox_contract bootstrap_contract -j2
ctest --test-dir out/study-121 -R '^(outbox_contract|bootstrap_contract)$' --output-on-failure
out/study-121/meta_outbox 18083 out/study-121/result-121 prepare 121
out/study-121/study_meta_db 18083 out/study-121/study.db
```

다른 터미널: out/study-121/meta_outbox 18083 out/study-121/result-121 resume
같은 폴더로 다시 resume하면 보관한 영수증을 사용한다. 다른 key는 새 폴더로 준비한다.
전체 검사: python3 scripts/check_learning_save_uncertainty.py
OpenSSL 개발 패키지 필요. SDL 선택 시 SDL2 필요. 다중 구성은 --config Release/-C Release.
독립 체크포인트 복사는 STUDY_VENDOR_DIR로 포함 의존성 경로를 지정한다.
