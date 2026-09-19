# 폴리싱 변경과 검증 기록

## 2026-09-19 — 서버 판정·계정 경계·사용자 안내·문서 동기화

현재 검증은 Linux의 로컬 작업 트리 기준이다. [Part 17](blog/part17-guest-account-recovery.md)은
계정 변경·실패 복구를, [Part 18](blog/part18-authoritative-results.md)은 서버 승패 판정·JSON
검증·결과 상태를 설명한다. [품질 검토](architecture-quality-review.md)에 발견한 문제와
해결 범위, 남아 있는 출시 조건을 구분했다.

| 확인 | 실제 결과 |
|---|---|
| WSS=ON Release 빌드 | 게임·meta·thread/reactor relay·게이트웨이·probe·단위 검사 빌드 성공 |
| WSS=OFF Release 빌드 | 게임·meta·두 relay·단위 검사 빌드 성공. `out/polish/quality-wss-off-build.log` |
| 전체 Python 회귀 | **1,784 passed, 2 skipped**, 200.82초. `out/polish/quality-full-tests.log` |
| 계정·보안 입장·두 relay 결과 검증 표적 검사 | **65 passed**, 12.83초. `out/polish/quality-focused-final.log` |
| CTest WSS=ON / OFF | **각각 9/9 통과**. `out/polish/quality-ctest.log`, `quality-wss-off-ctest.log` |
| Part 형식·현재 소스 발췌·로컬 파일 링크 | `scripts/check_part_docs.py` 통과. CI에도 같은 검사 추가 |
| 변경 공백 오류 | `git diff --check` 통과 |

전체 실행에서는 loopback reactor를 실제로 띄웠으며 두 relay의 경기 검증 fixture는 각각
별도 서버를 띄운다. 제외한 두 검사는 설치되지 않은 `torch`·`gymnasium`에 의존한다.
서버 미기동으로 생긴 skip은 없다. 표적 검사 65개는 전체 결과와 중복되며 더해서 세지 않는다.

주요 회귀는 일치하는 거짓 승리 신고의 무보상, 실제 입력으로 끝난 경기의 서버 승패/통계,
입력 변조·재작성·seed 변경 거절, 미완료 연결 종료의 무보상, JSON 전체 형식·중복 키 검사,
다른 서버로의 키 전송 차단, 최초 키 저장 실패 후 같은 계정 저장 재시도, 한글 경로,
credential 변경의 원자성·응답 유실·복구·입장권 폐기를 확인한다.

문서에서는 Part 0~18의 현재 소스 발췌를 전부 대조하고 필요한 예시를 갱신했다.
Colab 설정·폰트 반환값·소켓과 wire 필드·systemd 설정도 실제 코드에 맞췄다. 또한
옛 인증 캐시·평문 공개 포트·자기 신고 승패·기존 메뉴/설정 설명을 수정했다. 발췌 검사는 소스와의 일치를 확인하지만,
코드 예시의 모든 의미나 본문 문장의 정확성을 자동으로 증명하는 검사는 아니다.

아래 검증 이력의 “미구현/남음”은 **그 단계 당시 상태**다. PvP 입력 재현은 이번에 구현·검증했지만,
자동 플레이·담합·반복 보상 억제, 이탈 제재, 검증 비용을 포함한 ranked 부하 시험은 남는다.
Windows/macOS 실기기·설치 번들·최종 화면 육안 검수·실제 Colab 학습·브라우저 게임 포팅·
운영 서버 배포는 이번 실행에서 완료하지 않았다. 검증한 코드와 문서는 함께 버전 관리하며 실제 서비스 배포 여부와 구분한다.

재현 명령은 저장소 루트에서 실행한다. 전체 회귀용 reactor는 별도 터미널에서 먼저 띄운다.

```bash
./build-secure/tetris_relay_reactor --port 7788 --loops 1 --loopback-only
```

