# 123 — 저장 결과의 랭킹 조회

```sh
cmake -S docs/learn/checkpoints/123-ranking -B out/study-123 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-123 --target tetris study_account_db study_meta_db meta_submit ranking_probe ranking_contract account_ui_contract -j2
ctest --test-dir out/study-123 -R '^(ranking_contract|account_ui_contract)$' --output-on-failure
out/study-123/study_account_db 18080 out/study-123/shared.db
```

둘째 터미널: out/study-123/study_meta_db 18081 out/study-123/shared.db
셋째: out/study-123/ranking_probe 18080 → meta_submit 18081 123 → ranking_probe 18080
게임: out/study-123/tetris --account 18080 out/study-123/my-account
서버 두 개는 같은 DB 파일을 사용한다. 모두loopback실습용이며 공개POST권위인증은없다.
계정/랭킹버튼 → 연결/새로고침 Space, 뒤로 Escape.
검사: python3 scripts/check_learning_ranking.py
SDL2/OpenSSL/C++17필요.헤드리스는SCRIPTED+ranking_probe.독립복사는STUDY_VENDOR_DIR.
다중구성은--config Release/-C Release와실행경로를맞춘다.
