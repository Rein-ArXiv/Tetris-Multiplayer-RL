# 다시 시작하는 실행 안내

> 캐릭터별 모델·속도·일러스트와 서버 검증 BP는 [봇과 Colab 안내](bots-and-colab.md)를 먼저 보세요.

2026-09-19 코드 기준. **주 서버는 Mac 하드웨어에 설치한 Linux, 예비 서버는 Windows**다.
macOS는 별도 클라이언트 배포 대상이다. 기기 브랜드가 아니라 설치된 OS로 명령을 고른다.

처음부터 구현 과정을 복습하지 않아도 된다. 아래대로 게임을 켜 본 뒤,
[외형 수정](customization.md) → [출시 점검·Windows 이전](release-readiness.md)을 읽는다.
Velog용 [Part 15](blog/part15-release-polishing.md)는 표현·봇·보상,
[Part 16](blog/part16-secure-admission.md)은 WSS·입장권의 구현 의도와 배포 순서를 설명한다.

## 무엇을 켜야 하나

| 하고 싶은 일 | 실행하는 프로그램 | 필요 없는 것 |
|---|---|---|
| 혼자 플레이 / 내장 봇과 대전 | `tetris` | 서버, Python, PyTorch, 학습 모델 |
| 기록 없는 온라인 대전 | `tetris_relay_reactor` → 각 PC의 `tetris` | meta, DB, 학습 |
| 기록·RP·아이콘 상점 사용 | 로컬: meta → relay → 게임. 공개: API HTTPS + meta → 내부 relay → WSS gateway → 게임 | 가입 화면, PyTorch |
| 웹 랭킹 보기 | meta + HTTPS 프록시 + `web/ranking/index.html` | 게임 클라이언트 |
| 웹주소로 게임 플레이 | **아직 미구현** | 기존 실행 파일을 웹에 올리는 것만으로 되지 않음 |

`meta`는 기록 담당, `relay`는 연결·매칭 담당, `tetris`는 화면과 게임 규칙 담당이다.
랭크 PvP relay는 입력으로 보드 종료를 검증하고 meta가 기록한다. 봇 BP는 별도로 meta가 보드를 재현한다.

## 1. 게임부터 켜기

모든 명령은 저장소 루트에서 실행한다. 아래 빌드 폴더는 용도별로 분리해
이전에 켜 둔 CMake 옵션이 다음 빌드에 섞이지 않게 한다.

Ubuntu/Debian Linux 준비물:

```bash
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev libssl-dev
cmake -S . -B build-client -DCMAKE_BUILD_TYPE=Release \
  -DTETRIS_BUILD_GAME=ON -DTETRIS_BUILD_RELAY=OFF -DTETRIS_BUILD_META=OFF \
  -DTETRIS_BUILD_PY=OFF -DTETRIS_BUILD_BOT=OFF
cmake --build build-client --parallel 4
./build-client/tetris
```

Windows는 Visual Studio의 **C++ 데스크톱 개발**과 CMake를 설치하고 PowerShell에서:

```powershell
cmake -S . -B build-client -DTETRIS_BUILD_GAME=ON -DTETRIS_BUILD_RELAY=OFF -DTETRIS_BUILD_META=OFF -DTETRIS_BUILD_PY=OFF -DTETRIS_BUILD_BOT=OFF
cmake --build build-client --config Release --parallel 4
.\build-client\Release\tetris.exe
```

Windows 기본 창/음향은 Win32/XAudio2라 SDL2 설치가 필요 없다. 공개 HTTPS API에
접속할 클라이언트는 OpenSSL 개발 라이브러리와 실행 DLL도 준비해야 한다. meta 서버를 빌드할 때에는 HTTPS 사용 여부와 관계없이 계정 해시를 위한 OpenSSL Crypto가 필수다.
구성 로그에 `OpenSSL found: HTTPS meta client enabled`가 있는지 확인한다.

macOS는 Xcode command-line tools, CMake, SDL2, OpenSSL을 준비한다:

```bash
brew install cmake sdl2 openssl@3
cmake -S . -B build-client -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  -DTETRIS_BUILD_GAME=ON -DTETRIS_BUILD_RELAY=OFF -DTETRIS_BUILD_META=OFF \
  -DTETRIS_BUILD_PY=OFF -DTETRIS_BUILD_BOT=OFF
cmake --build build-client --parallel 4
./build-client/tetris
```