```bash
TETRIS_SECURE_BUILD="$PWD/build-secure" \
TETRIS_META_BIN="$PWD/build-secure/tetris_meta" \
TETRIS_RELAY_BIN="$PWD/build-secure/tetris_relay_reactor" \
TETRIS_RELAY_REACTOR_BIN="$PWD/build-secure/tetris_relay_reactor" \
uv run python -m pytest python/tests -q -ra
ctest --test-dir build-secure --output-on-failure
ctest --test-dir build-polish --output-on-failure
uv run python scripts/check_part_docs.py
```

빌드 폴더 이름은 로컬 검증용이다. 새 환경은 [실행 안내](start-here.md)의 구성·빌드부터
진행한다. WSS 검사에는 `TETRIS_BUILD_WSS=ON`, 단위 검사에는 `TETRIS_BUILD_TEST=ON`이 필요하다.

---

## 이전 검증 이력 — 계정 해시·폐기·복구

계정 구현의 의도와 실패 처리는 [Part 17](blog/part17-guest-account-recovery.md),
구조·SOLID·문서·보안·사용자 오인에 대한 재평가는 [품질 검토](architecture-quality-review.md)에 있다.
이 절은 이전 단계의 완료 로그를 2026-09-19에 확인한 기록이다. 그 뒤 같은 날 재실행한 최신 전체 검사 결과는 맨 위 절에 있다.

| 확인 | 결과 |
|---|---|
| 계정 변경 포함 Linux 전체 회귀 | **1,758 passed, 2 skipped**, 195.67초. `out/polish/account-full-tests.log` |
| 계정 보안 표적 검사 재실행 | **12 passed**, 10.68초. `out/polish/account-security-final.log` |
| WSS=ON Release 빌드와 CTest | 게임·meta·두 relay·gateway·probe 빌드 성공, **7/7 통과** |
| WSS=OFF Release 빌드와 CTest | 기존 개발 빌드 성공, **7/7 통과** |
| 파일 링크 | README와 docs의 코드 블록을 제외한 로컬 파일 링크 확인 |

새 표적 검사에는 손상된 옛 토큰을 발견했을 때 기존 행의 부분 해시화를 되돌리고
서버 시작을 거절하는 회귀를 추가했다. 이 검사는 위 전체 실행 후 추가되었으며 표적 검사에서
실행했다. 계정 검사에는 원문 미저장·이관 멱등성·이전 키와 입장권 폐기·경쟁 복구·키 충돌·
파일 잠금·저장 실패·응답 유실 후 재시도가 포함된다.

전체 실행의 skip은 torch·gymnasium 부재다. 실제 운영 DB를 이관하거나 서버를 배포하지
않았다. Windows/macOS 실행·새 계정 UI의 시각/사용성 검수·활성 경기 즉시 회수·PvP
결과 재현 검증은 완료한 것으로 표시하지 않는다. 현재 계정 변경은 로컬 작업 트리에 있다.

이하 WSS·봇 단계 수치는 이전 검증 이력이다.

## 후속 작업: WSS·일회용 입장권·Part 문서 정리

[Part 16](blog/part16-secure-admission.md)에 구현 의도와 코드 경로를 설명했다.
[Part 15](blog/part15-release-polishing.md)는 기존 시리즈의 구현 계약·번호별 설명·
현재 소스 발췌 형식으로 다시 작성했다. 단순 변경 목록 대신 표현 계층·봇 속도·
Colab·BP 검증의 소유권, 바꿔야 하는 파일과 이유를 연결했다.

이번 보안 변경:

- 네이티브 WSS와 TLS 게이트웨이, 일반 클라이언트의 원격 평문 HTTP 거절.
- HTTPS API에서 60초 일회용 게임 입장권 발급, relay secret으로 원자적 소비.
- relay의 장기 토큰 직접 입장 기본 거절, thread relay의 오프라인 인증 캐시 제거.
- 인증서 신뢰·호스트·만료 검증, Origin·프레임·크기·속도·연결 수 제한과 내부 loopback.
- Windows/macOS 시스템 CA 어댑터, TLS 런타임 패키징, WSS systemd 예제와 세 OS CI 구성.
- 통합 테스트에서 발견한 정상 WSS 연결 종료 대기 문제 수정: 소켓 취소만으로 남던
  idle 타이머 때문에 join이 멈추지 않도록 연결 전용 io_context를 정지한 뒤 join한다.

