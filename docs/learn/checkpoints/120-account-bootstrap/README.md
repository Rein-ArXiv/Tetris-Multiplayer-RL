# 120 — 계정 저장과 인증

```sh
cmake -S docs/learn/checkpoints/120-account-bootstrap -B out/study-120 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-120 --target account_bootstrap study_account_db bootstrap_contract -j2
ctest --test-dir out/study-120 -R '^bootstrap_contract$' --output-on-failure
out/study-120/study_account_db 18082 out/study-120/accounts.db
```

다른 터미널: out/study-120/account_bootstrap 18082 out/study-120/profile-local
동일 DB·포트·폴더로 재실행하면 같은 ID다. 키를 출력하지 않는다.
전체 검사: python3 scripts/check_learning_account_bootstrap.py
OpenSSL 개발 패키지 필요. SDL 선택 시 SDL2도 필요.
Windows 다중 구성은 --config Release/-C Release와 실행 경로를 맞춘다.
독립 복사는 STUDY_VENDOR_DIR로 포함 의존성 경로를 지정한다.
