# 소규모 출시 점검과 서버 이전

2026-09-19 코드 검토. **Linux 주 서버(Mac 하드웨어), Windows 예비 서버,
Windows/macOS/Linux 클라이언트**가 목표다. 아래는 코드·로컬 검증 결과이며,
외부 인터넷 접속이나 Windows/macOS 실기기 검증을 완료했다는 뜻은 아니다.

## 출시 판단

네이티브 클라이언트의 비공개 테스트를 진행할 기반은 있다. 하지만 **웹 게임과
공개 ranked 서비스가 준비됐다고 보기는 어렵다.** WSS·일회용 입장권은 구현했으며,
실제 인증서·내부 포트 격리와 아래 남은 항목을 확인해야 한다.

| 우선순위 | 확인한 상태 | 출시 전 할 일 |
|---|---|---|
| 배포 확인 | 계정 토큰은 HTTPS API로 교환, 게임은 WSS + 60초 일회용 입장권. 기본 relay는 장기 토큰 거절 | [Part 16](blog/part16-secure-admission.md)대로 인증서·gateway·loopback relay 구성, 외부 7777·8080 차단과 실접속 확인 |
| 차단 | `web/ranking`은 랭킹만 제공. WASM/WebGL/브라우저 네트워크 빌드 없음 | 웹 게임을 별도 포팅 작업으로 취급. 아래 범위 참고 |
| 높음 | 두 relay에 서버 seed·입력 기반 종료 검증 구현 | 허위 신고는 차단하지만 일부러 지는 담합·자동 플레이·부계정 파밍은 별도 제한 필요. 시뮬레이션을 포함한 부하 재측정 |
| 배포 확인 | SQLite 목적별 해시 저장·기존 DB 이관·키 교체·복구 파일 구현 | [Part 17](blog/part17-guest-account-recovery.md)대로 점검 시간의 DB 이관과 실제 OS 복구 절차 검수. 시간 기반 만료·활성 경기 즉시 종료는 미구현 |
| 높음 | Windows는 IOCP 단일 루프로 전환 | 예비 장비에서 실제 대전·종료·부하 측정. Linux 용량 수치 재사용 금지 |
| 높음 | macOS 배포에 OpenSSL dylib 복사·참조 수정 추가 | 전이 dylib, CA 저장소, 서명/공증, Apple/Intel 아키텍처를 깨끗한 Mac에서 검증 |
| 보통 | 사용자/경기 데이터 자동 삭제·토큰별 삭제 요청 API 없음 | 보존 기간과 삭제·복구 안내, 운영자 처리 절차 마련 |
| 보통 | vendored SQLite/cpp-httplib/이미지·폰트 파서 사용 | 버전·라이선스 목록 및 보안 업데이트 절차 유지. 이번 작업은 종속성 CVE 전수 감사가 아님 |

## 이번에 보강한 것

- [Part 18](blog/part18-authoritative-results.md): PvP 서버 입력 검증, 결과 사유·연습 표시, 서버별 계정·미저장 경고, 엄격한 JSON 검증을 추가했다.

- WSS 클라이언트·TLS 게이트웨이·일회용 게임 입장권을 추가했다. 잘못된 인증서·Origin·프레임과 티켓 재사용을 거절한다. thread relay의 오프라인 인증 캐시는 제거했다. [Part 16](blog/part16-secure-admission.md)에 소유권·제한·실패 처리와 검증을 설명한다.

- 봇전은 선택적으로 서버 리플레이 검증 후 공용 BP를 지급한다. `--bot-rewards`로
  켜며, 10 BP/승리·100 BP/UTC일·티켓 중복 방지를 적용한다. PvP relay 검증과
  별도 경로다. [봇과 Colab](bots-and-colab.md)에 운영 제한과 남은 파밍 위험을 정리했다.

- `meta`의 forwarding header 신뢰를 기본 OFF로 변경했다. `CF-Connecting-IP`는
  신뢰하지 않는다. 같은 호스트 프록시의 XFF가 필요할 때만 `--trust-loopback-proxy`를
  명시하고, 마지막 XFF 주소만 사용한다. 프록시는 클라이언트가 넣은 헤더를 그대로
  전달하지 말고 실제 연결 주소를 append/overwrite해야 한다.
- 공개 요청 60회/초, relay secret이 맞는 요청 512회/초와 별도로 **게스트 발급
  10회/60초/IP** 제한을 추가했다. IP 버킷 표는 각각 최대 4096개, 만료 항목 회수,
  카운터 포화 처리로 메모리 증가와 카운터 넘침을 제한한다. 거절 응답에는 Retry-After가 있다.
- HTTP 작업자는 8개, 대기열은 128개로 제한했다. 읽기·쓰기 대기 각각 5초.
  이 값은 소규모 시작점이며 대규모 DDoS 방어나 전역 계정 수 제한은 아니다.
