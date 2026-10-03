# 122 — 계정 화면과 비동기 서비스

```sh
cmake -S docs/learn/checkpoints/122-account-screen -B out/study-122 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-122 --target tetris study_account_db account_ui_probe account_ui_contract -j2
ctest --test-dir out/study-122 -R '^account_ui_contract$' --output-on-failure
out/study-122/study_account_db 18080 out/study-122/accounts.db
```

두 번째 터미널(저장소 루트): out/study-122/tetris --account 18080 out/study-122/my-account
계정 버튼 → 연결/재시도(Space), 뒤로(Escape). 처리 중에도 표시를 닫을 수 있다.
프로세스 종료 뒤 같은 폴더로 재실행하면 저장된 키를 사용한다. 미저장 경고 중 종료는 키를 잃을 수 있다.
전체 검사: python3 scripts/check_learning_account_screen.py
헤드리스는 SCRIPTED와 account_ui_probe/account_ui_contract 타깃을 사용한다.
C++17/CMake/SDL2/OpenSSL 필요. 독립 복사는 STUDY_VENDOR_DIR를 지정한다.
다중 구성 생성기는 --config Release/-C Release와 실행 경로를 맞춘다.
