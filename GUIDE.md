# 코드 이해 가이드 — 어디서부터 읽을 것인가

> ARCHITECTURE.md는 완성된 레퍼런스. 이 파일은 **처음 읽는 순서와 핵심 개념** 안내서.

---

## 먼저 알아야 할 3가지 핵심 개념

이 3가지를 이해하면 나머지 코드가 모두 "그래서 이게 필요했구나"로 연결됩니다.

### 1. SimGame이 모든 것의 중심이다

```
SimGame (src/sim_game.h)
  ↑ 감싼다 (Draw 추가)
Game (src/game.h)
  ↑ 사용한다
main.cpp

SimGame
  ↑ pybind11로 노출
Python (학습 / 봇)
```

- **SimGame**: 순수 게임 로직만. 렌더링 없음, 오디오 없음, 화면 없음.
- **Game**: SimGame을 감싸서 "그림 그리기"만 추가 (`renderer/*.cpp` OpenGL 2D 사용).
- **main.cpp**: Game을 들고 60Hz로 루프 돌림.

> 이 프로젝트는 raylib을 링크하지 않습니다. Windows는 핸드메이드 Win32+OpenGL
> (`platform/win32.cpp`), macOS/Linux 는 SDL2+OpenGL (`platform/sdl.cpp`)을
> 직접 구현해서 사용합니다. 자세한 과정은 `docs/blog/part0` ~ `part9` 참조.

> _"게임 로직을 고치려면 SimGame을 고쳐라. 화면을 고치려면 Game을 고쳐라."_

---

### 2. 60Hz 고정 틱 루프

```cpp
float deltaTime = platform_begin_frame();
AccumulateInput(chatComposing);              // 프레임마다 edge 입력 누적
accumulator += deltaTime;
while (accumulator >= SECONDS_PER_TICK) {    // 1/60초마다
    uint8_t mask = ConsumeInput(chatComposing);
    game->SubmitInput(mask);                 // 입력 등록
    game->Tick();                            // 게임 한 틱 진행
    accumulator -= SECONDS_PER_TICK;
}
renderer_begin(...);                         // 그리기는 틱과 무관하게 매 프레임
renderer_end();
```

핵심: **입력과 게임 진행(Tick)은 항상 1/60초 단위로 묶임**.  
화면은 GPU가 허락하는 만큼 빠르게 그리지만, 게임 상태는 60Hz로만 바뀜.

---

### 3. 멀티플레이 = "같은 것을 두 번 돌린다"

```
내 키보드 입력  → localInputs[tick]  → gameLocal->Tick()
상대방 입력     → remoteInputs[tick] → gameRemote->Tick()
```

- 두 플레이어가 **같은 시드(seed)**로 게임을 시작
- 매 틱 각자의 입력을 TCP로 교환
- 양측 모두 `gameLocal`과 `gameRemote`를 각자 시뮬레이션

네트워크로 "현재 상태"를 보내지 않음. 입력만 교환.  
같은 시드 + 같은 입력 순서 → 결과가 항상 같음 (결정론).

---

### 4. 메타는 별도 프로세스다

```
tetris (game)  ──HTTP──>  tetris_meta  (DB + ELO)
tetris_relay   ──HTTP──>  tetris_meta
```

- `tetris_meta` 는 SQLite 파일과 4개 HTTP endpoint 만 가진 독립 서비스.
  보통 항상 켜둘 수 있는 호스트 (Mac mini 등) 에 띄움.
- `game` / `relay` 는 **단방향**으로 meta 만 호출 — meta 는 절대 역방향 호출 안 함.
- 의존 방향이 단방향이라 meta 를 다른 게임으로 갈아끼우거나 (의존부만 바꾸면)
  meta 없이 unranked 모드로 굴려도 게임 자체는 그대로 동작.

> _"게임 로직 + 매치메이킹은 meta 없이도 돌고, meta 는 게임 코드 없이도 돈다."_

---

## 읽는 순서 (권장)

### Step 1 — 가장 작은 조각부터 (15분)

| 파일 | 읽을 것 | 왜 |
|---|---|---|
| `core/input.h` | 전체 (10줄) | INPUT_LEFT 등 비트마스크 정의. 이게 "입력"의 전부 |
| `core/constants.h` | 전체 (5줄) | TICKS_PER_SECOND=60. 이 숫자 하나가 게임 속도 |
| `core/rng.h` | 전체 (30줄) | 블록 순서를 결정하는 난수기. 시드가 같으면 순서가 같음 |

---

### Step 2 — 게임 데이터 구조 (20분)

| 파일 | 읽을 것 | 왜 |
|---|---|---|
| `src/sim_block.h` | 전체 | 테트로미노 하나. id, rotationState, rowOffset, columnOffset |
| `src/sim_grid.h` | 전체 | 20×10 보드. `grid[row][col]`에 블록 id 저장 (0=빈칸) |
| `src/sim_blocks.h` | 구조만 훑기 | L/J/I/O/S/T/Z 7종의 셀 좌표 정의 |

