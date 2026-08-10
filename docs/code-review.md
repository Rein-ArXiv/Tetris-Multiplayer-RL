# 코드 검수 상태

이 문서는 현재 작업 트리의 품질 판단을 기록한다. 사용법 문서가 아니라, 변경 전에
알아야 할 위험과 검증 근거를 먼저 보여 주는 문서다.

## 결론

결정론적 시뮬레이션, C++/Python 프로토콜 동등성, meta/relay 핵심 흐름은 자동
테스트 범위에서 정상이다. `SimGame`을 순수 코어로 두고 플랫폼, 표현, 네트워크,
학습 계층을 분리한 큰 경계도 타당하다.

relay worker 수명은 종료 신호와 drain 대기를 추가해 보완했고, active match 중
SIGTERM 회귀 테스트로 고정했다. 연결 setup과 진행 중 매치에는 IP·시간·전송량
상한을 두고, 계정 중복 session, 단절 몰수 처리, match 저장 재시도도 자동
검증한다. 이번 주기의 하드닝으로 클라이언트 초기화 실패는 침묵하지 않고
(`renderer_init` bool 반환 + `platform_fatal_error` 통지 후 종료), Win32는
per-monitor DPI 인식으로 물리 픽셀 기준 창·프리셋을 만들며, meta의 rate limit은
X-Forwarded-For의 rightmost 토큰만 신뢰해 첫 토큰 위조 우회를 막는다. 방치된
custom room은 대기 데드라인(게스트 무입장 15분, READY 미확정 60초)으로 서버가
정리하고, TCP keepalive는 Windows에서도 POSIX와 같은 15s/5s로 동작한다. 남은
가장 큰 유지보수 과제는 `src/main.cpp`의 집중된 책임이다.

## 우선순위별 검수 결과

### 해결됨 — relay 종료 시 worker 수명 경쟁

기존에는 connection, queue lobby, forwarder worker가 detach된 채 서버 소유 객체의
파괴보다 늦게 끝날 수 있었다. 현재는 connection worker와 relay pump worker의 활성
수를 추적한다. SIGTERM 시 신규 pump를 차단하고 모든 worker가 종료된 뒤에만
`MetaClient`와 net 전역 상태를 파괴한다.

검증:

- `test_relay_sigterm_drains_active_match`가 실제 forwarder를 연 상태에서 SIGTERM을 보낸다.
- relay가 3초 안에 exit code 0으로 종료되지 않으면 실패한다.
- first-frame worker도 전역 종료 상태를 확인해 5초 timeout 전에 빠져나온다.
- Windows에서는 CTRL_BREAK_EVENT(SIGBREAK) 등록으로 같은 우아한 종료 경로를 검증한다.

### 해결됨 — ranked 결과 중복과 단절 회피

relay가 생성한 `match_uuid`를 meta의 unique index와 결과 snapshot에 보존한다. HTTP
응답 유실로 동일 POST가 재시도되어도 최초 match 결과를 반환하고 RP/BP/XP와 승패를
다시 갱신하지 않는다. `post_match` 재시도에는 wall-clock 예산이 걸려 매치 종료
흐름이 재시도 대기로 늘어지지 않는다. 몰수 판정은 수집된 summary 개수로 갈린다 —
양쪽 summary가 있으면 교차검증 경로에 위임하고, 한쪽만 있으면 그 summary의 승패
주장을 존중하며(끊긴 순서 `disconnect_side`는 승자 판정이 아니라 생존자 통지
대상 선정에만 쓴다), summary가 하나도 없는 무경기는 meta에 저장하지 않고 델타 0
결과만 통지해 즉시 이탈 반복형 RP 파밍과 임의 승자 오염을 막는다. relay 자체 종료
중에는 몰수를 만들지 않는다.

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
실패하며, 파일 복사는 meta를 중지한 오프라인 절차로 분리했다. 아카이브 후 중간
산출물 `.db` 스냅샷을 정리하고 최근 `KEEP`개(기본 14)만 남기는 보존 정책이 스크립트
자체에 들어가, 저장 공간이 작은 meta 호스트에서 백업이 디스크를 채우는 경로를 막는다.
정리 실패는 경고만 남기고 이미 성공한 백업을 뒤집지 않는다.

### 해결됨 — 클라이언트 초기화 실패의 침묵

기존에는 GL 3.3 Core 컨텍스트나 렌더러 초기화가 실패해도 프로그램이 검은 창 또는
레거시 컨텍스트로 계속 진행해 원인을 알 수 없었다. 현재는 `platform_init` 실패가
`platform_should_close()`를 세우고, `renderer_init`이 bool을 반환하며, `main()`이
두 신호를 확인해 `platform_fatal_error` 메시지박스로 이유를 알린 뒤 종료 코드 1로
끝난다. 함께 들어간 하드닝: Win32 per-monitor DPI 인식과 `AdjustWindowRectExForDpi`
기반 창 보정, `WM_DPICHANGED` 처리, vsync OFF 시 240fps 상한, `image_init` 멱등화.