2026-09-11 Linux 로컬 결과:

| 확인 | 결과 |
|---|---|
| WSS=ON, BOT=OFF Release | 그래픽 게임·meta·thread/reactor relay·gateway·native probe 빌드 성공 |
| 전체 Python 회귀 | **1,747 passed, 2 skipped**, 198.50초. WSS 보안 검사 21개 포함 |
| WSS 기본 보안 모드 | 두 relay에서 정상 인증서·입장권·랜덤 매칭·룸·양방향 INPUT·실제 Session의 발급/나가기 확인 |
| 거절 경로 | 미신뢰 CA·호스트 불일치·만료, 원격 HTTP, 잘못된 secret/Origin/path/frame, 장기 토큰·입장권 재사용 |
| 수명과 자원 | 같은 티켓 경쟁 소비 1회, TTL·교체·4096 상한, 반복 WSS 접속 슬롯 반환 |
| CTest WSS=ON | **7/7 통과**: 기존 6개 + game_tickets |
| WSS=OFF 기존 개발 빌드 | 게임·두 relay·meta·테스트 Release 컴파일 성공, CTest **7/7 통과** |
| 결정론 덤프 | 기존 golden hash와 완전 일치 |
| 문서·배포 스크립트 | 28개 Markdown의 로컬 파일 링크와 shell 문법 검사 통과 |

전체 로그는 `out/polish/secure-full-tests.log`, 단위 검사는
`out/polish/secure-ctest.log`에 있다. 학습 라이브러리 `torch`, `gymnasium`이 없어
2개만 건너뛰었고, 서버 미기동이나 WSS 바이너리 누락으로 생긴 skip은 없다.
이 환경에서는 Boost 개발 헤더가 없어서 패키지를 `/tmp/tetris-boost`에 풀어
`TETRIS_BOOST_INCLUDE`로 지정했다. 시스템 패키지를 설치하거나 로컬 학습을 하지 않았다.
사용한 Boost는 1.90, OpenSSL은 3.5.5다.

전체 회귀의 기존 wire fixture는 `TETRIS_RELAY_LEGACY_AUTH=1`을 명시한다.
새 보안 검사 21개는 이 변수를 제거하고 실행한다. 호환 모드는 서비스 기본값이
아니며, legacy 성공을 일회용 입장권 검증의 근거로 사용하지 않는다.

Windows/macOS는 시스템 CA·빌드·CI 경로를 추가했지만 **실기기 또는 원격 CI 실행을
여기서 완료하지 않았다.** TLS 라이브러리 복사 로직의 추가와 배포 스크립트 문법 검사는
깨끗한 OS에서의 설치·실행·서명·공증 검증을 대신하지 않는다. 공개 인증서·DNS·
방화벽·서비스 배포도 수행하지 않았다. 이 단계 이후 계정 토큰 해시화/교체/복구를
아래와 별도로 검증했다. PvP 규칙 검증과 브라우저 게임 포팅은 남은 작업이다.

이하 수치는 앞선 단계의 검증 이력이며 위 최종 회귀와 중복된다.

## 후속 작업: 캐릭터 봇·Colab·공용 BP

[실행·수정 안내](bots-and-colab.md). `assets/opponents.cfg`, `bot::Controller`,
선택/결과 일러스트, 보드 위 아이콘 영역, Colab Drive 체크포인트·다중 캐릭터 ZIP,
ONNX 입출력 계약 검사, 선택형 서버 리플레이 검증과 공용 상점 BP를 추가했다.
기본 세 상대는 같은 휴리스틱의 속도 변형과 기존 임시 이미지다.

2026-09-11 Linux 로컬 검증:

