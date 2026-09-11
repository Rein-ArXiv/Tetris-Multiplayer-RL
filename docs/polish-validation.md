# 2026-09-11 폴리싱 변경과 검증 기록

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
방화벽·서비스 배포도 수행하지 않았다. 계정 토큰 해시화/회전/복구, PvP 규칙 검증,
브라우저 게임 포팅은 남은 작업이다.

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
- 원문 DB 계정 토큰, 회전/폐기/복구 부재와 PvP 서버 규칙 검증의 해결.
  게임 전송 보호는 후속 WSS·입장권 작업으로 추가했지만 이 항목들과는 별개다.

다음 우선순위는 [출시 점검](release-readiness.md)의 차단 항목이다.
