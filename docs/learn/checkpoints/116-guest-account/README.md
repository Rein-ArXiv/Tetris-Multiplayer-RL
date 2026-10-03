# 116 — 익명 계정과 자격 증명

OpenSSL Crypto 개발 패키지·C++17·CMake·Python3가 필요하다.

```sh
cmake -S docs/learn/checkpoints/116-guest-account -B out/study-116 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-116 --target study_account_db account_contract -j2
out/study-116/study_account_db 18082 out/study-116/accounts.db
```

별도 터미널:
```sh
python3 docs/learn/checkpoints/116-guest-account/tools/account_probe.py 18082
python3 scripts/check_learning_guest_account.py
```

도구는 원문 키를 메모리에만 보관한다. 다시 실행하면 새 계정을 만든다.
v5는 빈 account_keys를 추가하며 옛 fixture에 공개ID로 키를 연결하지 않는다.
서버는 loopback fixture다. HTTPS·등록제한·복구/폐기는 실제서비스와 별도이며 운영DB와 혼용하지 않는다.
기존 study_meta_db의 정산 fixture도 보존하므로 인증서버와 같은 공개API로 간주하지 않는다.
Windows 다중 구성은 --config Release와 실행 경로를 맞춘다. 단독 복사는 STUDY_VENDOR_DIR 지정.