### 해결됨 — X-Forwarded-For 첫 토큰 신뢰로 인한 rate limit 우회

meta의 `rate_limit_key`가 XFF의 첫 토큰을 신뢰해, loopback 프록시 뒤 배치에서
클라이언트가 매 요청 다른 XFF를 심으면 public 60/s 버킷을 무한 우회할 수 있었다.
현재는 신뢰 프록시가 마지막에 append한 rightmost 토큰만 사용한다. `CF-Connecting-IP`
우선 로직은 유지된다.

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
| 접속·단절 방어 | 양호 | IP별 setup 제한, 양 플랫폼 15s/5s keepalive, idle·대역폭 상한, 방 대기 데드라인, session lease, summary 기반 몰수 처리 |
| 클라이언트 초기화 | 양호 | 초기화 실패 신호(bool/`platform_should_close`) + `platform_fatal_error` 통지 후 종료, DPI 인식 |
| UI 확장성 | 개선 필요 | mode별 책임이 `src/main.cpp`에 집중 |
| Meta 저장소 | 양호 | DB 직렬화, transaction, migration, UUID 멱등 저장, 일관된 online backup 경로 존재 |
| RL 경계 | 양호 | pybind simulation, common env/model, export 경로가 분리됨 |

## 2026-08-10 검증 결과

GL 하드닝(초기화 실패 신호, DPI 인식, 240fps 상한, `image_init` 멱등화)과 서버
하드닝(몰수 재설계, 방 대기 데드라인, XFF rightmost, keepalive 정합, 백업 보존
정책)이 반영된 현재 작업 트리에서 다음을 확인했다.

```bash
git diff --check
cmake --build build-release --parallel          # Release: tetris + tetris_relay + tetris_meta
./build-release/sim_hash_dump > /tmp/tetris_sim_hash_dump.out
diff -u python/tests/_sim_hash_dump.txt /tmp/tetris_sim_hash_dump.out
.venv/bin/python -m pytest python/tests -q -rs
./build-release/tetris                          # [GL] 3.3 Core 기동 로그 확인
```

결과:

- Release 구성의 게임 클라이언트와 relay/meta 대상 빌드가 경고 없이 통과
- 결정론 골든 비교(`sim_hash_dump`) 일치
- Python 회귀 1657 passed, 5 skipped — 이전 주기에 socket 권한으로 막혔던 meta/relay
  loopback 스모크가 실제 실행으로 전환된 수집 결과다. 남은 skip은 선택 의존성 부재
  사유로, `-rs` 출력에서 의도와 일치함을 확인했다
- SDL/OpenGL 클라이언트가 GL 3.3 Core 컨텍스트로 기동하는 것을 확인

이 문서는 실행하지 못한 검증을 통과로 간주하지 않는다.

## 변경 시 반드시 지킬 계약

- `SimGame` 변경: 같은 seed와 tick input은 모든 플랫폼에서 같은 `StateHash`를 만든다.
- framing 변경: C++과 `python/netbot/framing.py`의 바이트 표현이 같다.
- placement 변경: C++ bot과 Python input expander의 action 해석이 같다.
- ranked match 변경: 양쪽 summary가 있으면 교차검증 일치가 반영 조건이고, 한쪽만 있으면 그 summary의 승패를 따르며, summary가 없으면 RP를 반영하지 않는다.
- ranked 단절 변경: `disconnect_side`는 통지 대상 선정 전용이다 — 승자 판정에 쓰지 않는다. relay 종료는 몰수가 아니다.
- match 저장 변경: 동일 `match_uuid` 재시도는 최초 결과를 반환하고 보상을 중복 반영하지 않는다.
- DB 변경: 기존 `PRAGMA user_version` 데이터가 반복 실행에도 한 번만 이관된다.
- 서버 변경: unranked는 meta 없이 동작하고, ranked match POST는 relay secret을 요구한다.

## 아직 자동 검증하지 못한 범위

- Windows Win32/WGL·GDI+ 이미지 decode·XAudio2 실제 실행
- Win32 DPI 인식의 시각 효과 — 고배율(125%/150%) 모니터에서 프리셋 창이 물리 픽셀로
  정확히 잡히는지, `WM_DPICHANGED`로 모니터 간 이동 시 창 크기가 유지되는지는
  고배율 모니터 실기에서 눈으로 확인해야 한다
- macOS SDL 앱 번들 실제 실행
- ONNX Runtime을 포함한 모델별 인게임 추론
- 장시간 실제 WAN lockstep과 패킷 지연/단절 복구
- 실제 공인망에서 200명 장시간 soak 중 지연·패킷 손실·동시 종료·공격이 겹치는 상황
