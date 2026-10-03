# 134 — 위협 모델과 입력 권한 경계

133-sharding의 파일은 CMake/README 외 그대로 유지한다. 새 InputAuthority는 입장을 마친
연결에 서버가 부여한 actor로 한 경기의 입력만 받는다. BoundInputStream이 이를 수신
스트림에 고정한다. 기존 전달기와 게임 창을 대체하지 않는 독립 서버 경계 실습이다.

```sh
cmake -S docs/learn/checkpoints/134-threat-model -B out/study-134 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-134 --target threat_model_contract threat_model_probe -j2
ctest --test-dir out/study-134 -R threat_model --output-on-failure
out/study-134/threat_model_probe
```

Linux는 `--epoll`을 프로브에 줄 수 있다. Windows 다중 구성은 빌드에 `--config Release`,
CTest에 `-C Release`, 실행 경로에 `Release/`를 추가한다. SDL 구성의 `tetris`는 누적 게임이다.

- `net/input_authority.h`: 상태·주체·타입·형식·경기·틱 창을 검사한 뒤 배치 저장.
- `net/bound_input_stream.h`: 16바이트 이하 수신 조각을 파싱하고 고정 actor로 제출.
- `tests/threat_model_contract.cpp`: 거절의 무변경, 배치 원자성, 재시도와 모든 스트림 분할.
- `tools/threat_model_probe.cpp`: 실제 TCP와 CallbackLoop, actor 11/22/99/0 대조.

프로브의 BOUNDARIES 출력은 거절9·저장2·동일 재시도1·양쪽 입력4틱을 뜻한다.
참가자/경기 식별자는 서버의 입장 결과를 대신하는 고정 시험 데이터다. 이 프로브는
인증·TLS·SimGame 결과 검증·보상 저장·실시간 입력 속도 제한을 구현하지 않는다.
전체 자동 검사: `python3 scripts/check_learning_threat_model.py`.
