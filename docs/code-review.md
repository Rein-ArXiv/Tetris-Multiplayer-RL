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

### 해결됨 — 실행 중 SQLite의 불일치 백업 가능성

기존 `scripts/backup_meta_db.sh`는 `sqlite3` CLI가 없을 때 실행 중인 `.db`, `-wal`,
`-shm` 파일을 차례로 복사했다. WAL 쓰기가 그 사이 진행되면 세 파일이 서로 다른
시점을 나타낼 수 있어 “전부 복사했다”는 사실만으로는 일관된 스냅샷이 되지 않는다.
현재 스크립트는 SQLite online backup API만 사용하고, 복사본의
`PRAGMA integrity_check`가 `ok`인 경우에만 아카이브한다. CLI가 없으면 명시적으로
실패하며, 파일 복사는 meta를 중지한 오프라인 절차로 분리했다.

### P2 — `src/main.cpp`의 책임 집중

`src/main.cpp`가 CLI, 설정 영속화, 인증 부트스트랩, 메뉴 FSM, 네트워크 진행,
bot roster, 게임 루프와 화면별 UI를 함께 소유한다. 기능은 동작하지만 새 메뉴나 설정을
추가할 때 기존 분기를 직접 수정해야 하므로 OCP 관점에서 확장 지점이 약하다.

메뉴 자체의 label과 실행 동작은 이제 `MenuItem { label, action }`으로 함께 묶이고
`MenuAction` switch로 dispatch된다. 표시 순서를 바꿨을 때 숫자 index가 다른 화면을
여는 결합은 제거됐다. 남은 문제는 메뉴 한 곳이 아니라 각 화면의 상태·update·render가
여전히 같은 함수에 모여 있다는 점이다.

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

### P2 — user-data 경로가 HTTP 클라이언트에 결합됨

`settings_file_path()`는 네트워크와 무관한 플랫폼 저장 경로인데
`meta/http_client.*`에 정의돼 있다. 그 결과 설정 화면만 구현하려 해도 meta 클라이언트
소스와 `httplib.h` 의존성을 먼저 가져와야 하며, 학습 문서에서도 설정 Part가 meta Part
뒤에 놓이는 인위적인 순서가 생긴다.

동작 오류는 아니므로 이번 변경에서 파일을 옮기지는 않았다. 리팩터링할 때
`platform/user_data.*` 같은 작은 모듈로 기준 디렉터리와 settings/token 경로 생성을
옮기고, `meta/http_client.*`는 토큰 파일 읽기·쓰기와 HTTP만 소유하게 하면 된다.
그러면 설정 UI는 플랫폼·렌더러·오디오 뒤에 바로 구현할 수 있고 meta는 온라인
프로필 기능으로 독립된다.

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
| Meta 저장소 | 양호 | DB 직렬화, transaction, migration, UUID 멱등 저장, 일관된 online backup 경로 존재 |
| RL 경계 | 양호 | pybind simulation, common env/model, export 경로가 분리됨 |

## 2026-08-09 검증 결과

현재 작업 트리에서 다음을 확인했다.

```bash
git diff --check
cmake --build /tmp/tetris-game-build --parallel
cmake --build /tmp/tetris-hardening-build --parallel
/tmp/tetris-hardening-build/sim_hash_dump > /tmp/tetris_sim_hash_dump.out
diff -u python/tests/_sim_hash_dump.txt /tmp/tetris_sim_hash_dump.out
.venv/bin/python -m pytest python/tests -q
```

결과:

- SDL/OpenGL 게임 클라이언트와 relay/meta 대상 빌드 통과
- 결정론 골든 비교 통과
- 소켓을 쓰지 않는 Python 회귀 항목 통과

이 검수 주기의 앞선 네트워크 허용 실행에서는 meta/relay loopback 통합 테스트가
통과했다. 마지막 문서·메뉴 action·CSPRNG 읽기 변경 뒤 재실행은 샌드박스의 socket
권한과 승인 한도 때문에 `PermissionError` 단계에서 막혔다. 제품 assertion 실패로
해석할 결과는 아니지만, **최종 merge 전에는 권한 있는 CI나 로컬 호스트에서 전체
통합 테스트를 다시 통과시켜야 한다.** 이 문서는 실행하지 못한 검증을 통과로
간주하지 않는다.

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

- Windows Win32/WGL·GDI+ 이미지 decode·XAudio2 실제 실행
- macOS SDL 앱 번들 실제 실행
- ONNX Runtime을 포함한 모델별 인게임 추론
- 장시간 실제 WAN lockstep과 패킷 지연/단절 복구
- 실제 공인망에서 200명 장시간 soak 중 지연·패킷 손실·동시 종료·공격이 겹치는 상황
