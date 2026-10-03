# 137 — 일회용 입장권: 발급·소비·만료

누적 게임에 입장권 저장소와 별도 상태 전이 진단기를 추가한다. 게임의 로그인 흐름에
진단 저장소를 연결하는 단계는 아니다. 저장소는 계정 인증·난수 생성·HTTP를 맡지 않는다.

- `meta/admission_tickets.h`: 용량을 템플릿으로 받는 저장소. 유효한 기존 입장권은
  교체 실패 시 유지하고, 소비는 조회·값 복사·제거를 같은 잠금 안에서 처리한다.
- `meta/ticket_wire.h`: OS 기반 암호학적 난수 생성 경계와 목적별 접두사·hex 파싱.
- `tests/admission_ticket_contract.cpp`: 실패 시 보존, 용량, 만료 경계, 동시 소비,
  자격 세대 불일치 후 재사용, wire 형식과 난수원 실패 검사.
- `tools/admission_ticket_demo.cpp`: 발급→교체→소비→응답을 버린 뒤 재시도→만료 관찰.
  입장권 문자열은 출력하지 않는다.

```sh
cmake -S docs/learn/checkpoints/137-admission-tickets -B out/study-137 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-137 --target admission_ticket_contract admission_ticket_demo -j2
ctest --test-dir out/study-137 -R '^admission_ticket_(contract|demo)$' --output-on-failure
python3 scripts/check_learning_admission_tickets.py
```

C++17·OpenSSL 개발 라이브러리·Boost.Beast 헤더가 필요하다. 별도 헤더 경로는 CMake에
`-DSTUDY_BOOST_INCLUDE=/path/to/include`, 검사 스크립트에는 `--boost /path/to/include`로
전달한다. 스크립트는 계약 검사와 현재 GameTickets의 ASan/UBSan 검사를 수행한다.
`--snippets-only`는 본문 발췌·현재 심볼·누적 파일 보존만 검사한다.

시간은 `steady_clock` 값을 인수로 공급한다. 진단기의 작은 용량과 수명은 실험 조건이며
서비스 정책은 호출자가 정한다. 만료 시각과 관찰 시각이 같으면 거절한다. 소비 성공 뒤
응답 전달이나 계정 세대 검사가 실패해도 항목을 되살리지 않는다. 저장소는 제거된 키를
영구 기억하지 않으므로 매 발급의 새롭고 예측하기 어려운 키는 난수원 계약이다.
잠금은 한 프로세스의 저장소 인스턴스에만 적용한다.

실제 메타 API의 HTTP·계정 DB 경계는 저장소 단위 검사와 별도로 실행한다.

```sh
cmake -S . -B out/study-137-meta -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_PY=OFF -DTETRIS_BUILD_META=ON -DTETRIS_BUILD_RELAY=OFF -DTETRIS_BUILD_REACTOR=OFF -DTETRIS_BUILD_TEST=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-137-meta --target tetris_meta game_tickets_test -j2
ctest --test-dir out/study-137-meta -R '^game_tickets$' --output-on-failure
TETRIS_META_BIN="$PWD/out/study-137-meta/tetris_meta" python3 -m pytest -q python/tests/test_secure_admission.py::test_ticket_scope_secret_and_atomic_consumption python/tests/test_secure_admission.py::test_game_ticket_issue_rate_limit python/tests/test_secure_admission.py::test_consumed_ticket_is_not_restored_when_reply_is_discarded python/tests/test_secure_admission.py::test_pending_ticket_does_not_survive_meta_restart python/tests/test_account_security.py::test_rotation_revokes_old_key_and_outstanding_game_ticket
```

HTTP 검사는 pytest와 requests가 필요하며 임시 DB·loopback 프로세스를 사용한다.
성공 헤더만 읽고 응답 본문을 버리는 검사와 같은 DB로 서버를 재시작하는 검사가 포함된다.
전체 경기·다중 서버의 분산 원자성을 검증하는 검사는 아니다.

누적 그래픽 게임은 `STUDY_PLATFORM=SDL` 구성의 `tetris` 타깃으로 빌드한다.
Windows 다중 구성에서는 `--config Release`, CTest의 `-C Release`와 실행 파일의
`Release/` 경로를 사용한다. 라이브러리는 대상 OS·아키텍처에 맞춰 준비한다.