| 확인 | 결과 |
|---|---|
| 전체 Python 회귀, reactor 대상 | **1,726 passed, 2 skipped**, 187.87초 |
| CTest: BOT=OFF, reactor=ON | **6/6 통과**: 기존 3개 + 컨트롤러·승리 재현·보상 DB |
| CTest: BOT=ON, reactor=OFF | **5/5 통과** |
| Linux ONNX Runtime 1.18.1 CPU 빌드 | 클라이언트·meta·계약 검사 실행 파일 빌드 성공 |
| ONNX 계약 검사 | 합성 40출력 모델 로드/합법 행동 추론 성공, 잘못된 41출력 모델 거절 |
| 보상 API | 실제 기록 승리 지급, 재시도 중복 방지, 소유권, 위조·너무 빠른 기록·발급 제한 확인 |
| 보상 DB | 동시 중복·하루 상한·RP/XP 불변·공용 아이콘 구매·구매 후 상한 유지 확인 |
| Colab 묶음 | 기본 3상대 ZIP 생성, 누락/외부 경로 거절 테스트, 일반 Python 셀 문법 검사 |

전체 로그는 `out/polish/bots-full-tests.log`. 이 로그의 2 skipped는 로컬에 없는
`torch`, `gymnasium` 의존 테스트다. 실제 Colab 실행·학습·체크포인트 export는
하지 않았다. ONNX 계약 검사의 모델은 학습 없이 만든 테스트 전용 상수 그래프이며,
플레이어용 모델로 설치하지 않았다. 최종 UI와 다른 OS 실기기 검증도 여전히 남는다.

재현은 아래 기존 전체 테스트 명령을 사용한다. ORT 검사만 따로 실행하려면:

```bash
uv run --no-project --with onnx python python/tests/make_onnx_fixtures.py /tmp/ort-check
./build-bot-polish/bot_onnx_contract_test /tmp/ort-check/valid.onnx /tmp/ort-check/invalid.onnx
```

아래는 앞서 수행한 빌드·문서·HTTP 방어 보강의 기록이다.

## 수정 범위

| 영역 | 변경 |
|---|---|
| 문서 입구 | `docs/start-here.md`, `customization.md`, `release-readiness.md`, 문서 허브·루트 README |
| Velog 원고 | Part 15 신규, Part 10의 HTTP 방어선/프록시 신뢰 발췌 교체, 관련 Part에 최신 경계 명시 |
| UI | `src/presentation.*`, `assets/theme.cfg`: 폰트·블록 RGBA, 비율 보존 아바타 프레임·대기 애니메이션 |
| 사용자 설정 | `UI animation`/`idle_animation` 저장. 상점 회전도 끄기. 폰트 실패 시 기본 폰트 재로드 |
| meta 방어 | 명시적 loopback XFF 신뢰, CF 헤더 무시, guest 별도 발급 예산, IP 표/작업 대기열 상한, no-store |
| 빌드 | Linux/Windows reactor와 macOS 기본 타깃 구분, CTest 등록, HTTPS 기본 URL/SSL 지원 불일치 구성 거절 |
| 배포 | release 옵션의 OFF/기본값까지 명시, macOS 호스트 아키텍처 기본, 서버 번들에 휴대형 백업 도구 포함 |
| 백업 | `scripts/backup_meta_db.py`: WAL online snapshot, 무결성 검사, 기존 출력 덮어쓰기 거절. bash 백업은 umask 077 |
| CI | Linux/Windows/macOS 클라이언트 컴파일 job, macOS portable 서버/코어 job 추가 |

게임 규칙·블록 ID·보드 크기·relay wire 메시지는 변경하지 않았다.
후속 봇 작업에서는 HTTP API와 `bot_rewards` DB 테이블을 추가했다.
새로운 학습이나 모델 export, 서버 배포, DNS·방화벽 변경은 수행하지 않았다.

## 실제 실행한 검사

이 작업 환경은 Linux, GCC 15.2, OpenSSL/SDL2/OpenGL 개발 라이브러리가 있는 환경이다.
빌드 폴더는 `build-polish`로 분리했다.