Apple/Intel용 라이브러리 아키텍처는 실행 파일과 같아야 한다.
macOS에서는 reactor 타깃을 기본 제외한다. 클라이언트와 기본 단위 테스트는 빌드할 수 있다.

메뉴에서 `Single Play` 또는 `Single vs Bot`을 선택한다. 파일을 찾는 기준이 실행
작업 폴더이므로 개발 중에는 위처럼 **저장소 루트에서 실행**한다. `--target tetris`만
빌드하면 별도 `copy_assets` 타깃은 실행되지 않는다. 일반 전체 빌드에는 포함된다.

## 2. 서버 빌드 — Linux 주 서버 / Windows 예비 서버 공통

기본 서버에는 C++ 툴체인·CMake·OpenSSL 개발 라이브러리가 필요하며 SDL2·OpenGL·폰트·사운드는 필요 없다. WSS를 켜면 Boost 헤더도 필요하다. ONNX 봇의 BP 검증을 켤 때에는 같은 모델과 OS별 ONNX Runtime을 추가한다.

```bash
cmake -S . -B build-server -DCMAKE_BUILD_TYPE=Release \
  -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON -DTETRIS_BUILD_META=ON \
  -DTETRIS_BUILD_REACTOR=ON -DTETRIS_BUILD_TEST=ON -DTETRIS_BUILD_PY=OFF
cmake --build build-server --config Release --parallel 4
ctest --test-dir build-server -C Release --output-on-failure
```

