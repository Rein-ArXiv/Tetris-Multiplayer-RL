# 136 — WSS 종단과 메시지 경계

135의 누적 코드에 MessageStream, 메시지 계약 검사, WSS 왕복 진단기를 추가한다.
메시지마다 게임 파서를 초기화하지 않으며 미완성 꼬리는 다음 메시지로 이어진다.
학습용 wire와 현재 게임의 wire는 별개다. 실제 게이트웨이는 내부 바이트를 해석하지 않는다.

```sh
cmake -S docs/learn/checkpoints/136-wss-tunnel -B out/study-136 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-136 --target message_stream_contract wss_roundtrip -j2
ctest --test-dir out/study-136 -R message_stream_contract --output-on-failure
python3 scripts/check_learning_wss_tunnel.py
```

OpenSSL 개발 라이브러리와 Boost.Beast 헤더가 필요하다. 별도 설치 경로라면 CMake에는
`-DSTUDY_BOOST_INCLUDE=/path/to/include`, 검사에는 `--boost /path/to/include`를 지정한다.
검사 스크립트는 임시 CA·인증서를 발급하고 loopback echo → 현재 WSS 게이트웨이 →
진단기를 연결한 뒤 프로세스와 키를 회수한다. 외부 서비스·계정·실사용 키가 필요 없다.

- `net/message_stream.h`: 완성 binary 메시지를 게임 프레임으로 이어 읽는 어댑터.
- `tests/message_stream_contract.cpp`: 분할 위치·payload·용량·오류 앞부분·EOF 검사.
- `tools/wss_roundtrip.cpp`: 체인과 SAN 검사 → Upgrade → split/bundle/fragment 왕복.

프로브 단독 사용은 `wss_roundtrip HOST PORT /path/to/ca.pem split`이다. `bundle`,
`fragment`도 선택할 수 있고 마지막 인수에 허용된 Origin을 줄 수 있다. 상대는
게임 서버 대신 받은 바이트를 되돌리는 실험용 backend여야 한다. 동기 도구이므로
그래픽 루프에서 호출하지 않으며, 실행자가 외부 시간 제한을 건다. 검사 스크립트는
이를 적용한다. 성공은 바이트/프레임 복원이며 계정 인증이나 게임 입장 성공이 아니다.

어댑터는 유효한 프레임을 순서대로 sink에 전달한다. 뒤의 오류가 앞서 전달한 프레임을
취소하지 않는다. sink 예외도 상태를 되돌리지 않으므로 호출자는 해당 스트림을 버린다.
finish는 전송 종료 뒤의 미완성 꼬리를 검사하며 메시지마다 호출하지 않는다.

누적 게임은 `STUDY_PLATFORM=SDL` 구성의 tetris 타깃이다. Windows 다중 구성에서는
`--config Release`, CTest의 `-C Release`와 실행 파일의 `Release/` 경로를 사용한다.
라이브러리는 대상 OS·아키텍처에 맞춰 준비한다.
