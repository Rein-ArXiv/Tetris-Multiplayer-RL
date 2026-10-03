# 117 — 아이콘 구매·소유권·선택

스키마v5를 유지한다. 서버 카탈로그의 default0/ruby100/gold250 BP를 사용한다.
구매는 소유권만 추가하며 선택은 별도다. 운영DB와 혼용하지 않는 loopback fixture다.

```sh
cmake -S docs/learn/checkpoints/117-icon-ownership -B out/study-117 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-117 --target study_account_db shop_contract -j2
out/study-117/study_account_db 18082 out/study-117/accounts.db
```

별도 터미널:
```sh
python3 docs/learn/checkpoints/117-icon-ownership/tools/shop_probe.py 18082
python3 scripts/check_learning_icon_ownership.py
```

새 계정은BP0이므로ruby구매402가정상이다. 성공/경합검사는소유한임시DB에만잔액을준비한다.
키는메모리에만보관하며다시실행하면새계정이다. OpenSSL개발패키지가필요하다.
Windows다중구성은--config Release/실행경로를맞춘다.체크포인트단독복사는STUDY_VENDOR_DIR지정.