---

### Step 3 — 게임 로직 핵심 (30분)

**`src/sim_game.h`** 전체 읽기 — 구현 말고 **인터페이스만**.

주목할 것:
```cpp
void SubmitInput(uint8_t inputMask);  // 이 틱에 어떤 키를 눌렀는가
void Tick();                           // 게임을 1/60초 앞으로 진행
uint64_t StateHash() const;           // 현재 보드 상태의 지문 (체크섬)
```

**`src/sim_game.cpp`** — 다음 함수만 순서대로:
1. `SimGame::Tick()` — 매 틱 무슨 일이 일어나는지
2. `SimGame::LockBlock()` — 블록이 바닥에 닿으면, 라인/T-spin/가비지를 어떻게 처리하는지
3. `SimGame::GetRandomBlock()` — 다음 블록 뽑기 (RNG 호출)
4. `SimGame::StateHash()` — 상태를 숫자로 압축하는 방법

---

### Step 4 — 메인 루프 (30분)

**`src/main.cpp`** — 전체를 읽되 다음 순서로:

1. `AccumulateInput()` / `ConsumeInput()` — 키보드 → 비트마스크, 좌우 DAS/ARR 반복
2. `AppMode` enum — Title / Single / Net / Room 흐름
3. CLI 파서 — `--host`, `--connect`, `--queue`, `--relay`
4. `main()` 안의 fixed-step 루프 — 60Hz 게임 진행
5. Net 모드 분기 — `startDelay`, `inputDelay`, `safeTick` 기반 lockstep

---

### Step 5 — 네트워킹 (45분)

순서가 중요합니다.

#### 5-1. 프레임 포맷 이해
**`net/framing.h`** 전체 — 메시지 구조:
```
[LEN: 2바이트][TYPE: 1바이트][PAYLOAD: LEN-1바이트][CHECKSUM: 4바이트]
```
TCP는 스트림이라 메시지 경계가 없음 → 이 포맷으로 경계 표시.

**`net/framing.cpp`** — `build_frame()`, `parse_frames()` 두 함수만.

#### 5-2. 세션 핸드셰이크
**`net/session.h`** 전체 훑기 — `SeedParams`, `GameOverChoice`, 공개 메서드 목록.

**`net/session.cpp`** — 다음 순서:
1. `Connect()` — 클라이언트가 연결을 시작할 때
2. `handleFrame()` — 메시지 수신 dispatch
3. `SendInput()` — 입력 전송
4. `ioThread()` — 백그라운드 송수신 스레드

#### 5-3. 핵심 질문: "언제 시뮬을 진행해도 안전한가?"
```cpp
int64_t safeTickInclusive = min(lastLocalSent, lastRemote) - inputDelay;
```
내가 보낸 마지막 틱과 상대가 보낸 마지막 틱 중 작은 것 — inputDelay.  
그 틱까지는 양쪽 입력이 모두 도착했음이 보장됨.

---

### Step 6 — 메타/랭킹 (선택 — ranked 매치 흐름 이해용, 30분)

게임 로직과는 분리된 영속 레이어입니다. unranked 모드에서는 통째로 우회 가능.

| 파일 | 읽을 것 | 왜 |
|---|---|---|
| `meta/database.cpp` | `kSchema`, `registerGuest`, `saveMatch` | SQLite 래퍼. mutex 로 모든 호출 전직렬화 + `BEGIN IMMEDIATE` 트랜잭션 |
| `meta/api_server.cpp` | `listen()` 안의 4개 라우터 | cpp-httplib 라우터 — `/v1/{guest,auth/verify,matches,leaderboard}` |
| `meta/http_client.cpp` | `MetaClient::*`, `token_file_path()` | relay/client 공용 wrapper. 플랫폼별 토큰 저장 경로(APPDATA / Library / XDG) |
| `server/relay.cpp::forwarderLoop` | selective passthrough 분기 전체 | ranked + meta 연결 시에만 `MATCH_SUMMARY` 를 가로채고, 나머지 프레임은 투명 통과 |
| `server/relay.cpp::finalizeRanked` | 전체 | 양쪽 `MATCH_SUMMARY` 모두 모이면 교차검증 → `post_match` → 양쪽에 `MATCH_RESULT` |
| `src/main.cpp` 의 `bootstrap_new_guest` 람다 | 전체 | 첫 실행 시 토큰 발급, 이후엔 verify → 404 면 자동 재발급 |
| `src/main.cpp` 의 `MATCH_RESULT` 폴링 | 전체 | 게임오버 화면에 ELO 변화 표시 |

---

## 핵심 데이터 흐름 요약

### 1인 모드

```
키 입력
  → AccumulateInput()/ConsumeInput() → uint8_t mask
  → game->SubmitInput(mask)
  → game->Tick()           ← SimGame 내부: 중력, 이동, 블록 잠금, 라인 클리어
  → renderer_begin/end      ← Game::Draw() → OpenGL 2D 렌더러로 화면 그림
```