PowerShell에서는 명령을 한 줄로 합친다(`\` 줄 이음은 bash 문법).
Windows 실행 파일은 `build-server\Release\*.exe`, Linux는 `build-server/*`다.
`--loops 1`부터 시작한다. Linux의 `--loops 4`는 매치 포워딩을 분산하고,
Windows IOCP는 소켓 이관을 지원하지 않아 단일 루프로 전환된다.

## 3. 먼저 기록 없는 로컬 대전

터미널 A:

```bash
./build-server/tetris_relay_reactor --port 7777 --loops 1
```

터미널 B, C 각각:

```bash
./build-client/tetris --relay 127.0.0.1:7777
```

두 창에서 같은 온라인 큐 또는 같은 커스텀 룸을 이용한다. LAN의 다른 PC에서
붙을 때는 `127.0.0.1` 대신 **서버의 LAN IPv4 주소**를 넣는다. 서버 방화벽은
테스트할 네트워크에 한해 TCP 7777을 허용한다. 서버는 IPv4 전체 인터페이스에서 듣는다.

## 4. 기록도 붙이기 — 로컬 검증 전용

Linux 터미널 A에서 비밀키 생성 및 meta 시작:

```bash
mkdir -p "$HOME/.config/entris" "$HOME/.local/share/entris-server"
umask 077
openssl rand -hex 32 > "$HOME/.config/entris/relay-secret"
export TETRIS_RELAY_SECRET="$(cat "$HOME/.config/entris/relay-secret")"
./build-server/tetris_meta --db "$HOME/.local/share/entris-server/tetris.db" --http 127.0.0.1:8080
```

이 비밀키 생성은 **최초 한 번**만 한다. 다음 실행은 저장된 값을 읽는다.
터미널 B에서 같은 파일을 읽고 relay 시작:

```bash
export TETRIS_RELAY_SECRET="$(cat "$HOME/.config/entris/relay-secret")"
./build-server/tetris_relay_reactor --port 7777 --loops 1 --meta http://127.0.0.1:8080
```

터미널 C에서 클라이언트:

```bash
./build-client/tetris --relay 127.0.0.1:7777 --meta http://127.0.0.1:8080
```

다른 사용자 데이터 폴더를 쓰는 두 클라이언트로 검증한다. 같은 OS 사용자로
두 번 실행하면 같은 토큰을 읽어서 중복 ranked 입장이 거절될 수 있다.
세 OS 모두 `TETRIS_USER_DATA_ROOT`에 절대 경로를 지정하면 계정과 설정을 분리한다. Linux/macOS 예시:

```bash
TETRIS_USER_DATA_ROOT="$PWD/out/test-player-b" ./build-client/tetris --relay 127.0.0.1:7777 --meta http://127.0.0.1:8080
```

끄는 순서는 클라이언트 → relay → meta다. 로컬에서도 계정 토큰 대신 일회용 입장권을
교환하지만, 이 평문 TCP 예제는 같은 PC 검증용이다. **공개 접속은 WSS**로 구성한다.
Boost·OpenSSL 준비, `TETRIS_BUILD_WSS=ON` 빌드, 인증서, gateway 시작, 두 플레이어
접속 명령은 [Part 16 §6](blog/part16-secure-admission.md#6-빌드와-실행--무엇부터-켜는가)에 있다.
공개 운영은 [배포 절차](public-server-deployment.md)를 따른다.

## 5. 실행이 안 될 때

| 증상 | 확인할 것 |
|---|---|
| 폰트·아이콘·소리가 없음 | 저장소 루트에서 실행했는지, 배포 폴더에 Font/Sounds/assets가 있는지 |
| WSS rebuild 안내 / 입장 실패 | `TETRIS_BUILD_WSS=ON`, `wss://도메인:포트/play`, 같은 버전 meta·relay, 인증서 및 relay secret 확인 |
| HTTPS rejected | OpenSSL이 발견된 빌드인지, 실행 DLL/dylib와 CA 인증서 설정이 맞는지 |
| `Connection refused` | relay 프로세스, 서버 주소, 방화벽, 포트 충돌 |
| LAN은 되지만 외부 접속 실패 | 공유기 포트 전달, 공인 IPv4/CGNAT, 외부망에서 실제 접속. DNS만으로 포트는 열리지 않음 |
| 중복 플레이어 거절 | 같은 사용자 폴더의 게스트 토큰을 두 창이 공유 중인지 |
| meta 시작 거절 | `TETRIS_RELAY_SECRET`이 설정됐는지. 공개 운영에 `--allow-public-matches` 사용 금지 |
| Mac에서 epoll 헤더 오류 | 업데이트된 CMake 사용, 오래된 캐시의 `TETRIS_BUILD_REACTOR=OFF` 확인 |
| smoke 테스트가 전부 skip | 테스트 포트 7788의 서버와 바이너리 환경변수가 실제로 준비됐는지 |

상세 패키징은 [Part 12](blog/part12-hardening-and-release.md), 출시 조건과 서버 이전은
[release-readiness.md](release-readiness.md)를 기준으로 한다.

## 가입 없이 기록 보관·복구하기

온라인 계정이 생성된 뒤 메뉴의 **Account & Recovery → Save recovery file**을 실행한다.
화면에 표시되는 폴더의 `account-recovery.json`을 별도 위치에 복사해 둔다. 다른 기기에서는
같은 서버로 실행하고 해당 폴더에 복구 파일을 놓은 뒤 **Restore recovery file**을
선택한다. BP·RP·아이콘은 유지하고 접근 키와 복구 파일을 새로 만든다.

키가 유출됐거나 다른 기기의 접근을 끊으려면 **Replace access keys**를 사용한다.
교체·복구 후에는 항상 새 복구 파일을 보관한다. 통신이나 저장 실패가 표시되면
**Retry / reconnect**로 마무리하며, 대기 파일을 지우고 새 교체를 시작하지 않는다.
이미 진행 중인 경기를 즉시 강제 종료하는 기능은 아직 없다.

서버별 폴더에 계정을 나누어 저장한다. 이전 버전의 계정은 **Import older account**에서
현재 서버를 확인한 뒤 가져온다. 최초 저장 실패 경고가 나오면 게임을 닫기 전에 **Retry / reconnect**로
저장을 완료한다. 같은 키를 다시 저장하며 온라인 보상은 저장 성공 후에 가능하다.

잘못된 키·손상된 파일을 발견하면 새 계정을 자동 생성하지 않고 복구를 안내한다.
키와 복구 파일을 모두 잃었을 때는 익명 계정을 확인할 별도 수단이 없다.
파일 경로·OS별 보호·서버 이관·실패 처리의 이유는 [Part 17](blog/part17-guest-account-recovery.md)에 있다.

랭크 결과가 미확인이면 재접속해 프로필을 확인한다. 연습전은 RP/BP를 주지 않는다.
재경기는 큐로 돌아가 새 연결로 시작한다. [Part 18](blog/part18-authoritative-results.md)에 판정 기준이 있다.