- 인증 응답을 포함하는 JSON 응답에 `Cache-Control: no-store`를 추가했다.
- release 스크립트가 캐시의 BOT/debug/net-trace/서버 주소를 우연히 재사용하지
  않도록 OFF/기본값까지 매번 넘긴다. macOS는 호스트 아키텍처가 기본이며,
  `ARCHS='arm64;x86_64'`는 모든 의존성을 universal로 준비했을 때만 사용한다.
- macOS 기본 클라이언트/테스트 빌드에 Linux epoll이 끼던 문제를 타깃 조건으로 고쳤다.
  Linux·Windows의 reactor 경로는 유지했다. macOS CI job도 추가했다.

프록시를 켜지 않으면 같은 프록시 뒤의 모든 사용자가 IP 제한을 공유한다.
PC방·학교·가족 네트워크도 하나의 NAT 주소를 공유할 수 있으므로 이 제한은
“한 사람당 10개”가 아니다. 다중 IP 공격에는 edge 제한·발급 총량/디스크 모니터가 필요하다.
기존 systemd meta 유닛은 신뢰 옵션을 자동으로 켜지 않는다.

## 로그인 없이 저장할 수 있나

**이미 가능하다.** 첫 실행 → `POST /v1/guest` → OS 난수 128비트 토큰 발급 →
사용자 폴더에 보관 → 다음 실행에서 같은 토큰 검증 → DB의 RP/XP/BP/아이콘 조회다.
가입 폼이 없을 뿐, 토큰을 열쇠로 쓰는 익명 계정이다. 최초 키 저장 실패는 화면에 표시하고 재시도 전 온라인 보상을 차단한다.

| OS | 현재 토큰 파일 |
|---|---|
| Windows | `%APPDATA%\Tetris\accounts\<origin locator>\account.json` |
| macOS | `~/Library/Application Support/Tetris/accounts/<origin locator>/account.json` |
| Linux | `${XDG_DATA_HOME:-$HOME/.local/share}/Tetris/accounts/<origin locator>/account.json` |

접근 키와 최신 복구 파일을 모두 잃으면 같은 사람임을 입증할 수 없다. 다른 PC와 자동 동기화되지 않는다.
지금 수동으로 토큰 파일을 옮기면 같은 기록을 쓸 수 있지만, 복사본을 가진 누구든
접근할 수 있고 동시 접속은 제한된다. 토큰은 비밀번호처럼 취급해야 한다.
계정은 정규화한 서버 origin별 폴더에 저장한다. 파일 안의 `api_url`도 확인해 다른 서버로
키를 자동 전송하지 않는다. 옛 `Tetris/token`은 Account 화면의 **Import older account**로
서버를 확인한 뒤 가져온다. 다른 서버의 새 계정은 **Create separate account**로 시작한다.
같은 서버에서 테스트 계정을 따로 쓰려면 절대 경로의 `TETRIS_USER_DATA_ROOT`를 지정한다.


가입 없는 게스트를 유지하면서 Account & Recovery에서 복구 파일을 만들 수 있다.
최신 복구 파일로 Restore하면 같은 기록에 새 접근 키·복구 코드를 발급한다. 사용한
복구 파일과 이전 접근 키는 다음 인증부터 거절된다. [Part 17](blog/part17-guest-account-recovery.md)의 절차를 따른다.
이메일·실명을 받지 않아도 IP 로그, 식별 토큰, 플레이 이력이 남는다.
“로그인이 없으니 개인정보가 전혀 없다”라고 안내하지 않는다. 수집 항목·목적·보존·삭제
안내는 실제 운영 국가와 배치에 맞게 별도로 확정해야 한다.

## 웹 게임으로 가려면

