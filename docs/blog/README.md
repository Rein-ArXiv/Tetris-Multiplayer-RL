# 제로부터 멀티플레이어 테트리스 + RL까지

이 시리즈의 목표는 두 가지다.

1. 빈 작업 디렉터리에서 공통 게임·네트워크 뼈대를 만든 뒤, 서비스 운영과 AI 경로를 각자의 의존 순서로 구현해 현재 저장소와 같은 기능 경계를 재현한다.
2. 완성형 게임 엔진(Unity·Unreal·Godot)이 대신 해주던 일들이 실제로 무엇인지 한 번씩 열어 본다. 창 생성부터 오디오 믹싱, lockstep 동기화, 신경망 추론까지다.

과거 작업 시간순이 아니라 **구현 의존성 순서**로 읽는다. 먼저 렌더링과 무관한 `SimGame`을 완성해 테스트 가능한 규칙 엔진을 만들고, 그 위에 플랫폼·렌더링· `Game`/`main.cpp`를 쌓는다. 이후에만 네트워크와 서버, Python/RL, 메타 서비스를 추가한다. 완성 상태의 모듈 관계는 루트 [`README.md`의 아키텍처](../../README.md#아키텍처)에서 짧게 복습할 수 있다.

## 코드 블록 읽는 법

모든 코드 블록에는 **바로 위 줄**에 세 라벨 중 하나가 붙어 있다. 순수 셸 명령 블록과 mermaid 다이어그램만 예외다.

| 라벨 | 형식 | 의미 |
|---|---|---|
| 현재 소스 발췌 | ``**현재 소스 발췌 — `경로`**`` | 현재 저장소에서 설명에 필요한 부분만 옮긴 코드. 경로와 함수·타입 이름으로 원문을 찾는다 |
| Part N 체크포인트 | ``**Part N 체크포인트 — `경로`**`` | 그 장까지 소개한 기능만 가진 **컴파일 가능한 중간 상태**. 최종 소스와 다르면 블록 바로 아래에 어느 Part에서 확장되는지 한 줄로 밝힌다 |
| 예시 | `**예시(실제 저장소에는 없음)**` | 설명용 의사코드·단순화 스니펫. 저장소에 대응 파일이 없다 |

`현재 소스 발췌`는 완성형의 계약을 보여 주고, `Part N 체크포인트`는 그 장까지의 동작만 하는 중간 형태를 보여 준다. 발췌는 생략된 문맥이 있을 수 있으므로 복사본이라기보다 설명의 기준으로 읽고, 실제 수정 때는 표시된 경로와 심볼을 확인한다.

주석은 그 장의 맥락에 맞게 다듬어 싣는다. 저장소 주석은 구현자가 빠르게 기억할 정보만 남기고, 배경·대안·실패 사례는 글의 본문이 맡는다. 따라서 문서의 주석과 소스 주석이 문장 단위로 같을 필요는 없지만, 코드의 계약과 동작은 같아야 한다.

`CMakeLists.txt`도 같은 규칙을 따른다. Part 0이 일곱 줄짜리 최소 뼈대를 세우고, **새 소스 파일을 추가하는 모든 장이 그 시점의 빌드 파일을 `Part N 체크포인트 — CMakeLists.txt` 로 다시 보여준다.** 각 장을 마칠 때마다 그 장의 타깃이 실제로 빌드되는 상태가 유지된다. 최종 `CMakeLists.txt` 전체 해부는 [Part 13](./part13-structure-and-build-reference.md)에 있으며, **다 만든 뒤에 보는 것**이다.

```mermaid
graph TB
    P0[Part 0<br/>빌드 뼈대] --> P1[Part 1<br/>결정론적 SimGame]
    P1 --> P2[Part 2<br/>플랫폼·창·입력<br/>체크포인트 데모]
    P2 --> P3[Part 3<br/>렌더링·UI<br/>체크포인트 데모]
    P3 --> P4[Part 4<br/>Game·main·60Hz 루프]
    P4 --> P5[Part 5<br/>오디오 + 설정 API]
    P4 --> P6[Part 6<br/>framing + 직결 lockstep]
    P6 --> P7[Part 7<br/>relay·room 확장]
    P1 --> P8[Part 8<br/>Python 바인딩·RL]
    P8 --> P9[Part 9<br/>ONNX 인프로세스 봇]
    P7 --> P10[Part 10<br/>메타 서버와 랭킹]
    P3 --> P11[Part 11<br/>설정과 옵션]
    P5 --> P11
    P10 --> P11
    P11 --> P12[Part 12<br/>검수와 배포]
    P12 -.-> P13[Part 13<br/>구조·확장 레퍼런스<br/>필요할 때 펼침]
```

## 읽는 순서

Part 0~7은 하나의 수직 슬라이스다. 규칙 엔진에서 화면·오디오·lockstep·relay까지 순서대로 쌓아야 각 완료 게이트가 의미를 가진다. 그 뒤에는 두 갈래가 독립적으로 열린다.

- **서비스·출시 경로:** Part 10 → Part 11 → Part 12. 계정·결과 영속화, 사용자 설정, 보안·백업·용량 검증 순서다.
- **AI 경로:** Part 8 → Part 9. 순수 `SimGame`을 Python 학습 환경으로 노출하고 같은 정책을 ONNX 인게임 봇으로 되돌린다.

두 경로를 모두 읽을 때 표의 Part 번호 순서는 학습 리듬을 위한 권장안이다. 제품 서버를 먼저 띄우려면 서비스 경로를 앞당겨도 AI 코드와 충돌하지 않고, 봇 연구가 목적이면 meta·배포를 건너뛰어도 된다.

| 순서 | 문서 | 완성되는 것 |
|---:|---|---|
| 0 | [Part 0: 준비물](./part0-project-setup.md) | OS별 툴체인 설치, 왜 엔진을 쓰지 않는가, 일곱 줄짜리 빌드 뼈대 |
| 1 | [Part 1: 결정론적 SimGame](./part1-deterministic-simulation.md) | 규칙, RNG, 가비지, 상태 해시와 headless 회귀 테스트(`sim_hash_dump`) |
| 2 | [Part 2: 플랫폼 계층](./part2-platform-window-input.md) | Win32/SDL2 창, OpenGL 3.3 Core 컨텍스트 생성, 시간과 입력. **독립 실행되는 체크포인트 데모** |
| 3 | [Part 3: 렌더링과 UI](./part3-rendering-and-ui.md) | GL 배칭 렌더러, 셰이더 SDF 둥근 사각형, 글리프 아틀라스 텍스트, 이미지, `gui_button`/`gui_checkbox`. **독립 실행되는 체크포인트 데모** |
| 4 | [Part 4: Game과 메인 루프](./part4-game-wrapper-and-loop.md) | `Game` 래퍼, `main.cpp`, 60Hz fixed-step, 메뉴 — 처음으로 `tetris` 가 빌드된다 |
| 5 | [Part 5: 오디오](./part5-audio.md) | MP3 decode, 이벤트 소비, XAudio2/SDL2 백엔드, on/off·볼륨 설정 API |
| 6 | [Part 6: Lockstep](./part6-lockstep-networking.md) | TCP framing 전체, HELLO/SEED/INPUT, 직결 P2P 세션과 해시 검증 |
| 7 | [Part 7: 릴레이 서버](./part7-relay-server.md) | 랜덤 큐, 커스텀 룸, transparent forwarding — 서버와 클라이언트 양쪽 |
| 8 | [Part 8: Python RL](./part8-python-rl.md) | pybind11 `tetris_py`, 관측/행동, Gym 환경과 체크포인트 |
| 9 | [Part 9: RL + ONNX 봇](./part9-rl-onnx-bot.md) | `.pt` 정책을 ONNX로 export하고 C++에서 실행 |
| 10 | [Part 10: 메타 서버와 랭킹](./part10-meta-and-ranking.md) | guest token, RP/XP/BP, 아이콘, ranked match 저장 |
| 11 | [Part 11: 설정과 옵션](./part11-settings-and-options.md) | 설정 영속화, `gui_slider`/`gui_value_selector`, 해상도·오디오·VSync와 결정성 경계 |
| 12 | [Part 12: 검수와 배포](./part12-hardening-and-release.md) | 보안 기본값, 리버스 프록시 배치, 패키징, 전체 회귀·통합 검증 |
| 13 | [Part 13: 구조와 확장 레퍼런스](./part13-structure-and-build-reference.md) | **순서대로 읽는 장이 아니다.** 완성 구조·`CMakeLists.txt` 전체 해부·플랫폼별 빌드, 그리고 "고치려면 어디를 건드리나" |

Part 0~4는 실행 가능한 싱글플레이 클라이언트를 만들고, Part 5는 표현 계층, Part 6~7은 같은 결정론 코어의 네트워크 경로를 완성한다. AI 경로는 `SimGame`만 의존하고, 서비스 경로는 relay와 UI 경계를 의존한다. 이 분기를 문서 순서와 실제 코드 의존성을 억지로 하나의 직선으로 만들지 않는 것이 핵심이다.

보안·단절·재시도 같은 횡단 관심사는 **그 상태를 소유한 글에 구현 원리**를 둔다. relay의 연결 제한과 기권 판정은 Part 7, DB 멱등성과 인증 경계는 Part 10이 설명한다. Part 12는 같은 내용을 다시 구현하지 않고 실제 배치, 백업, 장애 전환, 부하 측정과 릴리스 게이트로 묶는다. 독자는 기능을 만들 때 실패 계약을 함께 배우고, 운영 단계에서는 전체 시스템 관점으로 다시 검증할 수 있다.

### 헷갈리기 쉬운 경계

같은 주제가 두 장에 나뉘어 있는 곳이 몇 군데 있다. 어느 장에서 무엇이 만들어지는지 미리 알아 두면 "왜 여기에 이게 없지?" 를 피할 수 있다.

- **Part 2 / Part 3 은 각자 실행 가능한 산출물을 남긴다.** 이 시점에는 `tetris` 타깃을 빌드할 수 없다 — `src/game.cpp`, `net/*`, `bot/*` 등이 아직 없기 때문이다. 대신 두 장 모두 독자가 직접 만드는 **체크포인트 데모**를 제시하고, 그 데모를 그 시점의 `CMakeLists.txt` 에 `add_executable` 로 추가하는 방법까지 보여준다. Part 2 데모는 창·GL 컨텍스트 생성과 입력·dt 를, Part 3 데모는 도형·알파·텍스트·이미지·회전·view offset 을 한 화면에서 검증한다.
- **framing 은 Part 6 이 전부 다룬다.** 길이·타입·payload·체크섬 레이아웃과 `build_frame`/`parse_frames` 는 [Part 6](./part6-lockstep-networking.md) 소관이다. [Part 8](./part8-python-rl.md) 은 그 포맷을 Python 쪽에서 재현해 패리티를 검증하는 쪽만 다룬다.
- **Part 6 = 직결 P2P, Part 7 = 릴레이.** [Part 6](./part6-lockstep-networking.md) 은 두 클라이언트가 서로 직접 붙는 host/client 세션을 만든다. [Part 7](./part7-relay-server.md) 은 그 위에 `QUEUE_*`/`ROOM_*`/`MATCH_*` 메시지를 더해 릴레이 서버를 만들고, **같은 장에서 클라이언트 측 구현까지** 붙인다.
- **오디오 설정 API 는 Part 5, UI 는 Part 11.** `audio_set_music_enabled` / `audio_set_sfx_enabled` / `audio_set_music_volume` / `audio_set_sfx_volume` 는 [Part 5](./part5-audio.md) 에서 오디오 백엔드와 함께 만든다. [Part 11](./part11-settings-and-options.md) 은 거기에 설정 화면과 영속화를 붙일 뿐 새 오디오 API 를 만들지 않는다.
- **GUI 위젯은 사용처가 생기는 순서로 늘어난다.** 렌더러를 검증하는 `gui_button` / `gui_checkbox` 가 기본 상호작용을 만들고, 설정 화면에 필요한 `gui_slider` / `gui_value_selector` 가 같은 즉시 모드 계약을 확장한다. 완성형 선언은 `gui/gui.h` 에 모인다.

## 각 Part를 따라가는 방법

각 장에서 다음 네 항목을 확인한다.

1. **선행 상태**: 앞 장까지 존재해야 하는 파일과 공개 API.
2. **이번 Part의 파일**: 새로 만들거나 수정하는 실제 경로.
3. **연결점**: 새 코드가 이전 계층의 어느 API를 호출하는지.
4. **완료 게이트**: 빌드 명령, 자동 테스트, 화면 또는 로그의 기대 결과.

코드 조각만 복사하고 완료 게이트를 건너뛰면 뒤 장에서 원인을 찾기 어렵다. 특히 Part 1의 `sim_hash_dump`, Part 6의 framing/lockstep 테스트, Part 10의 meta/relay 통합 테스트는 이후 계층이 전제하는 계약이다.

### 빌드 규약 — 먼저 읽을 것

각 장의 명령을 그대로 쳤는데 configure 단계에서 죽는다면 대개 아래 다섯 가지 중 하나다.

**1. `TETRIS_BUILD_GAME` 은 기본 ON 이다.** 게임 클라이언트가 완성되기 전, 즉 **Part 0~3 에서는 `-DTETRIS_BUILD_GAME=OFF` 를 반드시 명시해야 한다.** 켜져 있으면 아직 만들지 않은 `src/game.cpp`, `net/*.cpp`, `renderer/*.cpp`, `bot/*.cpp`, `meta/http_client.cpp` 때문에 configure 가 실패한다. `third_party/httplib.h` 존재 검사에도 걸린다. 서버만 빌드할 때도 같은 이유로 OFF 가 필요하다.

```bash
# Part 1: 시뮬레이션만 있는 시점
cmake -S . -B build -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_TEST=ON
cmake --build build --target sim_hash_dump
./build/sim_hash_dump | diff - python/tests/_sim_hash_dump.txt && echo "결정론 OK"
```

**2. 출력 경로가 생성기마다 다르다.** 단일 구성 생성기(Makefiles/Ninja — Linux/macOS 기본)에서는 `--config` 가 무시되고 산출물은 `build/tetris` 다. multi-config 생성기(Visual Studio)에서는 `--config Release` 가 필요하고 산출물은 `build/Release/tetris.exe` 다. **두 경로를 섞어 쓰지 말 것.**

```bash
# Linux/macOS — SDL2 백엔드가 기본
cmake -S . -B build -DTETRIS_USE_SDL2=ON
cmake --build build --target tetris
./build/tetris

# Windows — Win32/XAudio2 handmade 백엔드가 기본
cmake -S . -B build -DTETRIS_USE_SDL2=OFF
cmake --build build --config Release --target tetris
./build/Release/tetris.exe
```

**3. `--target tetris` 는 `copy_assets` 를 돌리지 않는다.** `copy_assets` 는 ALL 타깃이라 타깃을 지정하면 건너뛴다. 빌드 디렉터리에서 실행하면 `Font/`·`Sounds/` 가 없어 폰트와 소리가 빠진다. 저장소 루트에서 실행하거나, 타깃을 지정하지 않고 `cmake --build build` 로 빌드한다.

**4. relay/room smoke 테스트는 포트 7788 고정이다.** `python/tests/test_relay_smoke.py` 와 `test_room_smoke.py` 는 `RELAY_PORT = 7788` 을 하드코딩한다. 기본 7777 로 띄우면 테스트가 실패하지 않고 **조용히 skip** 된다. `-q` 출력의 통과 개수를 반드시 확인한다.

```bash
cmake -S . -B build -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_RELAY=ON
cmake --build build --target tetris_relay
./build/tetris_relay --port 7788 &
sleep 1
uv run python -m pytest python/tests/test_relay_smoke.py \
                       python/tests/test_room_smoke.py -q
kill %1
```

**5. 디버그 단축키는 전용 빌드에만 있다.** 해시 덤프 `H` 키, 봇 속도 조절 같은 디버그 입력은 `-DTETRIS_ENABLE_DEBUG_UI=ON` 으로 빌드해야 살아난다(기본 OFF). 문서에서 디버그 단축키를 안내하는 곳은 전부 이 빌드를 전제한다.

torch 없이 도는 Python 테스트는 다음 세 개이며 저장소 루트에서 실행한다.

```bash
uv sync --dev
uv run python -m pytest python/tests/test_framing_parity.py \
                       python/tests/test_checkpoint_roundtrip.py \
                       python/tests/test_training_scripts_static.py -q
```

전체 회귀 절차(빌드 → 결정론 → 워커 → pytest → smoke → 스크립트 검사)는 [Part 12](./part12-hardening-and-release.md) 가 한곳에 모아 둔다.

## 현재 운영 모델

현재 구조는 세 실행 환경을 분리한다.

```mermaid
graph TB
    subgraph User["유저/배포 머신"]
        G[tetris<br/>C++ game]
        ORT[ONNX Runtime<br/>model/bots/*.onnx]
    end

    subgraph Server["운영 서버"]
        R[tetris_relay<br/>TCP 7777]
        M[tetris_meta<br/>HTTP 127.0.0.1:8080]
        DB[(SQLite)]
    end

    subgraph Colab["Colab 학습 환경"]
        PY[tetris_py + PyTorch]
        CKPT[.pt checkpoint]
        ONNX[exported .onnx]
    end

    G --> R
    G --> M
    R -- X-Relay-Secret --> M
    M --> DB
    PY --> CKPT --> ONNX --> ORT
```

- 유저 머신은 내장 휴리스틱 봇을 항상 사용할 수 있고, 학습 모델은 `model/bots/*.onnx`와 ONNX Runtime만 있으면 인게임 봇 로스터에 나타난다.
- PyTorch는 학습과 `.pt -> .onnx` export에만 필요하다.
- Mac mini 같은 약한 배포 머신에서는 torch를 설치하지 않아도 된다.
- `tetris_meta`는 기본적으로 relay secret 없이 시작하지 않는다.
- `tetris_meta`는 `127.0.0.1` 에만 bind하고 앞단의 리버스 프록시가 TLS를 종단한다 — 랭킹 페이지의 same-origin `/v1/` fetch가 성립하는 이유다 ([Part 12](./part12-hardening-and-release.md)).
- 로컬 검수는 C++ 빌드, 결정론 덤프, framing/placement 테스트, meta/relay smoke까지다. 학습은 하지 않는다.
- 현재 UI는 RP(0 시작/0 바닥), 누적 XP 레벨, BP 아이콘 상점을 사용한다.
- `python/netbot/`은 framing/input 패리티와 ONNX export 도구다. 실제 봇 실행 경로는 인프로세스 ONNX/휴리스틱 봇이다.
- `python/common/env_versus.py`는 가비지 교환형 2-보드 RL 환경을 제공하지만, 기본 trainer CLI는 아직 단일 보드 환경을 직접 생성한다 ([Part 8](./part8-python-rl.md) 참조).