### 멀티 모드

```
내 키 입력 ──→ localInputs[tick] ──→ session.SendInput() ──→ TCP ──→ 상대방
                                                                        ↓
상대방 입력 ←── session.GetRemoteInput() ←── TCP ←─────────────────────

양측이 모은 다음:
  gameLocal->SubmitInput(내 입력);   gameLocal->Tick();
  gameRemote->SubmitInput(상대 입력); gameRemote->Tick();
```

### 블록 하나가 떨어지는 과정

```
SimGame::Tick()
  → gravityCounterTicks++
  → gravityCounterTicks >= dropIntervalTicks?
      → MoveBlockDown()
          → IsBlockOutside() or !BlockFits()?
              → LockBlock()           ← 보드에 고정
                  → sim_grid.ClearFullRows()   ← 라인 클리어
                  → UpdateScore()
                  → GetRandomBlock()           ← RNG 호출
                  → currentBlock = nextBlock
                  → nextBlock = 새 블록
```

---

## 자주 헷갈리는 것들

### Q. Game과 SimGame의 차이?

| | SimGame | Game |
|---|---|---|
| 렌더링 의존성 | 없음 | 있음 (OpenGL 2D via `renderer/`) |
| 오디오 | 없음 | 있음 |
| 그림 | 없음 | 있음 |
| 게임 로직 | 여기에 있음 | SimGame에 위임 |
| Python/RL에서 사용 | 직접 사용 | 사용 안 함 |

### Q. "결정론"이 왜 그렇게 중요한가?

멀티플레이에서 상태를 직접 전송하지 않기 때문.  
같은 시드 + 같은 입력 → **반드시** 같은 결과여야 함.  
그래서 `rand()` 같은 시스템 함수를 쓰지 않고 직접 구현한 RNG(`core/rng.h`)를 사용.

### Q. inputDelay가 뭔가?

```
내가 틱 100에 보낸 입력이 상대에게 틱 102에 도착
→ 상대는 틱 100 입력을 못 받고 시뮬을 돌릴 수 없음
→ inputDelay=2로 설정하면 양측 모두 2틱 늦게 시뮬
→ 네트워크 지연이 있어도 동기화 유지
```

값이 클수록 지연에 강하지만 입력 반응이 느려짐.

### Q. StateHash는 왜 계산하는가?

- 양측이 같은 상태인지 검증하는 "지문"
- 현재 C++ 클라이언트는 600틱(10초)마다 해시를 교환 → 다르면 desync
- `core/hash.h`의 FNV-1a 64-bit로 그리드 전체를 해싱

### Q. 왜 relay 와 meta 를 분리했나?

- `tetris_relay` 는 모바일 Linux (Termux proot 등) 에 떠 있는 무상태/가벼운 포워더.
  자주 죽고 자주 살리는 환경이라 DB 를 들고 있으면 안 됨.
- `tetris_meta` 는 항상 켜둘 수 있는 별도 호스트 (Mac mini) 에서 SQLite 영속.
- 다른 게임으로 갈아끼울 때 meta 만 그대로 재사용할 수 있게 의존 방향을 단방향으로 묶음.

### Q. MATCH_SUMMARY 를 왜 양쪽이 모두 보내는가?

- 한쪽만 보내면 self-report 조작이 가능 — 자기 점수만 부풀려 보내면 끝.
- 양쪽 관측값을 릴레이가 모아 교차검증해서, 핵심 필드 (승자, 라인 수 등) 가
  불일치하면 ELO 미반영하고 그냥 매치를 unranked 처리.
- `server/relay.cpp::finalizeRanked` 에서 두 `MATCH_SUMMARY` 가 모두 모인 후에만
  meta `/v1/matches` 호출.

---

## 코드를 고칠 때 어디를 건드려야 하나

| 고치려는 것 | 파일 |
|---|---|
| 블록 낙하 속도 | `sim_game.cpp` — `dropIntervalTicks` |
| 블록 모양/회전 | `src/sim_blocks.h` |
| 점수 계산 | `sim_game.cpp` — `UpdateScore()` |
| T-spin 판정 | `sim_game.cpp` — `IsTSpinLock()` / `LockBlock()` |
| 키 배치/홀드 반복 | `src/main.cpp` — `AccumulateInput()` / `ConsumeInput()` |
| 화면 레이아웃 | `src/game.cpp` — `Draw()` |
| 네트 메시지 추가 | `net/framing.h` (MsgType), `net/session.cpp` (handleFrame) |
| RNG/결정론 | `core/rng.h` (건드리면 해시가 바뀜, 주의) |

---

## ARCHITECTURE.md 활용법

이 가이드로 전체 흐름을 잡은 후, 특정 함수나 파일의 상세 사양이 필요할 때 ARCHITECTURE.md의 **§15 전체 함수 레퍼런스**를 찾아보는 용도로 사용하세요.

> "처음엔 GUIDE.md → 이해되면 코드 직접 → 상세 스펙은 ARCHITECTURE.md"