브라우저는 현재 raw TCP 소켓에 직접 접속하지 않는다. Emscripten도 기존 TCP를
WebSocket 프록시 등으로 바꾸는 과정을 설명한다.
[공식 Emscripten networking 문서](https://emscripten.org/docs/porting/networking.html).

필요한 범위는 WASM 빌드 + WebGL 호환 셰이더/렌더러 + 브라우저 메인 루프/입력/오디오
+ 비동기 HTTP + 브라우저 WSS 어댑터 + 브라우저 저장소·인증이다.
서버 WSS 게이트웨이는 이번에 추가했지만, 브라우저 게임 측 어댑터는 아직 없다.
데스크톱 OpenGL 3.3 셰이더와 블로킹 작업을 단순히 CMake 옵션 하나로 바꿀 수 없다.
정적 웹 페이지 배포와 실제 게임 포팅을 별도 완료 기준으로 관리한다.
공용 사이트의 장기 토큰은 URL에 넣지 않는다. TLS·세션 수명 설계 기준은
[OWASP Session Management](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html)을 참고한다.

## Linux에서 Windows로 서버 이전

실행 파일을 복사하는 것이 아니라 **Windows용 실행 파일 + 일관된 DB 스냅샷 +
같은 relay secret + DNS/방화벽/HTTPS 설정**을 준비하는 작업이다.

### 미리 준비

1. Windows에서 [Part 16의 WSS 서버 빌드](blog/part16-secure-admission.md)를 완료한다. Boost·OpenSSL·gateway도 필요하며 reactor는 `--loops 1`.
2. 같은 소스 버전의 meta를 사용한다. 사용자·아이콘·경기 스키마가 달라지지 않게 한다.
3. secret은 비공개 경로로 전달하고 Windows 전용 서비스 계정만 읽게 ACL을 설정한다.
4. 게임용 DNS 이름과 API용 DNS 이름을 사용한다. IP를 클라이언트에 박아 두면
   장비 이전 때 클라이언트를 다시 배포해야 할 수 있다.
5. Windows 방화벽, 공유기 포트 전달, HTTPS 인증서/프록시, 자동 시작을 실제로 확인한다.
   저장소의 systemd 유닛은 Windows에서 실행되지 않는다.

### DB 스냅샷 만들기

Python 표준 라이브러리만 사용하는 새 도구는 Linux/Windows에서 같은 명령을 쓴다:

```bash
python scripts/backup_meta_db.py /srv/tetris/db/tetris.db ./out/handover.db
```

실행 중 WAL의 커밋된 데이터도 online backup으로 읽고 무결성을 검사한다.
출력은 기존 파일을 덮어쓰지 않는다. 로컬 파일시스템의 같은 디렉터리에서 원자적으로
완성본을 내보낸다(하드 링크 지원 필요). 기존 Linux 압축/보존 도구
`scripts/backup_meta_db.sh`도 계속 사용할 수 있다.

### 실제 전환 순서

1. 기존 relay의 신규 입장을 막고 진행 경기를 마친다. relay를 정상 종료해 결과 전송을 끝낸다.
2. meta를 공개 프록시에서 분리해 아이콘 구매 등 쓰기를 멈춘 뒤 최종 스냅샷을 만든다.
   이후 원본 meta를 중지한다. **활성 writer는 한 곳만** 둔다.
3. 스냅샷을 Windows의 보호된 데이터 폴더(예: `C:\EntrisData\tetris.db`)에 복사한다.
4. Windows PowerShell 세 터미널에서 비밀키를 읽고 순서대로 시작한다:

```powershell
# 터미널 A
$env:TETRIS_RELAY_SECRET = (Get-Content C:\EntrisSecrets\relay-secret -Raw).Trim()
.\build-secure\Release\tetris_meta.exe --db C:\EntrisData\tetris.db --http 127.0.0.1:8080
```

```powershell
# 터미널 B
$env:TETRIS_RELAY_SECRET = (Get-Content C:\EntrisSecrets\relay-secret -Raw).Trim()
.\build-secure\Release\tetris_relay_reactor.exe --port 7777 --loops 1 --loopback-only --max-sessions-per-ip 128 --meta http://127.0.0.1:8080
```

```powershell
# 터미널 C: 실제 게임 도메인의 체인·키를 보호된 경로에 준비한다.
.\build-secure\Release\tetris_wss_gateway.exe --port 8443 --backend-port 7777 --cert C:\EntrisSecrets\tls\fullchain.pem --key C:\EntrisSecrets\tls\privkey.pem --origin https://game.example.com
```

5. 기존 사용자 토큰으로 같은 RP/아이콘이 보이는지, 두 사용자 대전 결과가 한 번만
   저장되는지 확인한다. 인증서·외부망 접속까지 성공한 뒤 DNS/포트 전달을 전환한다.
6. 실패하면 새 서버를 먼저 중지한다. 새 서버에서 쓰기가 발생했다면 최신 스냅샷을
   원래 서버로 역이전한 뒤 재개한다. 예전 DB를 무조건 다시 켜면 새 기록이 사라진다.

온라인 경기 자체는 이전되지 않는다. 중간 입력·연결은 메모리에 있고 프로세스 이동으로
복구되지 않는다. 계획된 이전은 점검 시간으로 안내한다. 원본 디스크가 고장난 상황에서는
마지막 백업 이후 데이터가 사라질 수 있으므로 별도 장비의 백업 복원 연습이 필요하다.

## 검증 근거와 남은 확인

검증 결과는 [이번 작업 기록](polish-validation.md)에 기록한다. Linux에서 테스트가
통과했다는 사실과 Windows/macOS에서 직접 실행했다는 사실은 구분한다.
외부 DNS, 공유기, TLS 종단, Windows 서비스 등록, 서명/공증은 이 저장소 수정만으로
검증할 수 없다. 이 작업에서 실서버 설정을 변경하거나 배포하지 않았다.