| 검사 | 결과 |
|---|---|
| 게임 + thread relay + reactor relay + meta + 기본 테스트 Release 빌드 | 통과 |
| CTest worker_group / loop_primitives / reactor | 3/3 통과 |
| sim_hash_dump와 기존 `_sim_hash_dump.txt` 비교 | 완전 일치. 표현 변경으로 결정론이 바뀌지 않음 |
| reactor 대상 전체 Python 검사, 7788 서버를 실제 시작하고 준비 확인 후 실행 | **1716 passed, 2 skipped**, 181.66초 |
| meta 보안 + online backup 추가 검사(전체 실행 이후 추가한 trusted-proxy 테스트 포함) | **18 passed**, 1.24초. 위 전체 결과와 중복 항목 있음 |
| thread relay의 meta 연동·결과 교차검증 | **17 passed, 15 skipped**, 8.87초. skip은 reactor 전용 검사 |
| reactor OFF 구성의 기본 테스트 | 빌드 및 CTest 2/2 통과(Linux에서 타깃 제외 경로 검증) |
| HTTPS API 기본값 + HTTPS 지원 OFF 구성 | 기대한 CMake 오류로 거절 확인 |
| shell 배포 스크립트와 CI bash 블록 | 문법 검사 통과 |
| Linux GUI 실행 | OpenGL 3.3 Core 컨텍스트 생성 확인, 정상 기동 후 테스트 프로세스 종료 |

전체 검사의 두 skip은 `torch` 없는 checkpoint roundtrip, `gymnasium` 없는 versus
환경 검사다. 서버가 꺼져 생기는 relay/room smoke skip은 최종 전체 실행에 없다.
이 환경에 대규모 학습 의존성을 추가하거나 모델을 재학습하지 않았다.

게스트 제한·캐시 금지 새 테스트는 이전 `build/` 바이너리에서 **2개 실패**하는 것을
먼저 확인했다. 새 `build-polish` 서버에서는 통과한다. 처음 전체 검사에서 기존
handshake 테스트가 24명의 guest를 한 번에 만들다가 429를 받았다. 그 테스트는
“이미 가입한 여러 사용자의 연결 슬롯 반환”을 검증하므로 private fixture DB에
사용자를 미리 준비하도록 수정했다. 운영 guest 발급 제한은 완화하지 않았다.

재현 명령(별도 터미널에서 reactor를 7788로 먼저 실행):

```bash
./build-polish/tetris_relay_reactor --port 7788 --loops 1
```

```bash
TETRIS_META_BIN="$PWD/build-polish/tetris_meta" \
TETRIS_RELAY_BIN="$PWD/build-polish/tetris_relay_reactor" \
TETRIS_RELAY_REACTOR_BIN="$PWD/build-polish/tetris_relay_reactor" \
uv run python -m pytest python/tests -q -ra
```

현재 로컬 결과 원문은 `out/polish/full-tests.log`와 `out/polish/thread-tests.log`에
있다. 생성 로그는 버전 관리하지 않는다. 실행 중인 테스트 서버를 종료한 뒤
다른 relay 구현의 smoke를 시작한다. 테스트 서버는 운영 서버와 별도로 띄운다.

## 검증하지 않은 것

- Windows 또는 macOS 실기기 실행·HTTPS 인증서 검증·설치 번들·서명/공증.
  CI 구성을 추가했지만 여기서 원격 CI를 실행하거나 통과했다고 주장하지 않는다.
- 인터넷 밖에서의 실제 접속, CGNAT/포트 전달, DNS 전환, Windows 서비스 등록.
- 최종 UI의 모든 화면·해상도·폰트 조합에 대한 육안 확인. GUI 기동은 확인했지만
  최종 화면 디자인 검수는 별도로 필요하다.
- Linux 다중 루프의 용량 재측정. 이번 로컬 전체 회귀는 기본 단일 루프 기준이다.
- 동시 수백/수천 명에 대한 무중단 운영, 침투 테스트 전체, 모든 의존성 CVE 감사.
- PvP 서버 규칙 검증과 활성 세션 회수. DB 해시 저장·키 교체·복구 부재는 Part 17에서 해결했다.
  게임 전송 보호는 후속 WSS·입장권 작업으로 추가했지만 이 항목들과는 별개다.

다음 우선순위는 [출시 점검](release-readiness.md)의 차단 항목이다.
