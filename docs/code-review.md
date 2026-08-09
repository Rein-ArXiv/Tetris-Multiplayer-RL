# 코드 검수 상태

이 문서는 현재 작업 트리의 품질 판단을 기록한다. 사용법 문서가 아니라, 변경 전에
알아야 할 위험과 검증 근거를 먼저 보여 주는 문서다.

## 결론

결정론적 시뮬레이션, C++/Python 프로토콜 동등성, meta/relay 핵심 흐름은 자동
테스트 범위에서 정상이다. `SimGame`을 순수 코어로 두고 플랫폼, 표현, 네트워크,
학습 계층을 분리한 큰 경계도 타당하다.

relay worker 수명은 종료 신호와 drain 대기를 추가해 보완했고, active match 중
SIGTERM 회귀 테스트로 고정했다. 연결 setup과 진행 중 매치에는 IP·시간·전송량
상한을 두고, 계정 중복 session, 갑작스러운 단절 기권, match 저장 재시도도 자동
검증한다. 남은 가장 큰 유지보수 과제는 `src/main.cpp`의 집중된 책임이다.

## 우선순위별 검수 결과

### 해결됨 — relay 종료 시 worker 수명 경쟁

기존에는 connection, queue lobby, forwarder worker가 detach된 채 서버 소유 객체의
파괴보다 늦게 끝날 수 있었다. 현재는 connection worker와 relay pump worker의 활성
수를 추적한다. SIGTERM 시 신규 pump를 차단하고 모든 worker가 종료된 뒤에만
`MetaClient`와 net 전역 상태를 파괴한다.

검증:

- `test_relay_sigterm_drains_active_match`가 실제 forwarder를 연 상태에서 SIGTERM을 보낸다.
- relay가 3초 안에 exit code 0으로 종료되지 않으면 실패한다.
- first-frame worker도 전역 종료 상태를 확인해 3초 timeout 전에 빠져나온다.

### 해결됨 — ranked 결과 중복과 단절 회피

relay가 생성한 `match_uuid`를 meta의 unique index와 결과 snapshot에 보존한다. HTTP
응답 유실로 동일 POST가 재시도되어도 최초 match 결과를 반환하고 RP/BP/XP와 승패를
다시 갱신하지 않는다. 한 peer가 summary 전에 끊기면 서버가 관측한 방향을 기권으로
저장하되, relay 자체 종료 중에는 플레이어 패배로 만들지 않는다.

검증:

- 같은 UUID를 두 번 저장해도 match 행과 보상이 한 번만 변한다.
- 같은 player_id의 두 번째 활성 ranked session은 입장 단계에서 거부된다.
- 진행 중 한 peer를 끊으면 survivor만 결과를 받고 relay는 새 연결을 계속 받는다.

### P2 — `src/main.cpp`의 책임 집중

`src/main.cpp`가 CLI, 설정 영속화, 인증 부트스트랩, 메뉴 FSM, 네트워크 진행,
bot roster, 게임 루프와 화면별 UI를 함께 소유한다. 기능은 동작하지만 새 메뉴나 설정을
추가할 때 기존 분기를 직접 수정해야 하므로 OCP 관점에서 확장 지점이 약하다.

권장 분리 순서:

1. `GameSettings` load/save와 검증을 별도 모듈로 이동
2. guest/meta profile 상태를 작은 service로 이동
3. 화면별 update/render를 mode handler로 이동
4. mode 전환과 게임 세션 소유권만 app controller에 남김

한 번에 전면 재작성하지 말고 설정 또는 Customize 화면처럼 경계가 분명한 기능부터
옮기는 편이 회귀 범위를 제어하기 쉽다.

### P2 — 수동 JSON protocol의 확장 비용

`meta/protocol.h`의 제한된 수동 parser/serializer는 현재 고정된 내부 payload에는
작고 빠르지만, JSON escape, 타입 변화, 중첩 구조가 늘어날수록 각 endpoint가 parser의
암묵적 제약에 의존한다. 공개 API 필드가 확장되면 구조화 JSON 라이브러리 도입 또는
parser 계약 테스트 확대가 필요하다.

## 유지보수 관점 평가

| 영역 | 판단 | 근거 |
|---|---|---|
| 게임 규칙 | 양호 | `SimGame`이 렌더링·오디오 없이 단일 규칙 소스를 제공 |
| 결정론 | 양호 | 고정 RNG/해시와 C++ 골든, Python 교차 테스트 존재 |
| 플랫폼 교체 | 양호 | `platform.h`, `audio.h` 뒤에 Win32/SDL 구현 분리 |
| 네트워크 프로토콜 | 보통 이상 | framing/session/relay 분리와 parity 테스트 존재 |
| 서버 종료 수명 | 양호 | worker 활성 수 추적, 신규 pump 차단, SIGTERM drain 테스트 |
| 접속·단절 방어 | 양호 | IP별 setup 제한, keepalive, idle·대역폭 상한, session lease, 기권 처리 |
| UI 확장성 | 개선 필요 | mode별 책임이 `src/main.cpp`에 집중 |
| Meta 저장소 | 양호 | DB 직렬화, transaction, migration, UUID 멱등 저장 및 smoke 테스트 존재 |
| RL 경계 | 양호 | pybind simulation, common env/model, export 경로가 분리됨 |

## 2026-08-09 검증 결과

현재 작업 트리에서 다음을 확인했다.

```bash
git diff --check
cmake --build build-review -j2 --target sim_hash_dump tetris_relay tetris_meta
./build-review/sim_hash_dump > /tmp/tetris_sim_hash_dump.out
diff -u python/tests/_sim_hash_dump.txt /tmp/tetris_sim_hash_dump.out
.venv/bin/python -m pytest python/tests -q
```

결과:

- C++ 대상 빌드 통과
- 결정론 골든 비교 통과
- Python 전체 스위트의 수집 항목이 실패 없이 완료됨
- skip은 선택 의존성/환경에 따른 것인지 `-rs`로 사유를 별도 확인

샌드박스에서 loopback socket 권한 없이 실행하면 meta/relay 테스트가
`PermissionError`로 실패한다. 이는 제품 코드 실패가 아니며, 네트워크 권한을 허용한
동일 테스트는 통과했다.

## 변경 시 반드시 지킬 계약

- `SimGame` 변경: 같은 seed와 tick input은 모든 플랫폼에서 같은 `StateHash`를 만든다.
- framing 변경: C++과 `python/netbot/framing.py`의 바이트 표현이 같다.
- placement 변경: C++ bot과 Python input expander의 action 해석이 같다.
- ranked match 변경: 양쪽 summary가 일치할 때만 RP/BP/XP를 반영한다.
- ranked 단절 변경: summary 전 peer 단절은 기권이고 relay 종료는 기권이 아니다.
- match 저장 변경: 동일 `match_uuid` 재시도는 최초 결과를 반환하고 보상을 중복 반영하지 않는다.
- DB 변경: 기존 `PRAGMA user_version` 데이터가 반복 실행에도 한 번만 이관된다.
- 서버 변경: unranked는 meta 없이 동작하고, ranked match POST는 relay secret을 요구한다.

## 아직 자동 검증하지 못한 범위

- Windows Win32/GDI/XAudio2 실제 실행
- macOS SDL 앱 번들 실제 실행
- ONNX Runtime을 포함한 모델별 인게임 추론
- 장시간 실제 WAN lockstep과 패킷 지연/단절 복구
- 실제 공인망에서 200명 장시간 soak 중 지연·패킷 손실·동시 종료·공격이 겹치는 상황
