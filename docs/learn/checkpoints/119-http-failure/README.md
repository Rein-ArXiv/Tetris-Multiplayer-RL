# 119 — HTTP 실패·재시도 정책

```sh
cmake -S docs/learn/checkpoints/119-http-failure -B out/study-119 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-119 --target http_retry_contract meta_submit study_meta_db -j2
ctest --test-dir out/study-119 -R '^http_retry_contract$' --output-on-failure
out/study-119/study_meta_db 18081 out/study-119/matches.db
```

별도 터미널에서 meta_submit 18081 119를 두 번 실행하면 같은 row다.
전체 검사는 python3 scripts/check_learning_http_failure.py.
학습 DB/loopback만 사용하며 OpenSSL 개발 패키지가 필요하다.
Windows 다중 구성은 --config Release/-C Release와 실행 경로를 맞춘다.
독립 체크포인트 복사는 STUDY_VENDOR_DIR로 포함 의존 경로를 지정한다.
