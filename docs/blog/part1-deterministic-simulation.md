# Part 1: 테트리스 시뮬레이션 엔진 — 결정론적 게임 로직

> **시리즈:** 제로부터 멀티플레이어 테트리스 + RL | [시리즈 목차](./README.md) | **Part 1**

---

## 이 장의 구현 계약

- **선행 상태:** [Part 0](./part0-project-setup.md) 의 빌드 뼈대 (`CMakeLists.txt` 와 "tetris project skeleton" 을 찍는 `src/main.cpp`) 뿐이다. 이 장은 그 위에 새 파일만 얹는다.
- **이번 장의 파일:** `core/constants.h`, `core/input.h`, `core/rng.h`, `core/hash.h`, `src/position.{h,cpp}`, `src/sim_block.h`, `src/sim_grid.h`, `src/sim_blocks.h`, `src/sim_game.{h,cpp}`, `tests/sim_hash_dump.cpp`, 그리고 `CMakeLists.txt` 확장.
- **연결점:** 화면·오디오 없이 입력과 시드만으로 상태가 결정되는 코어를 만든다. 이후 모든 Part 가 이 위에 올라간다 — [Part 4](./part4-game-wrapper-and-loop.md) 의 `Game` 래퍼, [Part 6](./part6-lockstep-networking.md) 의 lockstep, [Part 8](./part8-python-rl.md) 의 pybind11 바인딩이 전부 `SimGame` 을 그대로 쓴다.
- **완료 게이트:** `sim_hash_dump` 출력이 `python/tests/_sim_hash_dump.txt` 와 **한 바이트도 다르지 않아야** 한다. 실행 명령은 이 장 말미의 수동 테스트에 있다.

`core/rng.h` 와 `core/hash.h` 를 선행 상태가 아니라 **이번 장의 산출물**로 잡은 데 주의하라. Part 0 은 이 파일들을 만들지 않는다. 결정론의 두 축인 RNG 와 상태 해시는 그 설계 근거를 시뮬레이션 규칙과 함께 봐야 이해되므로 여기서 처음 만든다.

## 들어가며

첫 구현 대상은 창이나 그림이 아니라 **상태 전이 규칙**이다. 화면이 없는 `SimGame`부터 만들면 한 입력의 결과를 해시와 테스트로 고정한 뒤, 이후의 모든 플랫폼·렌더링·네트워크 계층을 같은 코어 위에 올릴 수 있다.

이 프로젝트에서 게임 로직은 렌더링과 **완전히 분리**되어 있다. `SimGame` 클래스는 화면에 무엇을 그리는지 모른다. 입력을 받아 상태를 갱신하고, 그리드와 블록의 현재 상태를 노출할 뿐이다.

이 분리가 주는 세 가지 이점:

1. **결정론적 네트플레이**: 같은 시드 + 같은 입력 순서 = 동일한 상태. 네트워크로 입력만 교환하면 양쪽의 시뮬레이션이 일치한다.
2. **Headless RL 학습**: GPU 렌더링 없이 순수 시뮬레이션만 반복할 수 있다. 실제 처리량은 CPU, pybind11 호출 방식, 병렬화 방식에 따라 달라지므로 이 문서에서는 고정 수치를 제시하지 않는다. 학습은 Google Colab에서 Linux 환경으로 돌리고, 배포 머신에서는 추론만 한다.
3. **크로스 플랫폼 이식**: 렌더링 없는 순수 C++ 로직이므로 Win32 API에 의존하지 않는다. Linux, macOS, WASM 어디서든 컴파일 가능하다.

이 장의 실제 파일 경계는 `src/sim_game.{h,cpp}`, `src/sim_grid.h`, `src/sim_block.h`, `src/sim_blocks.h`, `core/rng.h`, `core/hash.h`다.

---

## 1. 아키텍처 개요

이 장이 끝났을 때 존재하는 것은 이게 전부다.

```mermaid
graph TB
    T["tests/sim_hash_dump.cpp<br/>고정 입력 스크립트"]
    subgraph SG["SimGame — 이 장에서 만드는 것"]
        D["SubmitInput(mask) / Tick()"]
        E["SimGrid: 20x10 int 배열"]
        F["SimBlock: 위치 + 회전"]
        G["XorShift64*: 블록 순서 RNG"]
        G2["XorShift64*: 가비지 RNG"]
        H["StateHash(): FNV-1a 64-bit"]
    end
    OUT["stdout 해시 덤프<br/>골든 파일과 diff"]

    T -->|"입력 마스크 1바이트"| D
    D --> E & F & G & G2
    E & F & G & G2 --> H
    H --> OUT
```

창도, 그림도, 소리도, 네트워크도 없다. **입력을 넣으면 상태가 바뀌고, 그 상태를 64비트 숫자 하나로 요약할 수 있는 것**까지가 이 장의 범위다. 확인하는 방법도 게임을 실행하는 것이 아니라 테스트 프로그램이 찍은 숫자를 비교하는 것이다.

화면 없이 시작하는 것이 답답해 보일 수 있다. 그런데 순서를 뒤집으면 곤란해진다. 화면부터 만들면 "규칙이 맞는지"를 눈으로만 확인하게 되고, 눈으로 보는 검증은 자동화되지 않는다. 지금 규칙을 골든 해시로 고정해 두면 어떤 계층을 얹더라도 같은 입력의 상태 전이가 변하지 않았는지 자동으로 확인할 수 있다.

**입력이 `SubmitInput(mask)` 라는 형태인 것도 지금 정해야 한다.** 한 틱의 입력을 1바이트 비트마스크로 받으면, 나중에 그 바이트를 그대로 파일에 쓰면 리플레이가 되고 그대로 소켓에 보내면 네트워크 동기화가 된다. 함수 시그니처 하나가 뒤의 여러 장을 결정한다.

`SimGame` 에 무엇이 붙게 되는지는 이 장의 관심사가 아니지만, 지금의 설계 판단이 어디를 향하는지는 알아 둘 만하다.

| 나중에 이 API 를 쓰는 것 | 쓰는 방식 | 어느 장 |
|---|---|---|
| 게임 화면 | `Grid()` / `CurrentBlock()` 을 읽어 그린다 | [Part 4](./part4-game-wrapper-and-loop.md) |
| 멀티플레이 | 입력 바이트를 교환하고 `StateHash()` 로 대조한다 | [Part 6](./part6-lockstep-networking.md) |
| 강화학습 | `ApplyPlacement(col, rot)` 로 한 수씩 둔다 | [Part 8](./part8-python-rl.md) |

세 가지가 **같은 `SimGame` 을 고쳐 쓰지 않고 그대로 쓴다**는 점이 중요하다. 그러려면 `SimGame` 이 저 셋 중 어느 것도 몰라야 한다.

### 1.1 먼저 만드는 시뮬레이션 파일

본격적인 시뮬레이션에 들어가기 전에, 나머지 전부가 include 하게 될 작은 파일 셋을 먼저 둔다. 셋 다 짧고 의존성이 없다.

**현재 소스 발췌 — `core/constants.h`**

```cpp
#pragma once

// Simulation tick rate (logic updates per second)
// 시뮬레이션의 시간 단위. 초가 아니라 '틱' 으로 센다.
// 부동소수 누적 오차가 두 실행 사이에 차이를 만들지 않게 하려는 것이다.
constexpr int TICKS_PER_SECOND = 60;
constexpr float SECONDS_PER_TICK = 1.0f / static_cast<float>(TICKS_PER_SECOND);
```

`TICKS_PER_SECOND` 는 이 프로젝트 전체에서 가장 널리 퍼지는 상수다. 중력 간격, 소프트 드롭 레이트, 네트워크 틱 번호, 게임 시작 지연이 전부 이 값에 걸려 있다. **시간을 초가 아니라 틱으로 세는 것이 결정론의 출발점**이다 — 초를 쓰면 부동소수 누적 오차가 두 클라이언트를 갈라놓는다.

**현재 소스 발췌 — `core/input.h`**

```cpp
#pragma once
#include <cstdint>

// Bitmask representing per-tick inputs
// 한 틱의 입력 전체를 1바이트에 담는다.
// (틱 번호, 이 1바이트) 쌍만 있으면 게임을 그대로 재현할 수 있다.
enum InputBits : uint8_t {
    INPUT_NONE   = 0,
    INPUT_LEFT   = 1 << 0,
    INPUT_RIGHT  = 1 << 1,
    INPUT_DOWN   = 1 << 2,
    INPUT_ROTATE = 1 << 3,
    INPUT_DROP   = 1 << 4,
};

inline bool hasInput(uint8_t mask, InputBits bit) { return (mask & bit) != 0; }
```

한 틱의 입력 전체가 **1바이트**다. INPUT 네트워크 프레임과 `core/replay.cpp` 의 리플레이 파일이 이 바이트를 그대로 공유하므로 별도 직렬화가 필요 없다. 규칙 엔진의 입력 단위가 테스트·네트워크·재생 파일의 공통 계약이 된 셈이다.

`InputBits`는 현재 wire 단위인 `uint8_t` 안에 들어가며 반시계 회전, 홀드, 180도 회전은 지원하지 않는다. 입력을 추가할 때는 비트만 늘리는 것으로 끝나지 않고 SRS 규칙, 리플레이, lockstep payload, Python 행동 변환을 같은 계약으로 갱신해야 한다.

**현재 소스 발췌 — `src/position.h`**

```cpp
#pragma once

class Position{
public:
    Position(int row, int column);
    int row;
    int column;
};
```

**현재 소스 발췌 — `src/position.cpp`**

```cpp
#include "position.h"

Position::Position(int row, int column)
{
    this -> row = row;
    this -> column = column;
}
```

`Position` 은 `(row, column)` 순서다. `(x, y)` 가 아니다. 이 순서를 끝까지 유지하는 것이 중요하다 — 그리드가 `grid[row][column]` 이고 테트로미노 셀 정의도 `Position(row, col)` 이라, 한 곳이라도 뒤집히면 블록이 90도 돌아간 채로 나온다. 화면 좌표계(`x` 가 가로)와 만나는 지점은 렌더링뿐이며, 그것은 [Part 3](./part3-rendering-and-ui.md) 의 문제다.

---

## 2. 그리드 표현

### 2.1 데이터 구조

테트리스 그리드는 20행 x 10열의 정수 배열이다:

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// src/sim_grid.h
class SimGrid
{
public:
    static constexpr int kRows = 20;
    static constexpr int kCols = 10;

    int grid[kRows][kCols];  // 0 = 빈칸, 1~7 = 블록 ID, 8 = 고스트, 9 = 가비지
    // ...
};
```

좌표계: `grid[0][0]`은 좌상단, `grid[19][9]`는 우하단이다. 행(row)이 증가하면 아래로, 열(col)이 증가하면 오른쪽으로 이동한다.

```mermaid
graph TB
    subgraph "테트리스 필드 (20행 x 10열)"
        TL["grid[0][0]<br/>좌상단"] --- TR["grid[0][9]<br/>우상단"]
        TL --- BL["grid[19][0]<br/>좌하단"]
        TR --- BR["grid[19][9]<br/>우하단"]
        BL --- BR
    end
```

셀 값의 의미:

셀 값(id)을 색상으로 변환하는 것은 `colors.cpp` 의 `GetCellColors()` 가 반환하는 팔레트 벡터다. id 가 곧 이 벡터의 인덱스이므로, 아래 색상 열은 `GetCellColors()` 의 원소 순서와 1:1로 대응한다:

| 값 | 의미 | 색상 (렌더링) |
|----|------|-------------|
| 0 | 빈칸 | darkGrey (배경) |
| 1 | L 블록 | green (초록) |
| 2 | J 블록 | red (빨강) |
| 3 | I 블록 | orange (주황) |
| 4 | O 블록 | yellow (노랑) |
| 5 | S 블록 | purple (보라) |
| 6 | T 블록 | cyan (청록) |
| 7 | Z 블록 | blue (파랑) |
| 8 | 고스트 | ghostColor (반투명 흰회색) |
| 9 | 가비지 | garbageColor (어두운 회색) |

> **참고:** 색상은 표준 테트리스 가이드라인의 피스별 관용색과 다르다. 이 프로젝트는 원본 raylib 코드의 팔레트(`GetCellColors()`)를 비트 호환을 위해 그대로 계승했으므로, "L=초록, I=주황"처럼 id→색 매핑이 관용과 어긋난다. id→피스 매핑(L=1, J=2, I=3, O=4, S=5, T=6, Z=7) 자체는 `sim_blocks.h` 와 일치한다.

### 2.2 왜 int인가

셀 값이 0~9이므로 `uint8_t`면 충분하다. 그러나 `int` (4바이트)를 사용하는 이유:

1. **연속 메모리 레이아웃**: `int grid[20][10]`은 800바이트 연속 메모리. `fnv1a64(&grid[0][0], sizeof(grid), h)`로 한 번에 해시할 수 있다.
2. **원본 호환성**: 원래 raylib 기반 코드(`Grid` 클래스)가 `int`를 사용했고, 상태 해시의 비트 단위 일치를 유지해야 한다.

그리드 전체가 800바이트라서 상태 복사와 해시 순회 비용은 작다. 정확히 몇 개의 캐시 라인을 건드리는지는 시작 주소 정렬과 CPU에 따라 달라지므로, 여기서 중요한 계약은 **작고 연속된 고정 크기 배열**이라는 점이다.

### 2.3 경계 검사와 빈칸 판별

**현재 소스 발췌 — `src/sim_grid.h`**

```cpp
    bool IsCellOutside(int row, int column) const
    {
        if (row >= 0 && row < kRows && column >= 0 && column < kCols)
        {
            return false;
        }
        return true;
    }

    bool IsCellEmpty(int row, int column) const
    {
        // 방어적 경계 검사: 범위 밖 좌표는 '비어있지 않음'(막힘)으로 처리한다.
        // 호출부는 보통 IsCellOutside 로 선검사하지만, 만약 무경계 접근이 들어와도
        // OOB 읽기를 방지한다. 해시 대상은 grid 내용뿐이므로 결정성/리플레이 호환성에
        // 영향이 없다.
        if (IsCellOutside(row, column))
        {
            return false;
        }
        if (grid[row][column] == 0 || grid[row][column] == 8)
        {
            return true;
        }
        return false;
    }
```

`IsCellEmpty`에서 고스트(id=8)를 빈칸으로 취급하는 것에 주의하라. 고스트 블록은 "현재 블록이 떨어질 위치"를 보여주는 시각적 가이드일 뿐, 물리적 충돌 대상이 아니다.

가비지(id=9)는 빈칸이 아니다. 고스트와 가비지의 차이는: 고스트는 현재 피스가 렌더링 힌트로 투영된 그림자이고, 가비지는 상대방이 보낸 물리적 블록이다. 빈칸 판정에서 가비지는 벽돌처럼 취급된다.

---

## 3. 테트로미노 형상과 회전

### 3.1 7종 블록

표준 테트리스의 7종 테트로미노. 각 블록은 4개의 셀로 구성된다:

```text
L (id=1)  J (id=2)  I (id=3)  O (id=4)  S (id=5)  T (id=6)  Z (id=7)

    #       #         ####      ##        ##          #        ##
  ###       ###                 ##       ##          ###        ##
```

### 3.2 회전 상태 룩업 테이블

각 블록은 최대 4개의 회전 상태를 가진다. 회전 상태별로 4개 셀의 **상대 좌표**(오프셋)를 미리 정의해둔다:

**예시(실제 저장소에는 없음)**

```cpp
// src/sim_blocks.h — T 블록 예시
class SimTBlock : public SimBlock
{
public:
    SimTBlock()
    {
        id = 6;
        cells[0] = {Position(0,1), Position(1,0), Position(1,1), Position(1,2)};
        cells[1] = {Position(0,1), Position(1,1), Position(1,2), Position(2,1)};
        cells[2] = {Position(1,0), Position(1,1), Position(1,2), Position(2,1)};
        cells[3] = {Position(0,1), Position(1,0), Position(1,1), Position(2,1)};
        Move(0, 3);  // 초기 위치: 3열 오프셋 (필드 중앙)
    }
};
```

회전 상태 0~3은 시계 방향 90도씩 회전한 형태다:

```text
rot=0     rot=1     rot=2     rot=3
  #         #         .         #
 ###       ##        ###       ##
  .         #         #         #
```

`cells`는 `std::map<int, std::vector<Position>>`으로 구현되어 있다. 각 키(0~3)에 대해 4개의 Position(row, column) 벡터가 매핑된다.

### 3.3 SRS와 단순 회전

이 구현에서는 **Super Rotation System(SRS)** 의 wall kick을 적용하지 않는다. 회전 후 벽이나 다른 블록과 겹치면 단순히 회전을 취소(undo)한다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::RotateBlockImpl()
{
    if (gameOver) return;
    currentBlock.Rotate();
    if (IsBlockOutside(currentBlock) == true || BlockFits(currentBlock) == false)
    {
        currentBlock.UndoRotation();
    }
    else
    {
        lastMoveWasRotate = true;
        rotateSoundEvent = true;
        ghostBlock = MakeGhostBlock(currentBlock);
    }
}
```

`lastMoveWasRotate = true;` 한 줄을 지금 넣어 두는 것이 중요하다. 이 플래그가 T-spin 판정의 전제이고(§B 에서 `IsTSpinLock` 이 읽는다), **상태 해시에도 들어간다**. 빠뜨리면 이 장의 완료 게이트인 골든 해시 비교가 `step=004` 부터 어긋난다 — 회전이 일어나는 첫 스텝이 거기다. 회전이 **성공했을 때만** 세운다는 점도 같이 봐야 한다. `UndoRotation()` 으로 되돌린 회전은 "회전한 적 없음" 이다.

SRS wall kick 은 회전 실패 시 블록을 좌우/상하로 밀어보는 추가 로직이다. Tetris Guideline 이 정의하는 공식 규칙이지만 이 프로젝트에서는 단순성을 위해 생략했다. SRS 를 구현하려면 **회전 전이**(4회전 × 2방향 = 8전이)마다 5개의 kick offset 을 정의한 표가 필요하고, JLSTZ 계열과 I 블록이 서로 다른 표를 쓴다.

여기에는 더 근본적인 제약이 하나 있다. 이 시뮬레이션에는 **반시계 회전 입력 자체가 없다.** `core/input.h` 의 회전 입력은 `INPUT_ROTATE` 하나뿐이고 `SimBlock::Rotate()` 도 시계 방향만 돈다. 즉 8전이 중 4개만 존재한다. SRS 를 제대로 넣으려면 입력 비트와 `SimBlock` 의 회전 API 부터 늘려야 한다.

회전을 시계 방향 하나로 묶은 대가는 T-spin 난이도다. 실전에서 T-spin 은 반시계 회전으로 들어가는 형태가 많은데, 그 진입로가 통째로 막혀 있다. `IsTSpinLock` 이 판정하는 T-spin 은 시계 회전으로 들어갈 수 있는 형태에 한정된다.

### 3.4 절대 좌표 계산

블록의 셀 위치는 **상대 좌표**(cells) + **오프셋**(rowOffset, columnOffset)으로 계산된다:

**현재 소스 발췌 — `src/sim_block.h`**

```cpp
    std::vector<Position> GetCellPositions() const
    {
        const std::vector<Position>& tiles = cells.at(rotationState);
        std::vector<Position> movedTiles;
        movedTiles.reserve(tiles.size());
        for (const Position& item : tiles)
        {
            movedTiles.emplace_back(item.row + rowOffset, item.column + columnOffset);
        }
        return movedTiles;
    }
```

예: T 블록(rot=0)이 rowOffset=5, columnOffset=3일 때:

```text
cells[0] = {(0,1), (1,0), (1,1), (1,2)}

절대 좌표 = {(5,4), (6,3), (6,4), (6,5)}
```

이 분리(상대 좌표 + 오프셋)는 같은 형상 데이터를 여러 위치에서 재사용할 수 있게 한다. 고스트 블록도 현재 블록과 같은 형상/회전 데이터를 공유하되, rowOffset만 다르다.

---

## 4. 충돌 감지

### 4.1 이동-후-검증 패턴

이동/회전의 충돌 감지는 "먼저 이동, 그 다음 검증, 실패 시 복원"하는 패턴을 따른다:

```mermaid
flowchart LR
    A["Move/Rotate<br/>상태 변경"] --> B{"IsBlockOutside?<br/>or !BlockFits?"}
    B -->|Yes| C["Undo<br/>상태 복원"]
    B -->|No| D["유지<br/>고스트 갱신"]
```

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// 좌측 이동 — src/sim_game.cpp
void SimGame::MoveBlockLeft()
{
    if (gameOver) return;
    currentBlock.Move(0, -1);                                // 1) 이동
    if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
    {
        currentBlock.Move(0, 1);                             // 2) 복원
    }
    else
    {
        ghostBlock = MakeGhostBlock(currentBlock);           // 3) 고스트 갱신
    }
}
```

### 4.2 두 단계 검사

충돌 검사는 두 단계로 나뉜다:

**1단계 — 경계 검사 (IsBlockOutside):**

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
bool SimGame::IsBlockOutside(const SimBlock& block) const
{
    std::vector<Position> tiles = block.GetCellPositions();
    for (const Position& item : tiles)
    {
        if (sim_grid.IsCellOutside(item.row, item.column))
        {
            return true;
        }
    }
    return false;
}
```

블록의 4개 셀 중 하나라도 그리드 범위(0~19행, 0~9열) 밖이면 true.

**2단계 — 점유 검사 (BlockFits):**

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
bool SimGame::BlockFits(const SimBlock& block) const
{
    std::vector<Position> tiles = block.GetCellPositions();
    for (const Position& item : tiles)
    {
        if (sim_grid.IsCellEmpty(item.row, item.column) == false)
        {
            return false;
        }
    }
    return true;
}
```

블록의 4개 셀 중 하나라도 이미 점유된 셀과 겹치면 false. 호출 관례상 `IsBlockOutside` 를 먼저 돌려 범위를 거른 뒤 `BlockFits` 로 점유를 확인한다. 다만 `IsCellEmpty` 자체가 진입부에 `if (IsCellOutside(...)) return false;` 가드(섹션 2.3 참고)를 두고 있으므로, 설령 범위 밖 좌표가 `BlockFits` 로 직접 들어와도 `grid[row][column]` 에 대한 **배열 경계 초과(out-of-bounds access)** 는 발생하지 않는다 — 범위 밖은 "막힌 셀"로 간주되어 `false` 가 된다. 즉 두 검사의 순서는 의미(범위 위반 vs 충돌)를 구분하기 위한 것이지, OOB 크래시를 막기 위한 필수 조건은 아니다.

### 4.3 하드 드롭

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::MoveBlockDrop()
{
    if (gameOver) return;
    while (IsBlockOutside(currentBlock) == false && BlockFits(currentBlock) == true)
    {
        currentBlock.Move(1, 0);
    }
    currentBlock.Move(-1, 0);
    dropSoundEvent = true;
    hardDropEvent  = true;   // 흔들림용 (렌더 전용, 해시 무관)
    LockBlock();
}
```

하드 드롭은 블록을 충돌할 때까지 아래로 반복 이동시킨 후, 한 칸 위로 복원한다. `while` 루프가 종료된 시점에서 블록은 **충돌 상태**이므로 `Move(-1, 0)` 으로 마지막 유효 위치로 돌아가야 한다.

마지막 두 줄이 이 함수의 유일하게 미묘한 부분이다. **같은 사건에 대해 플래그를 두 개 세운다.**

**현재 소스 발췌 — `src/sim_game.h`**

```cpp
    // ---- One-shot event flags for audio in the Game wrapper ----
    // Set by SimGame when the corresponding event occurs (successful rotate,
    // line clear). The Game wrapper reads and clears them each tick.
    mutable bool rotateSoundEvent  = false;
    mutable bool clearSoundEvent   = false;
    mutable bool dropSoundEvent    = false;  // 하드드롭(Space) 시
    mutable bool garbageSoundEvent = false;  // 가비지 행 수신 시
    // 하드드롭 화면 흔들림(약) 트리거용. dropSoundEvent 와 별개 — 그쪽은
    // 오디오(game.cpp)가 소비·리셋하므로 흔들림이 그것에 의존하면 안 된다.
    // 렌더 전용 1회 플래그 (해시/lockstep/replay 와 무관).
    mutable bool hardDropEvent     = false;  // 하드드롭(Space) 시 (흔들림용)

    // ---- Combat event flags (Section I) ----
    // LockBlock 내부에서 세팅되고 렌더러(쉐이크/이펙트)가 소비 후 클리어.
    mutable int  lastLinesCleared = 0;    // 마지막 LockBlock의 라인 클리어 수 (0..4)
    mutable int  lastTSpinLines = -1;     // T-spin 이벤트면 0..3, 아니면 -1
    mutable int  lastGarbageReceived = 0; // 마지막 LockBlock에서 실제 주입된 가비지 행 수
    mutable bool gameOverEvent = false;   // 이 틱에 gameOver 로 전이한 경우 1회
```

`dropSoundEvent` 와 `hardDropEvent` 가 나뉜 이유는 **소비자가 둘이기 때문**이다. 오디오는 [Part 5](./part5-audio.md) 의 `Game` 래퍼가 소비하고 즉시 리셋하며, 화면 흔들림은 [Part 4](./part4-game-wrapper-and-loop.md) 의 `apply_fx` 람다가 소비한다. 한 플래그를 공유하면 먼저 도는 쪽이 리셋해 나머지 하나가 영영 사건을 못 본다. 실행 순서에 의존하는 버그라 재현이 들쭉날쭉해 찾기 어렵다.

이 플래그들이 전부 `mutable` 인 것은 `const` 멤버 함수에서도 세울 수 있게 하기 위해서다. 그리고 **어느 것도 상태 해시에 들어가지 않는다.** 헤더 주석이 "렌더 전용 1회 플래그 (해시/lockstep/replay 와 무관)" 라고 못 박은 그대로다. 소리를 끈 클라이언트와 켠 클라이언트도 같은 게임 상태와 해시를 가져야 하므로, 소비 여부가 시뮬레이션 결정성에 영향을 주어서는 안 된다.

---

## 5. 라인 클리어 알고리즘

### 5.1 역순 순회의 이유

라인 클리어에서 가장 흔한 실수는 순방향(row 0 -> 19)으로 순회하는 것이다. 순방향 순회의 문제:

```text
순방향 순회 시:
row 17: ■■■■■■■■■■ ← 가득 참, 삭제 → 위의 row를 아래로 이동
row 18: ■■■■■■■■■■ ← 이제 이 자리에 옛 row 17의 위 행이 옴
                      → 원래 row 18은 이미 검사를 마쳤으므로 다시 확인되지 않음
```

역순(row 19 -> 0)이면 이 문제가 없다. 아래에서 위로 올라가며, 가득 찬 행을 삭제할 때 `completed` 카운터를 증가시키고, 가득 차지 않은 행은 `completed`만큼 아래로 이동시킨다:

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// src/sim_grid.h
int ClearFullRows()
{
    int completed = 0;
    for (int row = kRows - 1; row >= 0; row--)
    {
        if (IsRowFull(row))
        {
            ClearRow(row);         // 해당 행을 0으로 초기화
            completed++;
        }
        else if (completed > 0)
        {
            MoveRowDown(row, completed);  // completed칸 아래로 복사
        }
    }
    return completed;
}
```

### 5.2 단계별 예시

2줄 동시 클리어의 경우:

```text
초기 상태:          row 17 클리어 후:      row 18 클리어 후:     비-풀 행 이동 후:
row 15: ..■■....   row 15: ..■■....     row 15: ..■■....    row 15: ..........
row 16: .■■■■...   row 16: .■■■■...     row 16: .■■■■...    row 16: ..........
row 17: ■■■■■■■■■■ row 17: ..........   row 17: ..........    row 17: ..■■......
row 18: ■■■■■■■■■■ row 18: ■■■■■■■■■■  row 18: ..........    row 18: .■■■■.....
row 19: .■■■■■■..  row 19: .■■■■■■..   row 19: .■■■■■■..    row 19: .■■■■■■..
```

역순 순회이므로 row 19 → 18 → 17 → 16 → 15 순서로 처리한다. row 18과 17이 풀이면 `completed=2`. row 16은 `MoveRowDown(16, 2)` → row 18로 복사. row 15는 `MoveRowDown(15, 2)` → row 17로 복사.

### 5.3 size_t 주의점

`ClearFullRows`에서 루프 변수 `row`를 `size_t`(unsigned)로 선언하면 위험하다:

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// 위험: size_t는 unsigned이므로 row = 0일 때 row-- = 4294967295
for (size_t row = kRows - 1; row >= 0; row--)  // 무한 루프!
```

unsigned 정수에서 `0 - 1`은 언더플로되어 매우 큰 양수가 된다. `row >= 0`은 항상 true이므로 무한 루프에 빠진다. 반드시 `int`를 사용해야 한다.

---

## 6. 점수 시스템

아래는 **중간 단계(staged)** 형태다 — 레벨·T-spin 이 아직 없던 시점의 모습이며, 최종 코드가 아니다:

**Part 1 체크포인트 — `src/sim_game.cpp` (중간 단계)**

```cpp
// src/sim_game.cpp — 점수표만 먼저 붙인 중간 단계.
void SimGame::UpdateScore(int linesCleared, int levelUp)
{
    switch (linesCleared)
    {
    case 1: score += 100;  break;
    case 2: score += 300;  break;
    case 3: score += 600;  break;
    case 4: score += 1000; break;
    default: break;
    }
    score += levelUp * 1000;
}
```

| 클리어 줄 수 | 점수 | 비고 |
|-------------|------|------|
| 1줄 (Single) | 100 | 기본 |
| 2줄 (Double) | 300 | 3배 (1줄의 3배) |
| 3줄 (Triple) | 600 | 6배 |
| 4줄 (Tetris) | 1000 | 10배 — 4줄 동시 클리어의 보상이 압도적 |

이 점수표는 여러 줄을 한 번에 지울수록 더 크게 보상하도록 만든 프로젝트 고유 규칙이다. 완성형 sim은 이 base 점수에 현재 `level`을 곱하고, 라인 누적에 따른 레벨업·중력 가속과 T-spin 분기를 함께 적용한다. `UpdateScore(linesCleared, levelUp, tSpin)`이 한 잠금 사건에서 점수와 레벨을 갱신한다. 위의 단순형은 base 표를 이해하기 위한 체크포인트이고, `레벨 시스템과 T-spin` 블록이 현재 계약이다.

점수가 비선형적으로 증가하는 것이 핵심 게임 디자인이다: 4줄 동시 클리어(Tetris)의 보상이 1줄씩 4번 클리어(400점)보다 2.5배 높으므로, 플레이어에게 "I 블록을 기다려서 4줄을 한꺼번에 클리어"하는 전략적 선택을 유도한다.

---

## 7. 7-Piece Bag 랜더마이저

### 7.1 순수 랜덤의 문제

블록을 순수 랜덤으로 생성하면 같은 블록이 연속으로 나올 확률이 $1/7 \approx 14.3\%$이다. S와 Z가 연속 5번 나오면 게임이 사실상 불가능해진다.

### 7.2 가방 랜더마이저

Tetris Guideline(The Tetris Company)이 정의하는 공식 랜덤 알고리즘: 7종 블록을 "가방"에 넣고 섞은 후, 하나씩 꺼낸다. 가방이 비면 다시 채운다.

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
SimBlock SimGame::GetRandomBlock()
{
    // '가방' 이 비면 새로 채운다. RNG 를 부르는 횟수가 입력에 따라
    // 달라지지 않도록 주의 — 이 함수가 RNG의 유일한 호출 지점입니다.
    if (blocks.empty())
    {
        blocks = GetAllBlocks();
    }
    int randomIndex = rng.nextUInt(static_cast<uint32_t>(blocks.size()));
    SimBlock block = blocks[randomIndex];
    blocks.erase(blocks.begin() + randomIndex);
    return block;
}
```

가방에서 랜덤 인덱스로 하나를 뽑고 제거한다. Fisher-Yates 셔플과 동일한 결과를 낸다.

가방 랜더마이저의 성질:

- 같은 블록이 **연속 2번** 나올 수 있다. 이전 가방의 마지막과 새 가방의 첫 번째가 같은 경우다. 한 가방 안에는 같은 조각이 하나뿐이므로 3연속은 나오지 않는다.
- **가방 경계에 정렬된** 14개, 즉 완전한 두 가방에는 각 블록이 정확히 두 번 나온다.
- **임의 위치에서 잘라낸** 길이 14 구간은 이전 가방의 suffix, 완전한 가운데 가방, 다음 가방의 prefix에 걸칠 수 있다. 같은 조각은 각 가방에서 한 번씩만 나오므로 최대 세 번까지 등장할 수 있다.
- 각 블록이 정확히 한 번 나온다는 보장은 **완전한 가방 단위**에만 적용된다. 임의의 연속 7개 구간에는 적용되지 않는다.

### 7.3 결정론의 핵심: RNG 호출 지점

**RNG 호출은 `GetRandomBlock()` 안에서만 발생한다.** 이것이 결정론의 핵심 불변 조건이다.

만약 입력 처리 코드에서 RNG를 호출하면 (예: 파티클 이펙트용 난수), 입력 타이밍에 따라 RNG 상태가 달라지고, 같은 시드 + 같은 입력이라도 블록 순서가 달라진다. 결정론이 깨진다.

```text
불변 조건: RNG 호출 순서 = 블록 생성 순서 (입력/타이밍과 무관)

위반 예시:
  Tick 100: 블록 락 → GetRandomBlock() → rng.nextUInt()   [RNG call #5]
  Tick 101: 파티클 생성 → rng.nextFloat()                   [RNG call #6] ← 위반!
  Tick 150: 블록 락 → GetRandomBlock() → rng.nextUInt()   [RNG call #7]

  → 파티클이 없으면 #6이 빠지므로 #7의 RNG 상태가 달라짐
```

이 불변 조건은 코드 리뷰에서 자동으로 검증하기 어렵다. `rng.next*()` 호출이 `GetRandomBlock()` 외부에 없는지 수동으로 확인해야 한다.

---

## 8. XorShift64* RNG

### 8.1 왜 std::mt19937이 아닌가

`std::mt19937`(Mersenne Twister)은 C++ 표준 라이브러리의 대표적 RNG이다. 흔히 "엔진 출력이 컴파일러마다 달라서 못 쓴다" 고 알려져 있지만 **그건 사실이 아니다.** `std::mt19937` 이 뱉는 수열은 표준이 완전히 규정하며, 같은 시드를 넣으면 MSVC 든 GCC 든 libstdc++ 든 libc++ 든 **비트 단위로 같은 값**이 나온다.

진짜 문제는 한 겹 위에 있다:

표준은 **엔진**은 규정하지만 **분포 어댑터**(`std::uniform_int_distribution`, `std::shuffle` 등)의 내부 알고리즘은 규정하지 **않는다**. 같은 엔진 상태에서 `std::uniform_int_distribution<int>(0, 6)(engine)` 을 호출해도 MSVC 와 GCC 가 다른 값을 돌려줄 수 있고, 엔진을 몇 번 소비하는지도 다를 수 있다. 7-bag 을 섞는 데 이걸 쓰면 두 플랫폼의 블록 순서가 갈라진다.

그래서 `std::mt19937` 을 쓰려면 분포 계산을 직접 짜야 한다. 그 순간 "표준 라이브러리를 쓴다" 는 이점의 절반이 사라진다. 남은 절반 — 2.5 KB 짜리 624워드 상태와 `MT19937` 특유의 초기화 절차 — 은 이 프로젝트에는 오히려 부담이다. 아래에서 볼 두 가지 이유 때문이다.

이 프로젝트에서는 RNG 알고리즘, 분포 함수, 상태 크기를 모두 직접 소유한다.

**현재 소스 발췌 — `core/rng.h`**

```cpp
#pragma once
#include <cstdint>

// Simple xorshift64* RNG for deterministic cross-platform randomness
// 같은 시드 + 같은 호출 순서 = 같은 난수열. 이것이 이 클래스의 전부다.
// 블록 순서와 가비지 구멍 위치가 여기 달려 있다.
class XorShift64Star {
public:
    explicit XorShift64Star(uint64_t seed = 88172645463393265ull) : state(seed ? seed : 88172645463393265ull) {}

    // Next 64-bit value
    uint64_t next() {
        uint64_t x = state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state = x;
        return x * 2685821657736338717ull;
    }

    // Next unsigned int in [0, max)
    uint32_t nextUInt(uint32_t max) {
        return static_cast<uint32_t>(next() % (max ? max : 1u));
    }

    // 내부 상태를 그대로 꺼낸다 — 상태 해시에 섞어 넣기 위해서다.
    uint64_t getState() const { return state; }

private:
    uint64_t state;
};
```

이 구현에는 상태 크기와 플랫폼 간 재현성을 위해 자체 RNG를 택한 이유가 함께 드러난다.

**첫째, 상태가 `uint64_t` 하나다.** 마지막 줄의 `getState()` 가 그 결과다. §9 에서 만들 상태 해시는 RNG 를 포함해야 하는데 — 포함하지 않으면 두 클라이언트의 블록 순서가 갈라져도 해시가 같아진다 — `mt19937` 이라면 624워드 내부 배열과 인덱스를 전부 긁어와야 한다. 여기서는 8바이트를 그대로 해시 체인에 흘려보내면 끝이다. 리플레이 스냅샷도 마찬가지다.

**둘째, 분포가 `next() % max` 한 줄이라 눈에 보인다.** 모듈로 편향이 있다는 것을 알면서도 그대로 둔 선택이다. 이 프로젝트에서 `nextUInt`에 들어가는 `max`는 bag의 남은 원소 수와 가비지 홀 컬럼 수처럼 작은 값뿐이다. 편향은 게임에서 관측하기 어려운 수준이다. 중요한 것은 같은 고정 폭 정수 연산이 모든 플랫폼에서 같은 **결과 비트열**을 만든다는 사실이다. 기계어 자체는 컴파일러와 CPU마다 달라도 된다.

생성자의 `seed ? seed : 88172645463393265ull`도 필수다. xorshift는 상태 0에 갇히면 영원히 0만 뱉는다. 이 구현에서 시드 0을 그대로 받으면 `nextUInt()`가 매번 남은 bag의 첫 원소를 골라 무작위성이 사라지고, 같은 고정 조각 순서가 가방마다 반복된다.


### 8.2 XorShift64* 알고리즘

Marsaglia(2003)가 제안한 xorshift 계열 RNG의 변형이다. 세 번의 XOR-shift 연산 후 곱셈으로 출력을 혼합한다:

$$x \leftarrow x \oplus (x \gg 12)$$ $$x \leftarrow x \oplus (x \ll 25)$$ $$x \leftarrow x \oplus (x \gg 27)$$ $$\text{output} = x \times 2685821657736338717$$

이 구현은 shift 상수 `(12, 25, 27)`과 곱셈 상수 `2685821657736338717`을 쓰는 xorshift64* 변형이다. 내부 상태 전이는 0이 아닌 상태에서 긴 주기를 만들고, 마지막 곱셈은 출력 비트를 섞는다. 이 성질은 게임용 결정론적 난수에는 충분하지만 암호학적 예측 저항성을 제공하지 않는다.

특성:
- **상태 크기**: 64비트 (8바이트). Mersenne Twister의 2,496바이트 대비 극소
- **주기**: $2^{64} - 1 \approx 1.8 \times 10^{19}$. 테트리스 게임에서 사용하기에 충분
- **속도**: 단일 uint64 변수에 대한 비트 연산 3회 + 곱셈 1회. 캐시 친화적

### 8.3 분포 함수의 결정론

`nextUInt(max)`는 단순히 `next() % max`를 반환한다. 이 방식에는 **모듈러 편향(modulo bias)** 이 있다: `max`가 $2^{64}$의 약수가 아니면, 일부 값이 다른 값보다 미세하게 더 자주 나온다.

예: `next() % 7`에서, $\lfloor 2^{64} / 7 \rfloor = 2635249153387078802$이고 나머지 $2^{64} \mod 7 = 2$이므로, 값 0과 1이 다른 값보다 $1/(2^{64}/7) \approx 4 \times 10^{-19}$ 만큼 더 자주 나온다.

이 편향은 테트리스에서 무시할 수 있는 수준이다. 그러나 암호학적 용도에는 부적합하다.

> **레퍼런스:** George Marsaglia, "Xorshift RNGs" (2003, Journal of Statistical Software, Vol 8, Issue 14). 또한 Sebastiano Vigna, "An experimental exploration of Marsaglia's xorshift generators, scrambled" (2016) — xorshift64*의 통계적 분석.

---

## 9. 상태 해시 (FNV-1a 64-bit)

### 9.1 목적

네트워크 멀티플레이에서 양쪽 피어의 시뮬레이션이 동일한지 검증해야 한다. 매 틱마다 전체 게임 상태(800바이트 그리드 + 블록 상태 + RNG + 점수)를 전송하는 것은 비효율적이다. 대신, **64비트 해시**를 계산해서 교환한다. 해시가 일치하면 상태가 동일하다고 간주한다.

### 9.2 FNV-1a 알고리즘

FNV-1a는 비암호학적 해시 함수로, 단순하고 빠르다:

$$h_0 = 14695981039346656037$$ $$h_i = (h_{i-1} \oplus \text{byte}_i) \times 1099511628211$$

**현재 소스 발췌 — `core/hash.h`**

```cpp
inline uint64_t fnv1a64_value(const T& v, uint64_t seed = 14695981039346656037ull) {
    return fnv1a64(&v, sizeof(T), seed);
}
```

초기값 $14695981039346656037$은 FNV offset basis, 곱셈 상수 $1099511628211$은 FNV prime이다. 이 두 상수는 Fowler, Noll, Vo가 64비트 해시에 대해 경험적으로 최적화한 값이다.

### 9.3 상태 해시 구성

전체 `StateHash()` 구현은 다음과 같다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
uint64_t SimGame::StateHash() const
{
    uint64_t h = 14695981039346656037ull;
    // Grid bytes — layout must match old Grid::grid exactly.
    h = fnv1a64(&sim_grid.grid[0][0], sizeof(sim_grid.grid), h);
    // Current block state
    h = fnv1a64_value(currentBlock.id, h);
    int curRot = currentBlock.GetRotationState();
    int curRow = currentBlock.GetRowOffset();
    int curCol = currentBlock.GetColumnOffset();
    h = fnv1a64_value(curRot, h);
    h = fnv1a64_value(curRow, h);
    h = fnv1a64_value(curCol, h);
    // Next preview queue state
    h = fnv1a64_value(static_cast<int>(nextBlocks.size()), h);
    for (const SimBlock& next : nextBlocks)
    {
        h = fnv1a64_value(next.id, h);
        h = fnv1a64_value(next.GetRotationState(), h);
        h = fnv1a64_value(next.GetRowOffset(), h);
        h = fnv1a64_value(next.GetColumnOffset(), h);
    }
    // RNG / score / flags / gravity
    uint64_t rngState = rng.getState();
    h = fnv1a64_value(rngState, h);
    h = fnv1a64_value(score, h);
    int over = gameOver ? 1 : 0;
    h = fnv1a64_value(over, h);
    h = fnv1a64_value(gravityCounterTicks, h);
    h = fnv1a64_value(dropIntervalTicks, h);
    h = fnv1a64_value(softDropCounterTicks, h);
    h = fnv1a64_value(totalLinesCleared, h);
    h = fnv1a64_value(level, h);
    h = fnv1a64_value(lastMoveWasRotate ? 1 : 0, h);
    // Combat state — 양쪽이 동일한 입력에서 동일한 값을 도출하므로 해시에 포함하면
    // 가비지 로직 버그가 HASH 자동 검증(F.2)에서 즉시 DESYNC 로 잡힌다.
    uint64_t gRng = garbageRng.getState();
    h = fnv1a64_value(gRng, h);
    h = fnv1a64_value(attackLinesSent, h);
    h = fnv1a64_value(pendingGarbage, h);
    return h;
}
```

해시에 포함되는 항목은 다음이다. 그리드처럼 배열 크기가 명확한 항목은 byte 수를 적고, 구조체/컨테이너가 얽힌 항목은 컴파일러·표준 라이브러리·패딩에 따라 `sizeof` 가 달라질 수 있으므로 총합 수치를 문서에 고정하지 않는다.

| 항목 | 크기 | 이유 |
|------|------|------|
| 그리드 전체 | 800 bytes | 블록 배치 상태 |
| 현재 블록 + nextBlocks 큐 | 구현 의존 | 진행 중인 블록과 다음 3개 preview 상태 |
| RNG 상태 (piece-bag) | 8 bytes | 미래 블록 순서 결정 |
| 점수 | 4 bytes | 게임 진행도 |
| 게임오버 플래그 | 4 bytes | 종료 조건 |
| 중력/드롭 타이머 | 12 bytes | 다음 자동 하강/소프트 드롭 시점 |
| 레벨/누적 라인 | 8 bytes | 중력 속도와 점수 배율 |
| T-spin setup 플래그 | 4 bytes | 다음 lock의 T-spin 판정 |
| garbageRng 상태 | 8 bytes | 가비지 홀 위치 결정 |
| attackLinesSent | 4 bytes | 상대에게 보낸 누적 공격 |
| pendingGarbage | 4 bytes | 내가 받을 대기 공격 |

핵심은 비용 숫자가 아니라 **결정론 관련 상태를 빠짐없이 같은 순서로 섞는다**는 점이다. 해시 비용이 의심되면 추정 대신 대상 빌드에서 profiler 로 `StateHash()` 시간을 재야 한다.

### 9.4 충돌 확률

64비트 해시의 생일 역설(birthday paradox)에 의한 충돌 확률:

$$P(\text{collision}) \approx \frac{n^2}{2 \times 2^{64}}$$

$n = 10^6$ (백만 틱)에서: $P \approx \frac{10^{12}}{3.7 \times 10^{19}} \approx 2.7 \times 10^{-8}$.

60Hz 기준 백만 틱은 약 16,667초, 즉 약 4.6시간이다. 이 숫자는 "백만 번 해시를 비교해도 우연 충돌 확률이 매우 낮다"는 감을 주기 위한 계산이지, FNV-1a가 보안 해시라는 뜻은 아니다. 여기서 해시는 악의적 입력 방어가 아니라 디싱크 디버깅용 지문이다.

---

## 10. 공격 라인과 가비지 큐

여기까지 만든 `SimGame`은 싱글 플레이어 테트리스 엔진이다. 멀티플레이어에서는 한 쪽이 라인을 지우면 **상대방 필드 하단에 쓰레기 줄**(가비지)이 밀어올라간다. 이것이 1:1 테트리스의 유일한 상호작용 채널이다.

설계상 중요한 질문이 세 가지 있다:

1. 몇 줄을 지우면 몇 줄을 보내는가? (공격 테이블)
2. 가비지는 언제 상대 필드에 주입되는가? (타이밍)
3. 양쪽 피어가 **같은 칼럼에 구멍을 뚫어야** 한다 — 어떻게 보장하는가? (결정론)

### 10.1 공격 테이블

`attack_lines_for(n, tSpin)` 함수가 "라인 클리어 n줄 → 공격 x줄" 매핑을 결정한다. 일반 클리어와 T-spin은 같은 라인 수라도 공격량이 다르므로 `tSpin` 플래그를 함께 받는다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
static int attack_lines_for(int rowsCleared, bool tSpin)
{
    if (tSpin)
    {
        switch (rowsCleared) {
            case 1: return 2;   // T-spin Single
            case 2: return 4;   // T-spin Double
            case 3: return 6;   // T-spin Triple
            default: return 0;  // T-spin no-line
        }
    }
    switch (rowsCleared) {
        case 2: return 1;   // Double → 1 가비지
        case 3: return 2;   // Triple → 2 가비지
        case 4: return 4;   // Tetris → 4 가비지
        default: return 0;  // Single or none
    }
}
```

| 클리어 라인 | 공격 | 비고 |
|------------|------|------|
| 1줄 (Single) | 0 | 기본 클리어는 공격 없음 |
| 2줄 (Double) | 1 | 난이도 프리미엄 |
| 3줄 (Triple) | 2 | |
| 4줄 (Tetris) | 4 | 최대 효율 |
| T-spin Single | 2 | 회전 기술 보상 |
| T-spin Double | 4 | Tetris와 같은 공격 |
| T-spin Triple | 6 | 최고 공격량 |

Single을 공격에서 제외한 것은 **스팸 방지**다. 플레이어가 한 줄씩 반복 클리어하는 것보다 4줄을 모아 한 번에 터뜨리는 전략을 강제한다.

T-spin은 별도 판정으로 다룬다. 마지막 성공 이동이 회전이고, T-piece pivot 주변 네 모서리 중 3개 이상이 벽이나 기존 블록으로 막히면 T-spin이다. 이 판정은 `LockBlock()` 시작 시점, 현재 블록이 preview 큐 첫 블록으로 교체되기 전에 실행한다.

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
bool SimGame::IsTSpinLock() const
{
    if (currentBlock.id != 6 || !lastMoveWasRotate) return false;

    const int pivotRow = currentBlock.rowOffset + 1;
    const int pivotCol = currentBlock.columnOffset + 1;
    const int corners[4][2] = {
        {pivotRow - 1, pivotCol - 1},
        {pivotRow - 1, pivotCol + 1},
        {pivotRow + 1, pivotCol - 1},
        {pivotRow + 1, pivotCol + 1},
    };

    int blocked = 0;
    for (const auto& corner : corners)
    {
        const int row = corner[0];
        const int col = corner[1];
        if (sim_grid.IsCellOutside(row, col) || !sim_grid.IsCellEmpty(row, col))
        {
            blocked++;
        }
    }
    return blocked >= 3;
}
```

### 10.2 공격 누적과 전달

`SimGame`은 공격을 직접 상대에게 전송하지 않는다. 대신 **누적 카운터** `attackLinesSent`에 쌓아둘 뿐이다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::LockBlock()
{
    const bool tSpin = IsTSpinLock();
    std::vector<Position> tiles = currentBlock.GetCellPositions();
    for (const Position& item : tiles)
    {
        sim_grid.grid[item.row][item.column] = currentBlock.id;
    }
    currentBlock = NextBlock();
    ghostBlock = MakeGhostBlock(currentBlock);
    bool wasGameOver = gameOver;
    if (BlockFits(currentBlock) == false)
    {
        gameOver = true;
    }

    nextBlocks.erase(nextBlocks.begin());
    nextBlocks.push_back(GetRandomBlock());
    int rowsCleared = sim_grid.ClearFullRows();
    lastLinesCleared = rowsCleared;
    lastTSpinLines = tSpin ? rowsCleared : -1;
    if (rowsCleared > 0 || tSpin)
    {
        if (rowsCleared > 0) clearSoundEvent = true;
        UpdateScore(rowsCleared, 0, tSpin);
        attackLinesSent += attack_lines_for(rowsCleared, tSpin);
    }
    lastMoveWasRotate = false;

    // 가비지 주입 — 라인 클리어 적용 후, 다음 피스가 확정된 이 시점에서 하단으로 올라온다.
    // 주의: 클리어 없이 그냥 놓은 경우에도 pendingGarbage 가 있으면 받는다.
    int inserted = 0;
    if (pendingGarbage > 0 && !gameOver)
    {
        inserted = pendingGarbage;
        InsertGarbage(pendingGarbage);
        pendingGarbage = 0;
        // 가비지가 올라와 currentBlock 스폰 위치를 막았으면 topout.
        if (!BlockFits(currentBlock)) gameOver = true;
    }
    lastGarbageReceived = inserted;
    if (inserted > 0) garbageSoundEvent = true;

    if (gameOver && !wasGameOver) gameOverEvent = true;
}
```

외부(네트 레이어)는 매 틱 `AttackLinesSent()`를 폴링하고, 이전 틱 대비 **델타**를 뽑아 상대 SimGame의 `AddPendingGarbage()`로 전달한다. 접근자와 전달자는 다음과 같다:

**현재 소스 발췌 — `src/sim_game.h`**

```cpp
    int AttackLinesSent() const { return attackLinesSent; }
    int PendingGarbage() const { return pendingGarbage; }
    void AddPendingGarbage(int rows) { if (rows > 0) pendingGarbage += rows; }
```

```mermaid
graph TB
    subgraph PeerA["피어 A (내 SimGame)"]
        A1["4줄 클리어<br/>attackLinesSent<br/>→ 14 (+4)"]
        A2["외부: 델타 4 감지<br/>(local sim 기준)"]
        A3["상대 B 쪽에<br/>AddPendingGarbage(4)"]
        A1 --> A2 --> A3
    end

    subgraph PeerB["피어 B (상대 SimGame)"]
        B1["같은 입력 수신<br/>(네트 동기화)"]
        B2["자기 시뮬에서<br/>동일한 delta=4 도출"]
        B3["AddPendingGarbage(4)<br/>pendingGarbage=4"]
        B1 --> B2 --> B3
    end

    A2 -. 입력이 네트로 동기화 .-> B2
```

여기서 주목할 점은 **네트워크로 "공격 보냄" 이벤트를 별도로 전송하지 않는다**는 것이다. 양쪽이 같은 입력으로 같은 시뮬을 돌리면, A가 4줄을 지웠다는 사실을 B쪽 시뮬레이션도 자기 눈으로 본다 — A의 상대편 뷰는 어차피 B가 돌리는 시뮬과 동일하기 때문이다. 공격은 입력의 **함수**이지 별도 메시지가 아니다.

이 설계가 네트 프레임 포맷을 단순하게 유지한다. Part 6에서 보듯 와이어 프로토콜에는 `INPUT` 타입 하나만 있고, 가비지/공격 관련 메시지 타입은 존재하지 않는다.

### 10.3 가비지 주입 타이밍

대기 중인 가비지(`pendingGarbage`)는 **다음 `LockBlock` 시점에** 필드 하단으로 밀려 올라온다. 지금 떨어지고 있는 피스가 락되기 전까지는 주입되지 않는다 — 플레이어가 예측 불가능한 중간 주입으로 게임을 망치는 것을 막는다.

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
    int inserted = 0;
    if (pendingGarbage > 0 && !gameOver)
    {
        inserted = pendingGarbage;
        InsertGarbage(pendingGarbage);
        pendingGarbage = 0;
        // 가비지가 올라와 currentBlock 스폰 위치를 막았으면 topout.
        if (!BlockFits(currentBlock)) gameOver = true;
    }
    lastGarbageReceived = inserted;
    if (inserted > 0) garbageSoundEvent = true;

    if (gameOver && !wasGameOver) gameOverEvent = true;
```

주목할 상세 사항:

- **클리어 여부와 무관**: 그냥 땅에 붙인 피스라도 대기 가비지가 있으면 주입된다.
- **주입 후 스폰 검사**: 가비지가 올라오면 맨 위 행이 밀려나고 새 피스가 이미 스폰된 위치에 가비지가 겹칠 수 있다. 이때 `BlockFits(currentBlock)` 실패 → `gameOver = true` (이른바 "top-out").
- **gameOverEvent 플래그**: "이 틱에서 gameOver로 전이했다"를 1회만 표시. 렌더러가 게임오버 애니메이션/사운드를 트리거하는 계기.

### 10.4 InsertGarbage 내부

주입 로직은 3단계로 되어 있다. 기존 행을 위로 밀어올리고 → 하단에 가비지 행을 채우고 → 하나의 칼럼을 "구멍"으로 비운다.

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::InsertGarbage(int rows)
{
    if (rows <= 0) return;
    if (rows > SimGrid::kRows) rows = SimGrid::kRows;

    // 기존 행을 위로 밀어올린다 — 상단 rows 만큼은 소실 (오버플로우는 게임오버 처리).
    for (int r = 0; r + rows < SimGrid::kRows; r++)
    {
        for (int c = 0; c < SimGrid::kCols; c++)
        {
            sim_grid.grid[r][c] = sim_grid.grid[r + rows][c];
        }
    }
    // 하단 rows 행은 가비지 (id=9, 홀 1개). 한 공격 묶음은 동일 홀 컬럼 공유.
    int hole = static_cast<int>(garbageRng.nextUInt(SimGrid::kCols));
    for (int i = 0; i < rows; i++)
    {
        int gr = SimGrid::kRows - 1 - i;
        for (int c = 0; c < SimGrid::kCols; c++)
        {
            sim_grid.grid[gr][c] = (c == hole) ? 0 : 9;
        }
    }
}
```

결과 시각화 (3줄 가비지, 구멍 = 컬럼 4):

```text
    주입 전(before)          주입 후(after)
row 0:  ..........             row 0:  ..........   <- 옛 row 3
row 1:  ..........             row 1:  ..........   <- 옛 row 4
...                            ...
row 14: ..........             row 14: ..........   <- 옛 row 17
row 15: ..........             row 15: ..■■......   <- 옛 row 18
row 16: ..........             row 16: .■■■■.....   <- 옛 row 19
row 17: ..........             row 17: 9999.99999   <- 가비지, c=4 가 홀
row 18: ..■■......             row 18: 9999.99999
row 19: .■■■■.....             row 19: 9999.99999
```

**한 공격 묶음은 같은 홀 컬럼을 공유한다.** 4줄짜리 공격이 오면 4개 모두 같은 c=4에 구멍이 난다. 이 결정은 플레이어 측면에서 "한 번에 모든 가비지를 같은 I-블록으로 치울 수 있음"을 뜻한다 — 즉, Tetris를 맞은 쪽도 Tetris로 갚을 수 있는 구조.

반대 극단으로 각 줄마다 홀을 다시 뽑을 수도 있다. 이 프로젝트는 한 공격을 하나의 덩어리로 읽을 수 있고 대응 가능한 통로가 남도록 묶음 단위 고정 홀을 선택했다. 다른 정책을 택하면 밸런스뿐 아니라 `garbageRng` 소비 횟수와 상태 해시도 함께 달라진다.

### 10.5 가비지 결정론 — 왜 별도의 RNG 스트림인가

여기서 크리티컬한 결정: **가비지 홀 컬럼을 뽑을 때 쓰는 RNG**는 피스 가방용 `rng`와 **별개 인스턴스** `garbageRng`다.

**현재 소스 발췌 — `src/sim_game.h`**

```cpp
    XorShift64Star garbageRng;
```

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
SimGame::SimGame(uint64_t seed)
    : gameOver(false),
      score(0),
      rng(seed ? seed : 0xC0FFEE123456789ull),
      // splitmix-style fork: 시드와 상호 상관관계가 약한 별도 스트림.
      garbageRng((seed ? seed : 0xC0FFEE123456789ull) ^ 0x9E3779B97F4A7C15ull),
```

`0x9E3779B97F4A7C15` 은 황금비 상수다. $2^{64} / \phi$ 를 반올림한 값이고 splitmix64 가 상태 증분으로 쓰는 것과 같다. 비트 패턴이 고르게 흩어져 있어 (64비트 중 1이 38개) XOR 하면 시드의 거의 모든 비트 위치가 뒤집힌다.

다만 여기서 **과장하지 않는 것이 중요하다.** 자주 보는 설명 두 가지가 틀렸다.

첫째, "어떤 시드를 넣어도 해밍 거리가 일정하게 유지된다" 는 서술은 XOR 의 성질을 잘못 옮긴 것이다. `a ^ b` 와 `a` 의 해밍 거리는 언제나 `popcount(b)` 이고, 여기서는 시드와 무관하게 **항상 정확히 38**이다. 그건 이 상수의 특별한 성질이 아니라 XOR 의 정의다.

둘째, 이렇게 만든 두 스트림이 **통계적으로 독립이라는 보장은 없다.** splitmix64 는 상수를 **더한 뒤 mix 함수(shift-xor-multiply 3단)를 통과**시켜 상태를 분리하지만, 여기서는 XOR 한 번이 전부다. 두 `XorShift64Star` 는 상태 공간의 서로 다른 점에서 출발할 뿐이며, 한쪽이 다른 쪽의 궤도 위 어딘가에 있을 가능성을 배제하지 못한다. xorshift64* 의 주기가 $2^{64}-1$ 이고 한 게임에서 소비하는 난수가 수천 개 수준이라, 두 궤도가 실제로 겹칠 확률이 무시할 만할 뿐이다.

정직한 근거는 이것이다: **두 스트림이 서로 다른 상태에서 출발하고, 그 거리가 게임 한 판의 소비량보다 압도적으로 멀다.** 더 강한 보장이 필요하면 XOR 대신 splitmix64 의 mix 함수를 한 번 통과시키면 된다 — 비용은 곱셈 두 번과 shift 세 번이고, `getState()` 로 해시에 넣는 방식은 그대로다.

**왜 스트림을 분리하는가?** 피스 RNG와 가비지 RNG가 **같은 인스턴스**였다면:

```text
상상의 시나리오 (실제로는 이렇게 하지 않음):
  Tick 100: 내가 4줄 클리어 → attack=4 → 상대 pendingGarbage=4
  Tick 110: 상대가 2줄 클리어 → attack=1 → 내 pendingGarbage=1
  Tick 150: 내가 LockBlock
      → 내 가비지 주입 → nextUInt(10) 호출 → 내 rng 상태 진행
      → 그리고 내 GetRandomBlock() → 내 rng 상태 또 진행
  Tick 151: 상대가 LockBlock
      → 상대 가비지 주입 → 상대 rng.nextUInt(10) 호출
      ...
```

만약 양쪽이 가비지를 **다른 시점에** 받는다면, 동일 RNG 인스턴스에서는 nextUInt 호출 순서가 달라진다. 그러면 다음 피스의 번호도 달라진다 — **블록 순서가 어긋나 완전한 desync**.

`garbageRng`를 별도로 두면, 피스 가방 RNG는 입력 순서와 가비지 타이밍에 전혀 영향받지 않는다. 가비지 RNG도 반대로 독립적이다.

해시 결정론을 보장하기 위해 `garbageRng.getState()`도 `StateHash()`에 포함시킨다는 점을 기억하자 (9.3 섹션 참조). 양쪽 피어의 garbageRng가 갈라지면 HASH 메시지에서 즉시 DESYNC 감지된다.

### 10.6 이벤트 플래그 패턴

`SimGame`은 렌더/오디오 레이어와 한 방향으로만 통신한다: 이벤트 플래그를 세팅하고, 외부가 소비한다.

**현재 소스 발췌 — `src/sim_game.h`**

```cpp
public:
    static constexpr int kNextPreviewCount = 3;

    explicit SimGame(uint64_t seed = 0);

    // ---- Placement-level action API (for RL training) ----
    struct Placement
    {
        int col;
        int rot;
    };
```

각 플래그의 라이프사이클:

1. **세팅**: `LockBlock`/`RotateBlockImpl` 등 sim 내부 이벤트 발생 시 세팅.
2. **관찰**: 렌더러가 매 프레임(또는 매 틱) `sim.lastLinesCleared` 등을 읽어 애니메이션을 실행.
3. **클리어**: 관찰자가 명시적으로 0/false로 리셋. sim은 스스로 클리어하지 않는다.

`mutable`로 선언된 것은 `const SimGame&` 참조에서도 클리어할 수 있게 하기 위함이다. 예: 렌더러가 `const SimGame& sim` 만 받아도 `sim.rotateSoundEvent = false`로 리셋할 수 있다.

**왜 외부가 클리어하는가?** 만약 sim이 "다음 틱이 시작될 때" 자동으로 클리어한다면, 렌더러가 그 이벤트를 놓칠 수 있다. 60Hz 게임에서 이벤트 발생 틱(t)과 렌더 프레임(t+1)이 정확히 일치하지 않을 수 있기 때문이다. 소비 쪽이 책임지면 "본 적 없음" 상태가 남지 않는다.

```mermaid
sequenceDiagram
    participant Sim as SimGame
    participant R as 렌더러

    Note over Sim: sim.LockBlock()
    Sim->>Sim: lastLinesCleared=4<br/>clearSoundEvent=true<br/>lastGarbageReceived=0

    Note over R: 렌더 프레임 N
    Sim-->>R: 읽음: lastLinesCleared=4
    R->>R: 쉐이크 + 사운드 재생
    R->>Sim: 플래그 클리어<br/>clearSoundEvent=false<br/>lastLinesCleared=0

    Note over R: 렌더 프레임 N+1
    Sim-->>R: 관찰: 0
    R->>R: 아무 일도 없음
```

### 10.7 왜 sim 내부에 이벤트 플래그를 두는가

"렌더러가 sim 상태를 **비교**해서 이벤트를 추론할 수 있잖아?"라는 질문이 자연스럽다. 예: 이전 틱의 `score`와 현재 `score`를 비교해서 라인 클리어를 감지.

그러나 이 접근에는 세 가지 문제가 있다.

**1. 정보 손실.** `score` 델타가 300이면 Double인지 "Single + 다른 이유"인지 모호하다. `lastLinesCleared` 같은 전용 플래그가 있으면 애매함이 없다.

**2. 렌더러가 히스토리를 유지해야 한다.** 이전 틱의 상태를 저장해두고 비교해야 한다. sim이 플래그를 세팅하면 렌더러는 무상태(stateless)로 동작할 수 있다 — "이 틱에 뭐가 일어났지?"만 질문하면 된다.

**3. 가장 중요: 결정론 방향성.** 이벤트 감지 로직이 렌더러에 있으면, 언젠가 누군가 "아, 이 이펙트에 특수 케이스가 필요해" 하면서 sim 상태를 약간 바꾸려고 할 것이다. 예: "O-블록이 쉐이크를 덜 강하게 일으키게 하려고 sim.lastBlockId도 노출하자" → 곧이어 "여기 currentBlock을 살짝 보정하면 더 자연스러워지지 않을까?" → **결정론 파괴**.

sim 내부에서 플래그를 세팅하고 외부는 읽기만 하면, 렌더러가 sim을 수정할 이유가 없다. 코드 리뷰에서 `sim.xxx =` 형태 할당이 렌더 디렉터리에 나타나면 즉시 레드 플래그다.

---

## 11. 결정론 유지의 규칙

여기까지 `SimGame`은 수많은 결정론 규칙에 의존한다. 이 섹션은 그 규칙들을 한곳에 정리한다. 이 규칙들은 암묵적이어서는 안 된다 — 누구든 이 코드를 수정하는 사람이 **명시적으로** 지켜야 하는 계약이다.

### 11.1 불변 조건: 입력과 시드만이 상태를 결정한다

단일한 최고 원칙:

> 임의의 두 `SimGame` 인스턴스가 동일한 시드로 시작되어 동일한 `SubmitInput(mask)` / `Tick()` / `AddPendingGarbage(n)` 시퀀스를 받으면, 매 호출 후의 `StateHash()`는 비트 단위로 일치해야 한다.

이 조건은 모든 플랫폼(Windows MSVC, Linux GCC, Colab), 모든 컴파일러 버전, 모든 최적화 레벨에서 성립해야 한다.

이 보장을 깨는 경로는 예상외로 많다:

- 부동소수 사용 → 컴파일러별 반올림 차이
- std 라이브러리의 비결정적 구현 (uniform_int_distribution)
- 시스템 콜(`std::chrono::now()`, `rand()`)
- 메모리 주소 의존(`reinterpret_cast<uintptr_t>(&x)`)
- 스레드 순서에 따른 경쟁 조건

아래 11.2~11.7은 이 함정들을 구체적으로 다룬다.

### 11.2 일방향 참조: 외부가 sim을 건드리지 않는다

sim 은 외부 레이어를 모른다. 외부 레이어(렌더/오디오/네트/인풋 수집)는 sim을 **읽기만** 한다.

허용되는 쓰기 경로는 정확히 네 가지:

1. 생성자에서 시드 주입: `SimGame(seed)`
2. 입력 제출: `SubmitInput(mask)`, `Tick()`, `MoveBlockDown()`
3. 가비지 추가: `AddPendingGarbage(rows)`
4. 이벤트 플래그 소비 후 리셋: `sim.rotateSoundEvent = false` 등 (mutable 플래그만)

그 외에는 모두 금지. 특히:

- `sim.score = 0` 금지 — 시나리오가 더 복잡해지면 `score` 도 계산된 값이어야 한다.
- `sim.sim_grid.grid[5][3] = 7` 금지 — 물론 이것은 `sim_grid`가 private 이므로 컴파일러가 막는다.
- `const_cast<SimBlock&>(sim.CurrentBlock())` 금지 — 뒷문을 뚫으려 하면 리뷰에서 거부.

```mermaid
graph TB
    subgraph Sim["SimGame (단일 진리원)"]
        S1["sim_grid<br/>currentBlock"]
        S2["rng / garbageRng<br/>pendingGarbage"]
    end

    Input["입력을 넣는 쪽<br/>지금은 테스트 스크립트"]
    Output["상태를 읽는 쪽<br/>지금은 해시 덤프"]

    Input -- "write: SubmitInput / Tick" --> Sim
    Sim -- "read: Grid() / StateHash()" --> Output
```

입력을 넣는 쪽은 쓰기만 하고, 상태를 읽는 쪽은 읽기만 한다. 두 방향이 한 모듈에서 섞이는 순간 "누가 이 값을 바꿨는지" 를 추적할 수 없게 되고, 그러면 결정론도 무너진다.

이 경계에는 테스트 프로그램뿐 아니라 렌더링 클라이언트, 네트워크 세션, Python 바인딩도 연결된다. 어느 소비자도 `SimGame` 상태를 우회해 건드리지 않으며, 위 네 가지 허용 목록이 모든 연결의 공통 계약이다.

### 11.3 부동소수 금지

`SimGame` 내부에 `float` 또는 `double`은 **한 개도 없다**. 모든 좌표, 카운터, 타이머는 정수다.

검증: `src/sim_*.h`와 `src/sim_game.cpp`에서 `float`/`double`/`.f` 리터럴을 검색하면 결과가 없어야 한다.

이유는 단순하다. IEEE 754 부동소수 연산의 결과는 **명목상** 결정적이지만, 실제로는:

- `-ffast-math` 같은 최적화 플래그가 연산 순서를 재배치
- x87 FPU(80비트 확장 정밀도)와 SSE(64비트) 간 미묘한 차이
- `sin`/`cos`/`pow`는 libm 구현에 따라 ULP 단위로 다른 값을 반환

60Hz 물리에서 $10^{-15}$ 수준의 차이가 수천 틱 누적되면 결국 감지 가능한 분기로 커진다.

예외는 "결정론과 무관한 레이어"에만 허용된다:

- 렌더러(화면 좌표): 부동소수 자유롭게 사용
- 오디오 믹싱: 자유
- 타임스탬프 로깅: 자유

sim 내부는 금지.

중력 타이머도 정수다:

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// src/sim_game.cpp
gravityCounterTicks(0),
dropIntervalTicks(TICKS_PER_SECOND / 2) // default: drop every 0.5s
```

`TICKS_PER_SECOND = 60`이므로 `dropIntervalTicks = 30`. "0.5초"를 직접 표현하지 않고 "30틱"으로 표현한다. 0.5를 `float` 로 두면 `0.5 * 60 = 29.9999...` 같은 경로가 생길 수 있고, 컴파일러가 이를 29 또는 30으로 반올림할 수 있다.

### 11.4 해시에 포함되는 모든 것

`StateHash()`는 상태의 **의미 있는 모든 부분**을 포함해야 한다. 해시에 빠진 상태가 있다면, 그 상태가 다른데 해시가 같은 가짜 "일치"가 발생할 수 있다.

현재 포함되는 상태 (9.3 섹션의 표 참조):

- 그리드 (800 bytes)
- currentBlock의 id/rot/row/col
- nextBlocks preview 큐 전체의 size + 각 id/rot/row/col
- piece RNG state
- garbage RNG state
- score, gameOver 플래그
- gravityCounterTicks, dropIntervalTicks
- attackLinesSent, pendingGarbage

빠뜨리기 쉬운 것들:

- **nextBlocks**: 현재 블록만 해시하면 다음 피스 큐가 달라도 같은 해시. 실제로 양쪽 sim의 preview 큐가 어긋나면 다음 락에서 즉시 분기.
- **gravityCounterTicks**: 중력 카운터가 다르면 "언제 자동 하강할지"가 다르다. 즉시 분기.
- **garbageRng state**: 다음 가비지의 홀 위치가 달라진다.
- **lastLinesCleared / gameOverEvent**: 이것들은 `mutable` 일회성 플래그이고 외부가 클리어하므로 해시에 **일부러 넣지 않는다**. 렌더러의 클리어 타이밍 차이가 해시에 영향을 주면 안 되기 때문이다. 이런 플래그는 시각화 전용 "파생 정보"다.

새로운 상태 필드를 sim에 추가할 때마다 "이것이 해시에 포함되어야 하는가?"를 자문해야 한다. 판단 기준:

> **이 필드가 미래 시뮬레이션 결과에 영향을 주는가?** → 예 → 해시에 포함. **이 필드는 오직 외부(렌더/오디오)의 일회성 피드백용인가?** → 예 → 해시에 불포함.

### 11.5 sim_hash_dump: C++ ↔ Python 결정론 게이트

`tests/sim_hash_dump.cpp`는 결정론 회귀 테스트의 지상 진리원(ground truth)이다. 고정된 입력 스크립트를 여러 시드로 실행하고, 매 스텝의 `StateHash()`를 stdout에 찍는다.

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// tests/sim_hash_dump.cpp — 스크립트 일부
const Step kScript[] = {
    { INPUT_NONE,                           30 },
    { INPUT_LEFT,                            1 },
    { INPUT_LEFT,                            1 },
    { INPUT_LEFT,                            1 },
    { INPUT_ROTATE,                          1 },
    { INPUT_DROP,                            2 },
    // ... (좌/우 이동, 회전, 드롭, 다중 틱 중력을 고루 커버)
};
```

출력 형식:

```text
==== seed 0x0000000000000001 ====
seed=0x0000000000000001
initial_hash=0x<16자리>
step=000 mask=0x00 ticks=30 total_ticks=30 score=0 over=0 hash=0x<16자리>
step=001 mask=0x01 ticks=1 total_ticks=31 score=0 over=0 hash=0x<16자리>
...
final_hash=0x<16자리> final_score=<N> final_over=<0|1>
```

이 출력이 세 가지 게이트로 쓰인다:

1. **리팩토링 패리티**: 구 `Game::ComputeStateHash()`와 신 `SimGame::StateHash()`가 같은 스크립트에서 동일한 해시를 출력. 과거에 이 sim은 raylib 의존성을 제거하기 위해 리팩토링되었는데, 이 게이트가 비트 단위 비호환을 막았다.

2. **크로스 플랫폼 패리티**: Windows (MSVC) 빌드와 Linux (gcc, Colab) 빌드가 동일한 해시를 출력해야 한다. `int` 크기, unsigned modulo, FNV-1a 바이트 순회는 이론상 모두 플랫폼 독립이지만 실제로 검증할 가치가 있다.

3. **CI 회귀**: sim 의미를 바꾸지 **않는** 커밋은 이 덤프를 바꾸지 **않아야** 한다. 해시가 변하면 "이 커밋이 sim semantics를 건드렸다"는 즉각적인 시그널 — 모든 lockstep 피어가 DESYNC될 것.

Python 측 대응이 `python/tests/test_determinism_crossplatform.py`다:

**현재 소스 발췌 — `python/tests/test_determinism_crossplatform.py`**

```python
def _run_script(seed: int) -> list[tuple[int, int, int, bool, int]]:
    """Replay the script on a fresh SimGame and return the per-step state
    tuple ``(step, total_ticks, score, game_over, state_hash)``.

    Mirror of ``run_and_dump`` in ``sim_hash_dump.cpp``.
    """
    from sim import SimGame  # noqa: PLC0415

    sim = SimGame(seed)
    out: list[tuple[int, int, int, bool, int]] = []
    total_ticks = 0
    for step_index, (mask, ticks) in enumerate(SCRIPT):
        sim.submit_input(mask)
        for _ in range(ticks):
            sim.tick()
            total_ticks += 1
        out.append(
            (step_index, total_ticks, sim.score(), sim.game_over(), sim.state_hash())
        )
        if sim.game_over():
            break
    return out


@pytest.mark.skipif(not _have_native(), reason="Native tetris_py not built")
def test_initial_hash_stable_across_seeds() -> None:
    """Sanity check: same seed -> same initial hash on every call."""
    from sim import SimGame  # noqa: PLC0415

    for seed in SEEDS:
        a = SimGame(seed).state_hash()
        b = SimGame(seed).state_hash()
        assert a == b, f"unstable initial hash for seed 0x{seed:016x}"


@pytest.mark.skipif(not _have_native(), reason="Native tetris_py not built")
def test_script_replay_stable() -> None:
    """Determinism: replaying the script twice gives the same hash sequence."""
    for seed in SEEDS:
        a = _run_script(seed)
        b = _run_script(seed)
        assert a == b, f"unstable script replay for seed 0x{seed:016x}"


@pytest.mark.skipif(not REFERENCE_FILE.exists(), reason="No reference dump")
@pytest.mark.skipif(not _have_native(), reason="Native tetris_py not built")
def test_matches_cpp_reference_dump() -> None:
    """Cross-platform parity vs the C++ ``sim_hash_dump`` reference output.

    Parses the captured stdout of the C++ test driver and checks that
    every ``hash=0x...`` line matches the Python replay.
    """
    text = REFERENCE_FILE.read_text(encoding="utf-8")

    # Parse: each "==== seed 0x... ====" block contains lines like
    #   step=000 mask=0x00 ticks=30 total_ticks=30 score=0 over=0 hash=0x...
    blocks = text.split("==== seed ")
    expected_by_seed: dict[int, list[int]] = {}
```

`test_matches_cpp_reference_dump`는 C++ 덤프 파일을 파싱해서 Python에서 재생한 해시와 비교한다. Python 바인딩이 같은 C++ sim을 링크한다는 사실만으로는 충분하지 않다 — 실제로 같은 입력 시퀀스에서 같은 해시가 나오는지 확인해야 결정론 보장이 완결된다.

### 11.6 FNV-1a 32 vs 64, 그리고 Python 마스킹

프로젝트에는 두 가지 FNV-1a가 있다:

- **FNV-1a 64**: sim state hash (`core/hash.h`)
- **FNV-1a 32**: 와이어 프레임 체크섬 (`net/framing.cpp` 및 `python/netbot/framing.py`)

둘의 상수가 다르다:

| 변종 | offset basis | prime |
|------|--------------|-------|
| 32bit | 0x811C9DC5 (2166136261) | 0x01000193 (16777619) |
| 64bit | 14695981039346656037 | 1099511628211 |

C++ 구현은 각각 `uint32_t`, `uint64_t`를 쓰므로 곱셈 오버플로가 **자연스럽게 truncation**된다 — 언어 표준이 unsigned 오버플로를 modulo 산술로 정의하기 때문이다.

Python은 임의 정밀도 정수라 자동 truncation이 **없다**. 그래서 매 단계 수동 마스킹이 필요하다:

**현재 소스 발췌 — `python/netbot/framing.py`**

```python
def fnv1a32(data: bytes, seed: int = FNV1A32_OFFSET) -> int:
    """FNV-1a 32-bit hash. Identical bit pattern to ``net::fnv1a32`` in C++."""
    h = seed & FNV1A32_MASK
    for byte in data:
        h ^= byte
        h = (h * FNV1A32_PRIME) & FNV1A32_MASK
    return h


# --- little-endian 읽기/쓰기 ---

def le_write_u16(buf: bytearray, value: int) -> None:
    buf += struct.pack("<H", value & 0xFFFF)


def le_write_u32(buf: bytearray, value: int) -> None:
    buf += struct.pack("<I", value & 0xFFFFFFFF)


def le_write_u64(buf: bytearray, value: int) -> None:
    buf += struct.pack("<Q", value & 0xFFFFFFFFFFFFFFFF)


def le_read_u16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def le_read_u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def le_read_u64(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


# --- 프레임 만들기와 뜯기 ---

def build_frame(msg_type: MsgType | int, payload: bytes | bytearray) -> bytes:
    """Serialise ``(msg_type, payload)`` into the wire format.

    The result is exactly what ``net::build_frame`` produces in C++ — bytewise
```

`h = (h * FNV1A32_PRIME) & FNV1A32_MASK`의 `& 0xFFFFFFFF`가 없으면 Python은 곱셈 결과를 $2^{32}$ 이상까지 확장해버리고, 몇 바이트만 지나도 C++ 결과와 완전히 다른 값이 나온다.

이 마스킹 누락은 초기 개발에서 실제로 발생한 버그였다. 체크섬이 맞지 않아 프레임이 "조용히 무시"되고, 클라이언트가 HELLO에 응답을 못 받는 상황이 관찰되었다. Wireshark로 캡쳐된 바이트와 `parse_frames` 로그를 대조해서야 원인을 찾아냈다.

교훈: 언어를 이식할 때 정수 오버플로 시맨틱은 맨 먼저 확인해야 한다. C++에서 당연한 것이 Python에서 당연하지 않다.

### 11.7 Non-sim 레이어에서 RNG가 필요하면?

렌더러가 파티클 이펙트를 위해 난수가 필요하다면? 오디오가 랜덤 피치 변조가 필요하다면? 답은 간단: **sim과 무관한 별도의 RNG 인스턴스를 쓴다.**

**예시(실제 저장소에는 없음)**

```cpp
// 예시: 렌더러 내부
struct Renderer
{
    XorShift64Star vfxRng{someTimeBasedSeed};
    // vfxRng는 sim.rng와 완전히 독립. sim의 결정론에 영향 없음.
};
```

이 vfxRng는 시스템 시간, 틱 카운터 등 결정론과 무관한 시드로 초기화해도 된다. 렌더러의 시각 효과가 양쪽 피어에서 "정확히 같을" 필요는 없기 때문이다 (눈으로 봐서 비슷하면 충분).

절대 금지: `sim.rng`를 렌더에서 빌려 쓰는 것. `RngState()` 접근자가 있지만 이것은 오직 **관찰(해시/디버그)** 용이다. `rng`를 소비(`next()` 호출)하면 sim의 다음 블록이 달라져 desync.

---

## 12. 소프트 드롭 레이트 제한 (`softDropCounterTicks`)

### 12.1 문제: 60Hz 에서 60셀/초는 너무 빠르다

여기까지 만든 `SubmitInput` 은 단순한 매핑 테이블이다: 입력 비트가 세팅되어 있으면 해당 동작을 실행한다. 좌/우/회전/하드드롭 자체는 한 틱에 한 번 실행되며, 좌우 키를 오래 누르는 반복 입력은 `main.cpp` 의 DAS/ARR 카운터가 비트마스크를 만들어 준다.

문제는 **소프트 드롭(DOWN 홀드)** 이다. 플레이어는 DOWN 을 누르고 있는 동안 블록이 "부드럽게 가속되어 떨어지기를" 기대한다. 그런데 `SubmitInput` 이 매 틱 호출되고 DOWN 비트가 켜져 있으면 그대로 `MoveBlockDown()` 이 호출된다. 60Hz 고정 스텝(Part 4에서 정의)에서 이것은 **초당 60셀 하강** 을 뜻한다.

빈 20행 그리드에서 초당 60셀은 위에서 아래까지 약 0.33초밖에 걸리지 않는다. 첫 눌림에 즉시 반응하면서도 누르고 있는 동안 좌우 이동과 회전을 섞을 시간을 주려면 반복 하강은 이보다 느려야 한다. 이 판단은 다른 게임의 수치를 복제한 것이 아니라 현재 보드 높이와 60Hz 입력 주기에서 직접 나온다.

게다가 RL 에이전트가 `SubmitInput(INPUT_DOWN)` 을 연속 호출하면, 매 틱 하강이 이루어져 horizontal move/rotate 윈도우가 극도로 좁아진다. RL 입장에서도 소프트 드롭 속도가 적당히 느려야 탐색 공간이 안정적이다.

### 12.2 설계: 카운터 기반 게이팅

초기 충동은 "DOWN 홀드 시 N틱에 1번만 `MoveBlockDown`" 이지만, 단순히 모듈로(`tick % N == 0`) 로 하면 다음 문제가 생긴다:

- **눌림 시점 지연**: 플레이어가 방금 DOWN 을 누른 순간 `tick % N != 0` 이면 최대 N-1 틱의 무반응 구간이 생긴다. 키감이 즉각적이지 않다.
- **전역 tick 의존**: `SimGame` 은 내부 tick 카운터를 따로 가지고 있지 않다. 외부 루프가 `Tick()` 을 몇 번 호출했는지를 참조해야 해서 SimGame 자체의 결정론성이 외부 호출 패턴에 의존하게 된다.

대신 **per-input 카운터** 를 도입한다. 규칙:

1. `softDropCounterTicks` 필드를 `SimGame` 에 추가.
2. `constexpr int kSoftDropIntervalTicks = 3` — 이 값이 "몇 틱마다 1칸 하강할지"를 결정한다.
3. DOWN 이 held 인 틱:
   - 카운터가 0 이하면 **즉시 `MoveBlockDown()`** 하고 카운터를 `kSoftDropIntervalTicks` 로 세팅 (= 다음 하강까지 쿨다운).
   - 카운터가 양수면 1 감소시키고 하강은 스킵.
4. DOWN 이 떼진 틱: 카운터를 0 으로 리셋 → 다음에 DOWN 을 다시 누르면 첫 프레임에 바로 반응.

이 구조는 네 가지 특성을 동시에 만족한다:

| 특성 | 달성 방식 |
|------|----------|
| 첫 눌림 즉시 반응 | 떼면 카운터=0, 다음 눌림의 첫 틱에 `0 <= 0` 이므로 바로 하강 |
| 일정한 반복 주기 | 하강 직후 카운터=N, N 번 감소해야 다시 0 도달 |
| 외부 tick 독립 | 카운터는 SimGame 내부 상태. 외부가 몇 틱 간격으로 부르든 본인 상태만 본다 |
| 결정론 | 카운터 변화는 오직 `SubmitInput(mask)` 에서만 일어남. 같은 mask 시퀀스 = 같은 카운터 |

### 12.3 튜닝 표

`kSoftDropIntervalTicks` 값 하나로 속도가 결정된다. 60Hz 기준 환산:

| counter | 실제 이동 간격 | 초당 하강 | 평가 |
|----------|----------------|----------|------|
| 1 | 2 ticks | 30 cells/s | 매우 빠름 |
| 2 | 3 ticks | 20 cells/s | 빠른 템포 플레이어용 |
| **3** | **4 ticks** | **15 cells/s** | **현재 값. 컨트롤 가능한 속도** |
| 4 | 5 ticks | 12 cells/s | 클래식 감각 |
| 5 | 6 ticks | 10 cells/s | 느림 |
| 6 | 7 ticks | ~8.6 cells/s | 초보자용 |

3은 플레이 테스트에서 "중력 기본값(0.5초에 1셀 = 2셀/초) 대비 충분히 빠르지만, 연속 입력이 과하게 미끄러지지 않는다" 는 기준으로 선택되었다. 초당 60셀 → 15셀로 낮추면 체감 컨트롤 난이도가 크게 개선된다.

### 12.4 구현: `SubmitInput` 전/후

**Before (단순 매핑, 소프트 드롭 60셀/초):**

**예시(실제 저장소에는 없음)**

```cpp
// 예시(이전 상태 — 현재 저장소에는 없음)
void SimGame::SubmitInput(uint8_t inputMask)
{
    if (gameOver) return;
    if (hasInput(inputMask, INPUT_LEFT))   MoveBlockLeft();
    if (hasInput(inputMask, INPUT_RIGHT))  MoveBlockRight();
    if (hasInput(inputMask, INPUT_DOWN))   MoveBlockDown();  // 매 틱 → 60셀/초
    if (hasInput(inputMask, INPUT_ROTATE)) RotateBlockImpl();
    if (hasInput(inputMask, INPUT_DROP))   MoveBlockDrop();
    DropExpectation();
}
```

**After (카운터 게이팅, 실제 저장소 코드):**

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::SubmitInput(uint8_t inputMask)
{
    if (gameOver) return;

    if (hasInput(inputMask, INPUT_LEFT))   MoveBlockLeft();
    if (hasInput(inputMask, INPUT_RIGHT))  MoveBlockRight();

    // 소프트 드롭: 매 틱 호출되면 60셀/초(너무 빠름). N틱마다 1회로 제한.
    //   최초 눌림(카운터=0) 은 즉시 반응, 그 다음부터 kSoftDropIntervalTicks
    //   (=3, 60Hz → 약 15셀/초) 간격. 뗐다가 다시 눌러도 즉시.
    //   결정론: 이 카운터는 상태 해시에 포함되므로 양쪽 클라이언트 동일 전개.
    constexpr int kSoftDropIntervalTicks = 3;
    if (hasInput(inputMask, INPUT_DOWN)) {
        if (softDropCounterTicks <= 0) {
            MoveBlockDown();
            softDropCounterTicks = kSoftDropIntervalTicks;
        } else {
            softDropCounterTicks--;
        }
    } else {
        softDropCounterTicks = 0;
    }

    if (hasInput(inputMask, INPUT_ROTATE)) RotateBlockImpl();
    if (hasInput(inputMask, INPUT_DROP))   MoveBlockDrop();

    DropExpectation();
}
```

한 틱의 흐름을 그림으로:

```mermaid
graph TB
    A[SubmitInput mask 도착] --> B{INPUT_LEFT?}
    B -->|yes| B2[MoveBlockLeft]
    B -->|no| C{INPUT_RIGHT?}
    B2 --> C
    C -->|yes| C2[MoveBlockRight]
    C -->|no| D{INPUT_DOWN held?}
    C2 --> D
    D -->|yes| E{softDropCounterTicks<br/>&lt;= 0 ?}
    D -->|no| F[softDropCounterTicks = 0<br/>뗌 리셋]
    E -->|yes 즉시 하강| G[MoveBlockDown<br/>softDropCounterTicks = 3]
    E -->|no 쿨다운 중| H[softDropCounterTicks--]
    F --> I{INPUT_ROTATE?}
    G --> I
    H --> I
    I -->|yes| I2[RotateBlock]
    I -->|no| J{INPUT_DROP?}
    I2 --> J
    J -->|yes| J2[MoveBlockDrop<br/>하드드롭 + LockBlock]
    J -->|no| K[DropExpectation<br/>고스트 갱신]
    J2 --> K
```

핵심 관찰:

- `softDropCounterTicks` 는 DOWN 브랜치 내부에서만 증감한다. LEFT/RIGHT/ROTATE/DROP 분기는 이 값을 건드리지 않는다. 동시 입력(예: LEFT + DOWN) 도 정상 동작: LEFT 는 항상 반응, DOWN 은 카운터 규칙대로.
- DOWN 을 떼는 순간 카운터가 0 으로 초기화되므로, 짧게 여러 번 탭하는 플레이 스타일도 매 탭이 첫 프레임에 반응한다 ("탭 드롭").
- 하드드롭(`INPUT_DROP`) 은 별도 경로이고 카운터와 무관하다. 하드드롭은 한 번의 피스 고정이라 반복 속도 자체가 의미 없다.

### 12.5 결정론: 왜 `softDropCounterTicks` 가 `StateHash` 에 들어가는가

이것은 **중요한 포인트** 이다. 언뜻 보면 `softDropCounterTicks` 는 "입력 관련 임시 카운터" 이므로 해시에 포함할 필요가 없어 보인다. 이미 같은 입력 시퀀스면 양쪽이 동일하지 않은가?

맞다 — 하지만 **그래서 포함해야 한다**. 두 가지 이유:

**1. 자체 검증**: F.2 자동 HASH 검증(Part 6) 은 10초마다 양쪽 해시를 비교한다. 만약 어떤 버그(예: DOWN 엣지 검출 오류, 카운터 초기화 누락) 로 한쪽만 카운터가 꼬이면, 다음 DOWN 입력부터 하강 타이밍이 1~2틱 어긋나고 → 피스가 다른 열에 락 → 그리드 해시 분기. 하지만 그리드가 갈라진 이후에야 잡히는 것보다, **카운터 자체를 해시에 넣어두면** 타이밍이 어긋나는 바로 그 틱에 감지된다.

**2. 저비용 보험**: `softDropCounterTicks` 는 `int` 1개. FNV-1a 에 4바이트 추가 섞는 비용은 무시 가능하다. "결정론 관련 내부 상태는 전부 해시에 넣는다"는 규칙을 일관되게 적용하면 나중에 "이건 넣을지 말지" 고민할 필요가 없다.

실제 `StateHash()` 구현에서 해당 라인을 보면:

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// src/sim_game.cpp — StateHash() 내부 발췌
h = fnv1a64_value(gravityCounterTicks, h);
h = fnv1a64_value(dropIntervalTicks, h);
h = fnv1a64_value(softDropCounterTicks, h);   // ← 이 한 줄
```

이 한 줄이 있어, 만약 플랫폼별 `SubmitInput` 호출 순서가 미묘하게 달라지거나 (예: 네트워크 피어와 로컬 피어가 DOWN 입력을 다른 틱에 처리) 카운터가 어긋나는 순간 DESYNC 배너가 뜬다.

### 12.6 생성자 초기화: in-class initializer 에 의존하지 않기

`SimGame` 헤더에서 `softDropCounterTicks` 는 in-class initializer 를 갖는다:

**현재 소스 발췌 — `src/sim_game.h`**

```cpp
    int softDropCounterTicks = 0;
```

이것만으로 충분해 보인다. 그러나 `sim_game.cpp` 생성자는 **mem-init list 에 명시적으로 한 번 더** 초기화한다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
SimGame::SimGame(uint64_t seed)
    : gameOver(false),
      score(0),
      rng(seed ? seed : 0xC0FFEE123456789ull),
      // splitmix-style fork: 시드와 상호 상관관계가 약한 별도 스트림.
      garbageRng((seed ? seed : 0xC0FFEE123456789ull) ^ 0x9E3779B97F4A7C15ull),
      gravityCounterTicks(0),
      dropIntervalTicks(TICKS_PER_SECOND / 2), // default: drop every 0.5s
      // in-class initializer 에 의존하지 않고 명시 — StateHash 포함 필드들의
      // 결정론 보장을 위해 생성자 시점에 확정.
      softDropCounterTicks(0),
      lastMoveWasRotate(false),
      attackLinesSent(0),
      pendingGarbage(0)
{
    blocks = GetAllBlocks();
    currentBlock = GetRandomBlock();
    nextBlocks.reserve(kNextPreviewCount);
    for (int i = 0; i < kNextPreviewCount; ++i)
    {
        nextBlocks.push_back(GetRandomBlock());
    }
    ghostBlock = MakeGhostBlock(currentBlock);
    // sim_grid is zero-initialized by its default constructor.
}
```

왜 이중 초기화인가? 세 가지 이유가 얽혀 있다:

- **컴파일러 별 일관성**: C++ 표준은 mem-init list 에 없는 멤버에 대해 in-class initializer 를 사용하라고 규정하지만, MSVC/Clang/GCC 가 mem-init list 와 in-class initializer 가 혼재할 때 경고를 다르게 내놓는다. 명시적으로 모두 mem-init list 에 올리면 이 "언어 변호사" 영역을 아예 피한다.
- **순서 가시성**: mem-init list 의 순서는 **초기화 순서와 무관** (멤버 선언 순서를 따른다) 하지만, 인간 독자에게는 "이 생성자가 책임지는 필드의 전체 목록" 이 한눈에 보인다. 새로 멤버를 추가했는데 초기화를 깜빡한 실수를 코드 리뷰에서 잡기 쉽다.
- **해시 오염 차단**: 만약 어떤 필드가 uninitialized 상태(stack garbage 또는 heap garbage) 로 남으면, `StateHash()` 가 그 쓰레기를 읽어 해시에 섞는다. 시드 동일한 두 인스턴스라도 OS/할당 패턴에 따라 해시가 달라진다 — 가장 추적하기 어려운 종류의 DESYNC. `attackLinesSent` 와 `pendingGarbage` 도 같은 이유로 해시에 포함되니, mem-init list 에 명시했다.

결과적으로 "StateHash 가 참조하는 모든 스칼라 필드는 mem-init list 에 나타난다" 는 규칙이 생긴다. 체크리스트로 쓰기 좋다.

### 12.7 수동 확인

빌드 후:

```bash
# 클라이언트 실행, 키보드 DOWN 을 약 1초 길게 누르면서 블록이 바닥까지 내려가는 데
# 걸리는 시간을 초시계로 측정.
./build/Release/tetris.exe

# 블록 스폰 지점(row=0~2)에서 바닥(row=19)까지 약 18 셀 이동.
# 이동 직후 counter=3 으로 재장전하고 이후 감소하므로
# 실제 이동 간격은 약 4틱: 60/(3+1) = 15 cells/s → 18/15 ≈ 1.2 초.
```

기대 결과: DOWN 을 한 번 눌러 바닥까지 약 1.2초. DOWN 을 짧게 탭하면 탭마다 1셀씩 내려감 (뗀 직후 카운터=0). DOWN 을 계속 누른 상태에서는 대략 4틱 간격으로 1셀씩.

---

## 13. 섹션별 해시 분해 (`StateHashBreakdown`)

### 13.1 동기: "해시가 다르다" 만으로는 부족하다

F.2 자동 HASH 검증(Part 6) 은 10초마다 양쪽 피어의 `StateHash()` 를 교환해 비교한다. 다르면 "DESYNC" 배너를 띄운다. 여기까지는 좋다 — 갈라진 순간을 10초 이내에 포착할 수 있다.

문제는 그 다음이다. **어느 필드가 갈라졌는가?** 64비트 해시는 일방향이라 역으로 "그리드 탓인지, RNG 탓인지, 콤바트 상태 탓인지" 를 알 수 없다. 디버깅 시나리오:

1. DESYNC 배너 발견.
2. 로그에 남은 것은 `tick=1234 local=0xA1B2C3D4... remote=0xE5F6A7B8...` 두 값.
3. 원인 후보는 그리드 800바이트 + 블록 32바이트 + RNG 16바이트 + score/flags + combat 상태… 모두 섞여 있다.
4. 재현을 위해 덤프 스크립트를 작성하고, 시드를 복제하고, 단계별로 필드를 비교하는 긴 삽질.

이 삽질을 **한 줄 로그로** 대체하는 것이 `StateHashBreakdown`이다. 전체 상태를
원인 도메인별 독립 해시로 계산해 DESYNC 시 함께 출력한다. 로그를 보는 순간
"combat 해시만 다르다 → 가비지 로직부터 본다"처럼 범위가 즉시 좁혀진다.

### 13.2 구조: 원인 도메인별 상태 묶음

`SimGame::HashBreakdown`은 전체 해시를 원인별로 나눈 값을 담는 POD다:

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
// src/sim_game.h
struct HashBreakdown {
    uint64_t grid;
    uint64_t currentBlock;
    uint64_t nextBlock;
    uint64_t rng;
    uint64_t scoreFlags;    // score, gameOver, gravityCounter, dropInterval, softDrop
    uint64_t combat;        // garbageRng, attackLinesSent, pendingGarbage
};
HashBreakdown StateHashBreakdown() const;
```

섹션 경계를 고른 기준은 **"같이 틀릴 확률이 높은 것끼리 묶는다"**다.
그리드 버그는 그리드 해시만, RNG 버그는 RNG 해시만, 콤바트 버그는 combat
해시만 튄다. 이렇게 묶으면 한 도메인이 갈라졌을 때 다른 도메인들이 정상
대조군 역할을 한다. 필드가 늘어나면 개수를 맞추는 것이 아니라 가장 가까운
원인 도메인에 포함하고, `StateHash()`와 진단 해시를 함께 갱신한다.

| 섹션 | 포함 필드 | 대표 버그 패턴 |
|------|-----------|----------------|
| `grid` | `sim_grid.grid[20][10]` 전체 (800 bytes) | 라인 클리어 순서 오류, 가비지 주입 행 오프셋 오류 |
| `currentBlock` | `id, rotationState, rowOffset, columnOffset` | 회전 복원 누락, SRS 충돌 처리 차이 |
| `nextBlock` | preview 큐 size + 각 `id, rotationState, rowOffset, columnOffset` | 7-bag 가방 교체 타이밍 |
| `rng` | `XorShift64Star` 메인 스트림 state (uint64) | RNG 외부 소비, 호출 횟수 차이 |
| `scoreFlags` | `score, gameOver, gravityCounterTicks, dropIntervalTicks, softDropCounterTicks, totalLinesCleared, level, lastMoveWasRotate` | 점수 계산 실수, 중력 타이머 리셋 누락, 소프트 드롭/T-spin 판정 상태 엣지 |
| `combat` | `garbageRng state, attackLinesSent, pendingGarbage` | 공격 테이블 불일치, 가비지 적용 순서 |

각 섹션 해시는 **독립적인 FNV-1a 체인** 이다. 모두 동일한 offset basis `14695981039346656037ull` 에서 시작한다. 다른 섹션의 바이트가 섞이지 않으므로, `grid` 해시는 오직 그리드 상태에만 의존한다.

### 13.3 전체 구현

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
SimGame::HashBreakdown SimGame::StateHashBreakdown() const
{
    HashBreakdown b{};
    constexpr uint64_t BASE = 14695981039346656037ull;

    // Grid
    b.grid = fnv1a64(&sim_grid.grid[0][0], sizeof(sim_grid.grid), BASE);

    // Current block
    uint64_t cb = BASE;
    cb = fnv1a64_value(currentBlock.id, cb);
    cb = fnv1a64_value(currentBlock.GetRotationState(), cb);
    cb = fnv1a64_value(currentBlock.GetRowOffset(), cb);
    cb = fnv1a64_value(currentBlock.GetColumnOffset(), cb);
    b.currentBlock = cb;

    // Next preview queue
    uint64_t nb = BASE;
    nb = fnv1a64_value(static_cast<int>(nextBlocks.size()), nb);
    for (const SimBlock& next : nextBlocks)
    {
        nb = fnv1a64_value(next.id, nb);
        nb = fnv1a64_value(next.GetRotationState(), nb);
        nb = fnv1a64_value(next.GetRowOffset(), nb);
        nb = fnv1a64_value(next.GetColumnOffset(), nb);
    }
    b.nextBlock = nb;

    // RNG
    b.rng = fnv1a64_value(rng.getState(), BASE);

    // Score / flags / gravity / level
    uint64_t sf = BASE;
    sf = fnv1a64_value(score, sf);
    sf = fnv1a64_value(gameOver ? 1 : 0, sf);
    sf = fnv1a64_value(gravityCounterTicks, sf);
    sf = fnv1a64_value(dropIntervalTicks, sf);
    sf = fnv1a64_value(softDropCounterTicks, sf);
    sf = fnv1a64_value(totalLinesCleared, sf);
    sf = fnv1a64_value(level, sf);
    sf = fnv1a64_value(lastMoveWasRotate ? 1 : 0, sf);
    b.scoreFlags = sf;

    // Combat
    uint64_t co = BASE;
    co = fnv1a64_value(garbageRng.getState(), co);
    co = fnv1a64_value(attackLinesSent, co);
    co = fnv1a64_value(pendingGarbage, co);
    b.combat = co;

    return b;
}
```

구현 상 포인트:

- **필드 묶음은 `StateHash()`와 완전히 같은 순서**로 섞는다. 전체 체인을
  진단 목적의 독립 체인으로 나눴을 뿐이다. 이 일관성 덕분에 필드 누락을
  교차검사하거나 전체 해시와 진단 해시를 함께 검증하기 쉽다.
- `HashBreakdown b{};` 로 zero-init. 만약 어떤 섹션의 해시 계산이 조건부로 스킵되어도(현재는 없지만) 스킵된 필드가 쓰레기 값으로 남지 않는다.
- `grid` 섹션은 단일 `fnv1a64(포인터, 크기, BASE)` 호출로 끝난다. 800바이트를 한 번에 말려 넣어 가장 빠르다. 나머지 섹션은 개별 `fnv1a64_value` 체인.

### 13.4 사용법: DESYNC 시 로그 한 줄

lockstep 해시 교환기가 DESYNC를 감지했다면 `StateHashBreakdown()`을 같은 tick의 진단 로그에 연결할 수 있다. 현재 네트워크 경로는 전체 해시를 교환하며, 아래는 필드 묶음까지 로그에 추가할 때의 예시다.

**예시(실제 저장소에는 없음)**

```cpp
// 예시: DESYNC 진단 로그에 필드 묶음을 추가하는 형태
if (localHash != remoteHash) {
    auto b = sim.StateHashBreakdown();
    fprintf(stderr,
        "[DESYNC] tick=%u local=%016llx remote=%016llx\n"
        "  grid=%016llx current=%016llx next=%016llx\n"
        "  rng=%016llx scoreFlags=%016llx combat=%016llx\n",
        tick, (unsigned long long)localHash, (unsigned long long)remoteHash,
        (unsigned long long)b.grid, (unsigned long long)b.currentBlock,
        (unsigned long long)b.nextBlock, (unsigned long long)b.rng,
        (unsigned long long)b.scoreFlags, (unsigned long long)b.combat);
}
```

양쪽 창에서 같은 tick의 진단 라인을 나란히 열고, 이름이 같은 상태 묶음을
비교한다. 먼저 달라진 묶음은 다음처럼 읽는다.

| 증상 (다른 섹션) | 가장 유력한 원인 |
|------------------|------------------|
| `grid` 만 다름 | 라인 클리어 알고리즘 분기, 가비지 행 삽입 오프셋 |
| `rng` 만 다름 | 외부 코드에서 `sim.rng` 소비, 또는 `GetRandomBlock()` 호출 횟수 차이 |
| `rng` 와 `nextBlock` 동시 다름 | piece-bag 재충전 타이밍 차이 (가방이 비면 `rng` 도 같이 변함) |
| `combat` 만 다름 | `AddPendingGarbage` 호출 순서 또는 `garbageRng` 소비 차이 |
| `scoreFlags` 만 다름 | 점수 계산 실수, `softDropCounterTicks` 엣지, 또는 `lastMoveWasRotate` T-spin 판정 상태 |
| `currentBlock` 만 다름 | 회전/이동 복원 로직 분기 |
| 모두 다름 | 시드 자체가 다르거나, `SubmitInput` 호출 순서가 완전히 갈라짐 |

이 표 하나로 "재현 스크립트 작성 → 필드 수작업 비교" 단계가 통째로 생략된다. 버그 리포트를 받는 시점에 이미 범위가 좁혀져 있는 셈이다.

### 13.5 전체 해시와의 관계

`StateHashBreakdown()` 은 `StateHash()` 를 **대체하지 않는다**. 두 함수는 용도가 다르다:

- `StateHash()`: 10초 자동 검증용 한 줄 비교. 빠르고 단순하게 "같은가 다른가" 만 판단.
- `StateHashBreakdown()`: DESYNC 가 감지된 **후** 에 호출하는 진단 도구. 6배 데이터지만 호출 빈도가 낮으니 비용은 문제 없다.

실용적 운영: 매 틱 `StateHash()` 만 계산/교환하고, 검증 실패 시에만 `StateHashBreakdown()` 로 덤프한다. 정상 틱에서 비용 0, 이상 틱에서만 약간의 추가 작업.

---

## 14. 고스트 블록

고스트 블록은 현재 블록을 아래로 투영(hard drop 시뮬레이션)하여 착지 위치를 미리 보여주는 시각적 가이드다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::DropExpectation()
{
    if (gameOver) return;
    while (IsBlockOutside(ghostBlock) == false && BlockFits(ghostBlock) == true)
    {
        ghostBlock.Move(1, 0);
    }
    ghostBlock.Move(-1, 0);
}
```

고스트 블록의 id는 8로 설정된다. 이 값은 그리드 셀에 기록되지 않는다 (`IsCellEmpty`가 8을 빈칸으로 취급하므로). 렌더링 시에만 반투명 회색으로 표시된다.

`DropExpectation`은 `SubmitInput` 종료 시 호출된다. 매 입력 후 고스트를 갱신해야 현재 블록의 위치 변화가 즉시 반영된다.

---

## 15. 블록 잠금과 게임 오버

### 15.1 LockBlock 전체

`LockBlock`은 sim의 중심 상태 전이다. 피스 고정, 다음 피스 준비, 게임오버 판정, 라인 클리어, 가비지 주입 — 모두 이 함수에서 일어난다. 전체를 한 번에 인용한다 (생략 없음):

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::LockBlock()
{
    const bool tSpin = IsTSpinLock();
    std::vector<Position> tiles = currentBlock.GetCellPositions();
    for (const Position& item : tiles)
    {
        sim_grid.grid[item.row][item.column] = currentBlock.id;
    }
    currentBlock = NextBlock();
    ghostBlock = MakeGhostBlock(currentBlock);
    bool wasGameOver = gameOver;
    if (BlockFits(currentBlock) == false)
    {
        gameOver = true;
    }

    nextBlocks.erase(nextBlocks.begin());
    nextBlocks.push_back(GetRandomBlock());
    int rowsCleared = sim_grid.ClearFullRows();
    lastLinesCleared = rowsCleared;
    lastTSpinLines = tSpin ? rowsCleared : -1;
    if (rowsCleared > 0 || tSpin)
    {
        if (rowsCleared > 0) clearSoundEvent = true;
        UpdateScore(rowsCleared, 0, tSpin);
        attackLinesSent += attack_lines_for(rowsCleared, tSpin);
    }
    lastMoveWasRotate = false;

    // 가비지 주입 — 라인 클리어 적용 후, 다음 피스가 확정된 이 시점에서 하단으로 올라온다.
    // 주의: 클리어 없이 그냥 놓은 경우에도 pendingGarbage 가 있으면 받는다.
    int inserted = 0;
    if (pendingGarbage > 0 && !gameOver)
    {
        inserted = pendingGarbage;
        InsertGarbage(pendingGarbage);
        pendingGarbage = 0;
        // 가비지가 올라와 currentBlock 스폰 위치를 막았으면 topout.
        if (!BlockFits(currentBlock)) gameOver = true;
    }
    lastGarbageReceived = inserted;
    if (inserted > 0) garbageSoundEvent = true;

    if (gameOver && !wasGameOver) gameOverEvent = true;
}
```

순서가 중요하다:

1. **현재 피스를 그리드에 기록**: `grid[row][col] = currentBlock.id`.
2. **currentBlock ← NextBlock() 승격**: preview 큐의 첫 피스가 현재 피스가 된다.
3. **스폰 위치 검사**: 새 currentBlock이 이미 점유된 셀과 겹치면 `gameOver = true`. 이것이 1차 top-out 조건.
4. **preview 큐 갱신**: 첫 항목을 제거하고 `GetRandomBlock()` 으로 새 3번째 preview 를 보충한다.
5. **라인 클리어 + 점수**: `ClearFullRows()`, `UpdateScore(rowsCleared, 0, tSpin)`, T-spin 공격 테이블. (시그니처는 `UpdateScore(int linesCleared, int levelUp, bool tSpin)` 로 인자가 셋이다.)
6. **가비지 주입**: `InsertGarbage()` — 2차 top-out 검사.
7. **gameOverEvent 플래그**: 이 틱에 전이했다면 1회 표시.

### 15.2 게임 오버가 두 번 검사되는 이유

게임 오버 조건은 두 번 검사된다:

- **1차 (스폰 블록)**: preview 큐 첫 블록이 승격된 직후, 이미 블록이 쌓여 있어 스폰 위치가 막힌 경우.
- **2차 (가비지 top-out)**: 가비지가 올라온 후, 밀려 올라간 블록이 스폰 위치를 덮친 경우.

두 경우 모두 게임오버지만 플레이어의 주관적 경험은 다르다. 1차는 "내가 너무 높이 쌓았다", 2차는 "상대 공격을 받다가 죽었다". 로그나 리플레이에서 구분 가능하도록 설계되었다 (필요하면 별도 플래그 추가).

### 15.3 GameOver 순서의 중요성

스폰 충돌 검사는 preview 큐를 보충하기 전에 일어나지만, 큐 갱신은 `gameOver` 여부와 관계없이 계속 실행된다. 여기서 지켜야 할 계약은 두 가지다. 게임 오버 화면에서도 `nextBlocks`가 유효해야 하고, 모든 `LockBlock`이 같은 위치에서 피스 RNG를 소비해야 한다. 큐 보충을 `if (!gameOver)` 안으로 옮기면 화면 데이터만 달라지는 것이 아니라 두 클라이언트의 RNG 상태도 갈라진다. 따라서 중요한 것은 검사와 보충의 상대적 위치 자체보다 **보충을 조건부로 만들지 않는 것**이다.

---

## 16. SimGame 상태 머신

전체 게임 로직을 상태 전이로 정리한다:

```mermaid
stateDiagram-v2
    [*] --> Playing: SimGame(seed)
    Playing --> Playing: SubmitInput(mask)
    Playing --> Playing: Tick() [gravity]

    state Playing {
        [*] --> Falling
        Falling --> Falling: Move/Rotate
        Falling --> Locked: 아래 이동 실패
        Locked --> ClearCheck: 그리드에 기록
        ClearCheck --> SpawnNext: ClearFullRows() + Attack
        SpawnNext --> GarbageInject: pendingGarbage > 0
        SpawnNext --> Falling: pendingGarbage == 0
        GarbageInject --> Falling: BlockFits(new) = true
        GarbageInject --> GameOver: BlockFits(new) = false
        SpawnNext --> GameOver: BlockFits(new) = false
    }

    Playing --> [*]: gameOver = true
```

한 틱의 실행 흐름:

1. **SubmitInput**: 좌/우/하/회전/드롭 입력 처리. 각 입력은 이동-검증-복원 패턴.
2. **Tick**: 중력 카운터 증가. `dropIntervalTicks`에 도달하면 `MoveBlockDown`.
3. `MoveBlockDown`에서 아래 이동 실패 시 `LockBlock`.
4. `LockBlock`: 그리드 기록 → 게임 오버 검사 → 새 블록 생성 → 라인 클리어 → 공격 누적 → 가비지 주입 → 2차 게임 오버 검사 → 이벤트 플래그 설정.

---

## 레벨 시스템과 T-spin

초기 sim은 점수가 고정 표였고 `dropIntervalTicks`도 생성자에서 정한 값으로 유지됐다. 현재 규칙은 **10라인마다 레벨 증가, 레벨에 따른 중력 가속, base 점수에 레벨 배율 적용**을 한 묶음으로 추가한다. 고전 테트리스의 점진적 난도 상승에서 아이디어를 얻었지만, 레벨 경계·중력식·점수표는 이 프로젝트가 직접 정한 계약이다.

T-spin 은 별개 주제지만 레벨과 동시에 들어왔다. "T 피스를 회전으로 빈틈에 끼워 넣어 라인을 지운다" 는 경쟁 테트리스의 기본 보너스 행위인데, sim 입장에서는 "마지막 위치 변경이 회전이었나" + "pivot 주변 4모서리 중 3 이상이 막혔나" 라는 두 조건만 추적하면 된다.

### A. 레벨 시스템

`SimGame` 에 두 필드를 추가:

**현재 소스 발췌 — `src/sim_game.h`**

```cpp
    int totalLinesCleared = 0;  // 누적 클리어 라인 수
    int level = 1;              // 현재 레벨 (10라인마다 +1, 최대 20)
```

`UpdateScore` 가 새 시그니처로 바뀐다 — 라인 수 + 레벨업 보너스 + T-spin 여부:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::UpdateScore(int linesCleared, int levelUp, bool tSpin)
{
    // 프로젝트 점수표에 현재 레벨을 배율로 적용한다. NES와 마찬가지로
    // 높은 레벨의 생존을 더 보상하지만 base 점수 자체는 이 게임 고유 값이다.
    if (tSpin)
    {
        switch (linesCleared)
        {
        case 0: score += 400  * level; break;
        case 1: score += 800  * level; break;
        case 2: score += 1200 * level; break;
        case 3: score += 1600 * level; break;
        default: break;
        }
    }
    else
    {
        switch (linesCleared)
        {
        case 1: score += 100  * level; break;
        case 2: score += 300  * level; break;
        case 3: score += 600  * level; break;
        case 4: score += 1000 * level; break;
        default: break;
        }
    }
    score += levelUp * 1000;

    // 레벨 시스템: 10라인마다 레벨업 + 중력 증가.
    totalLinesCleared += linesCleared;
    int newLevel = totalLinesCleared / 10 + 1;
    if (newLevel > level) {
        level = (newLevel > 20) ? 20 : newLevel;
        // 레벨별 중력: 1→30틱, 5→25틱, 10→18틱, 15→11틱, 20→3틱.
        // TICKS_PER_SECOND=60 기준으로 30에서 3까지 정수 선형 보간한다.
        int newInterval = 30 - (level - 1) * 27 / 19;  // 레벨1=30, 레벨20=3
        if (newInterval < 3) newInterval = 3;
        dropIntervalTicks = newInterval;
    }
}
```

base 점수표에 `level`을 곱하는 것이 핵심이다. Tetris의 base 1,000점은 Lv 5에서 5,000점, Lv 20에서 20,000점이 된다. 후반에 살아남는 가치가 점수로 환산된다.

중력 공식 `30 - (level-1) * 27 / 19` 는 정수 연산만으로 Lv1=30, Lv20=3 을 보간한다 — 부동소수 금지(섹션 11.3) 원칙을 지키기 위해 의도적으로 정수 나눗셈. `27/19 ≈ 1.42` 이므로 거의 매 레벨 1~2 틱씩 빨라진다. 60 Hz 기준으로 환산:

| 레벨 | dropIntervalTicks | 초당 중력 낙하 셀 |
|------|-------------------|------------------|
| 1    | 30                | 2 셀/초           |
| 5    | 30 − 4×27/19 = 25 | 2.4 셀/초         |
| 10   | 30 − 9×27/19 = 18 | 3.3 셀/초         |
| 15   | 30 − 14×27/19 = 11 | 5.5 셀/초        |
| 20   | 3                 | 20 셀/초          |

Lv 20 의 3 틱 (= 0.05 초) 은 사실상 "보자마자 떨어진다" 수준. 소프트 드롭 한도(섹션 12.3) 가 4 틱이라는 점을 생각하면, 고레벨에서는 손가락보다 중력이 빠르다.

`level` 과 `totalLinesCleared` 둘 다 `StateHash` 에 들어간다 — 섹션 11.4 의 "해시에 포함되는 모든 것" 목록에 추가된 항목이다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
    sf = fnv1a64_value(score, sf);
    sf = fnv1a64_value(gameOver ? 1 : 0, sf);
    sf = fnv1a64_value(gravityCounterTicks, sf);
    sf = fnv1a64_value(dropIntervalTicks, sf);
    sf = fnv1a64_value(softDropCounterTicks, sf);
    sf = fnv1a64_value(totalLinesCleared, sf);
    sf = fnv1a64_value(level, sf);
    sf = fnv1a64_value(lastMoveWasRotate ? 1 : 0, sf);
```

`dropIntervalTicks` 가 양쪽 클라이언트에서 같은 틱에 같은 값으로 바뀌어야 lockstep 가속이 일치한다. `level` 과 `totalLinesCleared` 를 해시에 넣어두면, 한쪽만 레벨이 잘못 올라간 경우 즉시 DESYNC 배너가 뜬다.

### B. T-spin 판정

`SimBlock::id` 가 6 인 피스가 T 피스다 (섹션 3.1 의 GetAllBlocks 순서: I,J,L,O,S,T,Z → 인덱스 5 인데 본 sim 은 1-base id 로 6). T-spin 의 두 조건:

1. **마지막 위치 변경이 회전** — 이동 (좌/우/하/드롭) 이 아닌 회전으로 끝났어야 한다.
2. **T pivot 주변 4모서리 중 3+ 가 막힘** — pivot 셀 (피스 중심) 의 네 대각선 셀 중 적어도 3 개가 그리드 경계 밖이거나 비어있지 않아야 한다.

`lastMoveWasRotate` 플래그가 매 동작에서 갱신된다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::RotateBlockImpl()
{
    if (gameOver) return;
    currentBlock.Rotate();
    if (IsBlockOutside(currentBlock) == true || BlockFits(currentBlock) == false)
    {
        currentBlock.UndoRotation();
    }
    else
    {
        lastMoveWasRotate = true;
        rotateSoundEvent = true;
        ghostBlock = MakeGhostBlock(currentBlock);
    }
}
```

좌/우/하 이동이 성공하면 플래그가 다시 false 로 떨어진다 (`MoveBlockLeft`/`MoveBlockRight`/`MoveBlockDown` 각 분기). 회전 자체가 실패한 경우 (벽에 막힘) 는 플래그가 변하지 않는다 — UndoRotation 만 일어났을 뿐 sim 의 "마지막 동작" 은 직전 그 무엇이다.

판정 함수:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
bool SimGame::IsTSpinLock() const
{
    if (currentBlock.id != 6 || !lastMoveWasRotate) return false;

    const int pivotRow = currentBlock.rowOffset + 1;
    const int pivotCol = currentBlock.columnOffset + 1;
    const int corners[4][2] = {
        {pivotRow - 1, pivotCol - 1},
        {pivotRow - 1, pivotCol + 1},
        {pivotRow + 1, pivotCol - 1},
        {pivotRow + 1, pivotCol + 1},
    };

    int blocked = 0;
    for (const auto& corner : corners)
    {
        const int row = corner[0];
        const int col = corner[1];
        if (sim_grid.IsCellOutside(row, col) || !sim_grid.IsCellEmpty(row, col))
        {
            blocked++;
        }
    }
    return blocked >= 3;
}
```

T 피스의 bounding box 가 3×3 이므로 `(rowOffset+1, columnOffset+1)` 이 항상 중심이다 — 회전 상태와 무관. 4 모서리 중 3+ 가 막혀 있으면 "T 가 끼워들어간 모양" 이라는 휴리스틱이 성립한다.

`LockBlock` 의 첫 줄에서 잠금 직전 상태로 호출:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::LockBlock()
{
    const bool tSpin = IsTSpinLock();
    std::vector<Position> tiles = currentBlock.GetCellPositions();
    for (const Position& item : tiles)
    {
        sim_grid.grid[item.row][item.column] = currentBlock.id;
    }
    currentBlock = NextBlock();
    ghostBlock = MakeGhostBlock(currentBlock);
    bool wasGameOver = gameOver;
    if (BlockFits(currentBlock) == false)
    {
        gameOver = true;
    }

    nextBlocks.erase(nextBlocks.begin());
    nextBlocks.push_back(GetRandomBlock());
    int rowsCleared = sim_grid.ClearFullRows();
    lastLinesCleared = rowsCleared;
    lastTSpinLines = tSpin ? rowsCleared : -1;
    if (rowsCleared > 0 || tSpin)
    {
        if (rowsCleared > 0) clearSoundEvent = true;
        UpdateScore(rowsCleared, 0, tSpin);
        attackLinesSent += attack_lines_for(rowsCleared, tSpin);
    }
    lastMoveWasRotate = false;

    // 가비지 주입 — 라인 클리어 적용 후, 다음 피스가 확정된 이 시점에서 하단으로 올라온다.
    // 주의: 클리어 없이 그냥 놓은 경우에도 pendingGarbage 가 있으면 받는다.
    int inserted = 0;
    if (pendingGarbage > 0 && !gameOver)
    {
        inserted = pendingGarbage;
        InsertGarbage(pendingGarbage);
        pendingGarbage = 0;
        // 가비지가 올라와 currentBlock 스폰 위치를 막았으면 topout.
        if (!BlockFits(currentBlock)) gameOver = true;
    }
    lastGarbageReceived = inserted;
    if (inserted > 0) garbageSoundEvent = true;

    if (gameOver && !wasGameOver) gameOverEvent = true;
}
```

`tSpin` 이 true 면 `attack_lines_for` 가 가비지 라인을 부풀려 산정한다.

### C. attack_lines_for — 공격 테이블

T-spin 인지 아닌지에 따라 가비지 라인 수가 다르다:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
static int attack_lines_for(int rowsCleared, bool tSpin)
{
    if (tSpin)
    {
        switch (rowsCleared) {
            case 1: return 2;   // T-spin Single
            case 2: return 4;   // T-spin Double
            case 3: return 6;   // T-spin Triple
            default: return 0;  // T-spin no-line
        }
    }
    switch (rowsCleared) {
        case 2: return 1;   // Double → 1 가비지
        case 3: return 2;   // Triple → 2 가비지
        case 4: return 4;   // Tetris → 4 가비지
        default: return 0;  // Single or none
    }
}
```

| 클리어 | 일반 | T-spin |
|--------|------|--------|
| 0 줄   | 0    | 0 (점수만)  |
| 1 줄   | 0    | **2** (TSS) |
| 2 줄   | 1    | **4** (TSD) |
| 3 줄   | 2    | **6** (TST) |
| 4 줄   | 4 (Tetris) | — |

T-spin Double 이 일반 Tetris 와 같은 가비지를 보내고, T-spin Triple 은 그것을 넘어선다. 경쟁 테트리스에서 TST 가 "결정타" 로 불리는 이유다.

T 는 1 줄짜리도 가비지를 보낸다는 점이 핵심. 일반 Single 은 가비지 0 인데 T-spin Single 은 2 — 일반 클리어로는 닿을 수 없는 영역이라 별도 가비지 보상이 정당화된다.

### D. 결정론

T-spin 판정은 `currentBlock.rowOffset/columnOffset/id` + `sim_grid` + `lastMoveWasRotate` 만으로 결정된다. 모두 sim 내부 상태고, 모두 `StateHash` 에 들어간다. 즉 **양쪽 클라이언트의 같은 시드 + 같은 입력 = 같은 T-spin 판정** 이 자동 보장된다.

특히 `lastMoveWasRotate` 가 모든 동작 분기에서 일관되게 갱신되는 게 중요하다. `SubmitInput` 의 RotateBlockImpl 만 true 로 올리고, MoveBlockLeft/Right/Down 의 *성공* 분기는 false 로 내린다. 회전이 실패해도 (벽 막힘 → UndoRotation) 플래그를 건드리지 않는다 — 그 직전 동작이 회전이었으면 여전히 회전 상태다.

```mermaid
flowchart TB
    A["LockBlock 호출"] --> B{"id == 6?"}
    B -->|No| F["tSpin=false"]
    B -->|Yes| C{"lastMoveWasRotate?"}
    C -->|No| F
    C -->|Yes| D["pivot 주변 4모서리 검사"]
    D --> E{"막힌 모서리 ≥ 3?"}
    E -->|No| F
    E -->|Yes| G["tSpin=true"]
    F --> H["attack_lines_for(rows, false)"]
    G --> I["attack_lines_for(rows, true)<br/>1줄=2 / 2줄=4 / 3줄=6"]
```

### E. 여기서 빌드해보자

```bash
cmake --build build --config Release
./build/Release/tetris.exe
```

기대 동작:
- 첫 라인 클리어 후 화면 우측 패널의 LEVEL 이 1, LINES 가 1~9 누적. 10 라인 누적 시 LEVEL 이 2 로 올라가고 블록 낙하 속도가 눈에 띄게 빨라진다.
- T 피스를 좁은 슬롯에 회전으로 끼워넣고 라인을 지우면 점수 증가폭이 일반 클리어 (1 줄 = 100×Lv) 보다 훨씬 크다 (T-spin Single = 800×Lv). 멀티 모드라면 상대 보드 상단에 가비지 2 줄이 올라온다.
- `H` 키로 해시를 출력했을 때 `level=N, lines=M` 부분이 양쪽 클라에서 정확히 일치 (lockstep 가속).

---

## 오류와 함정

이 섹션은 실제로 이 코드베이스에서 잡힌 결정론 버그와, 잡기 어려웠던 이유를 기록한다. "같은 실수를 다시 하지 않기 위해" 남긴다.

### (1) RNG 호출 순서 변경 → 결정론 파괴

**증상:** 같은 시드를 넣었는데 양쪽 피어의 블록 순서가 다르다. 10~20초간은 동기화되다가 어느 시점에서부터 완전히 갈라진다.

**원인:** RNG가 `GetRandomBlock()` 외부에서 호출되면, 호출 횟수가 입력/타이밍에 따라 달라져 RNG 상태가 분기한다. 예: 렌더 레이어에서 "블록 잠금 시 파티클 이펙트" 용도로 `sim.rng.next()`를 빌려 쓴 경우.

**해결:** RNG 호출 지점을 `GetRandomBlock()` 하나로 제한. 시각 효과/오디오 등에 난수가 필요하면 별도의 RNG 인스턴스를 사용 (11.7 섹션).

**재발 방지:** `sim.RngState()` 접근자는 const 를 반환하고, `rng` 멤버는 private. 외부가 소비할 수 없다.

### (2) size_t 역순 순회 언더플로

**증상:** `ClearFullRows()` 호출 시 무한 루프 또는 메모리 접근 위반.

**원인:** `for (size_t row = kRows - 1; row >= 0; row--)`에서 `row`가 unsigned이므로 `0 - 1 = SIZE_MAX`, 조건 `row >= 0`이 항상 참.

**해결:** 루프 변수를 `int`로 선언. 또는 `for (int row = kRows; row-- > 0;)` 패턴 사용.

> **레퍼런스:** C++ 표준 [basic.fundamental]: unsigned 정수의 오버플로/언더플로는 모듈러 산술로 잘 정의된다 (UB가 아니다). 그러나 의도하지 않은 모듈러 산술은 논리 오류의 원인이 된다.

### (3) 회전 후 undo 누락 → 벽 속 삽입

**증상:** 블록이 벽이나 다른 블록과 겹친 상태로 고정된다.

**원인:** `RotateBlockImpl()`에서 충돌 시 `UndoRotation()` 호출을 빠뜨리면, 겹친 상태가 유지된 채 다음 프레임에서 `LockBlock()`이 호출될 수 있다.

**해결:** 이동-검증-복원 패턴을 엄격히 따른다. 모든 상태 변경 후 반드시 충돌 검사를 수행하고, 실패 시 복원.

### (4) 블록 생성 순서 변경 → 해시 불일치

**증상:** `StateHash()`가 원본 `Game` 클래스와 다른 값을 반환한다.

**원인:** `GetAllBlocks()`의 블록 순서가 원본과 다르면, 같은 RNG 시드에서 다른 블록이 선택된다. 예: 원본이 `{I,J,L,O,S,T,Z}` 순서인데 `{L,J,I,O,S,T,Z}`로 변경하면, `rng.nextUInt(7) = 0`이 원본에서는 I 블록, 변경 후에는 L 블록이 된다.

**해결:** `GetAllBlocks()` 순서를 원본과 **정확히** 일치시킨다. 코드 주석으로 순서를 명시:

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
std::vector<SimBlock> SimGame::GetAllBlocks() const
{
    // Order MUST match original Game::GetAllBlocks exactly: I,J,L,O,S,T,Z.
    // The order determines which id is at which vector index, and the RNG
    // selects by index — changing order breaks state hash parity.
    return {SimIBlock(), SimJBlock(), SimLBlock(), SimOBlock(), SimSBlock(), SimTBlock(), SimZBlock()};
}
```

### (5) 고스트 블록과 충돌 판정

**증상:** 고스트 블록 위에 다른 블록을 놓을 수 없다.

**원인:** `IsCellEmpty`가 고스트(id=8)를 빈칸으로 취급하지 않으면, 고스트가 있는 셀에 블록을 놓을 수 없게 된다.

**해결:** `IsCellEmpty`에서 `grid[row][column] == 8`을 빈칸으로 판별. 고스트는 시각적 가이드일 뿐 물리적 실체가 아님.

### (6) 부동소수 실수: 중력 타이머를 float로

**증상:** 로컬에서는 잘 동작하지만 상대 피어와 연결하면 서서히 desync. 정확히 어느 순간에 갈라지는지 일정하지 않다.

**원인:** 중력 타이머 로직을 "시간 기반"으로 작성한 초기 버전에서 `gravityCounter += deltaSeconds;` 형태였다. MSVC와 GCC의 `float` 연산이 특정 시퀀스에서 ULP 단위로 다른 값을 내놓고, 누적되며 `gravityCounter >= dropInterval` 분기가 다른 틱에서 트리거.

**해결:** 부동소수를 완전히 제거. `gravityCounterTicks` 는 단순 `int`로 카운트. 0.5초 = 30틱을 직접 상수로 박는다:

**예시(설명용 축약 — 실제 코드는 부록 B)**

```cpp
gravityCounterTicks(0),
dropIntervalTicks(TICKS_PER_SECOND / 2) // default: drop every 0.5s
```

**재발 방지:** sim 디렉터리에서 `float`/`double` 문자열을 금지어로 설정하고, CI에서 grep 체크.

### (7) 외부 상태 주입: 렌더에서 sim 수정

**증상:** 렌더 프레임률이 떨어지는 순간 desync.

**원인:** 초기 개발에서 렌더러가 고스트 블록 색상 변경을 위해 `sim.ghostBlock.id` 를 직접 수정한 코드가 있었다. 고스트가 `id=8` 이 아닌 다른 값을 가지면 `IsCellEmpty`가 이를 빈칸으로 인식하지 않아 충돌 판정이 달라진다.

**해결:** ghostBlock 은 sim이 관리하고 렌더러는 읽기만. 색상은 렌더러가 `ghostBlock.id == 8` 을 고유 시각 효과로 변환하는 것으로 처리.

**재발 방지:** 11.2 절의 일방향 참조 규칙을 문서화. 코드 리뷰에서 `sim\.` 으로 시작하는 할당을 렌더 디렉터리에서 플래그.

### (8) FNV-1a Python 포트의 마스킹 누락

**증상:** 클라이언트 → 서버 연결이 HELLO 후 무한 대기. 서버 로그에서 프레임이 받아졌다는 기록이 없다.

**원인:** `net/framing.cpp`의 FNV-1a 32bit를 `python/netbot/framing.py`로 포팅할 때, Python이 unsigned 32bit truncation을 자동으로 하지 않는다는 점을 간과. `h = h * FNV1A32_PRIME` 다음 `& FNV1A32_MASK` 가 빠지면 Python 결과가 몇 바이트 이후 32비트를 넘어서고 C++ 결과와 다름. 체크섬 미스매치로 서버의 `parse_frames` 가 프레임을 조용히 버림 (방어적 설계).

**해결:** 매 곱셈 후 명시적 `& 0xFFFFFFFF`. 11.6 절의 코드 참조.

**재발 방지:** `python/tests/test_framing_parity.py`가 C++에서 캡쳐한 프레임 바이트 시퀀스를 Python 으로 파싱하고 역으로 빌드해서, 체크섬이 bit-for-bit 일치하는지 CI에서 게이트.

### (9) 가비지 RNG 스트림 공유 → 피스 순서 어긋남

**증상:** 전투가 시작되는 순간(첫 공격이 주입되는 시점)부터 양쪽 피어의 블록 순서가 갈라진다.

**원인:** 초기 설계에서 `garbageRng` 없이 `rng`로 가비지 홀도 뽑음. 양쪽 피어가 공격을 주고받는 타이밍이 **미세하게** 다르면(예: 한쪽이 다른 쪽보다 1틱 빠른 락), `rng.nextUInt(10)` 호출 순서가 엇갈려 다음 피스 번호도 어긋남.

사실 lockstep 네트코드에서는 양쪽이 틱별로 정확히 동일한 입력을 갖고 있어 "타이밍 차이"가 원리적으로 없어야 한다. 그러나 `AddPendingGarbage` 가 외부에서 호출되는 방식이 일관되지 않으면 (예: 어느 쪽이 먼저 적용하는지) 차이가 생긴다.

**해결:** `garbageRng` 를 별도 스트림으로 분리 (10.5 절). piece-bag RNG 와 가비지 홀 RNG 가 서로 간섭하지 않는다.

**재발 방지:** `StateHash()` 에 `garbageRng.getState()` 포함 (9.3 절). F.2 자동 HASH 검증에서 즉시 DESYNC로 감지.

---

---

## 부록 A. CMakeLists 확장

실행 파일 하나였던 빌드 뼈대에 처음으로 재사용 가능한 계층을 만든다. 시뮬레이션 소스를 변수로 묶고, 그 위에 결정론 테스트 타깃을 세운다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
# -----------------------------------------------------------------------------
# Pure (no raylib) logic — used by game, pybind11 module, and tests.
set(TETRIS_SIM_SOURCES
    src/sim_game.cpp
    src/position.cpp
)

set(TETRIS_SIM_HEADERS
    src/sim_game.h
    src/sim_grid.h
    src/sim_block.h
    src/sim_blocks.h
    src/position.h
    core/constants.h
    core/input.h
    core/rng.h
    core/hash.h
)
```

`.cpp` 가 둘뿐인 것이 이 계층의 성격을 그대로 보여준다. `SimGrid`, `SimBlock`, `SimBlocks` 는 전부 헤더 전용이다 — 구조체와 inline 함수만 담고 있어 별도 번역 단위가 필요 없다. `SimGame` 만 구현 분량이 커서 `.cpp` 로 분리했다.

pybind11 모듈과 `sim_hash_dump` 는 `${TETRIS_SIM_SOURCES}` 만 링크해 renderer·audio·net 심볼 없이 빌드된다. **`SimGame` 이 실수로 `renderer.h` 를 include 하는 순간 이 두 타깃의 빌드가 깨진다.** 계층 경계가 문서가 아니라 빌드 시스템으로 강제되는 셈이다.

**현재 소스 발췌 — `CMakeLists.txt`**

```cmake
if (TETRIS_BUILD_TEST)
    add_executable(sim_hash_dump
        tests/sim_hash_dump.cpp
        ${TETRIS_SIM_SOURCES}
        ${TETRIS_SIM_HEADERS}
    )
    target_include_directories(sim_hash_dump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

`TETRIS_BUILD_TEST` 는 기본 ON 이다. 이 체크포인트의 검증 대상은 `sim_hash_dump`다. 완성형 `CMakeLists.txt`에서 같은 옵션 아래에 보이는 `worker_group_test`는 relay 작업 큐의 종료 계약을 검증하는 별도 타깃이다.

`TETRIS_BUILD_GAME` 은 **기본 ON** 이라는 점에 주의하라. 이 장까지 만든 파일로는 게임 클라이언트를 빌드할 수 없으므로(`src/game.cpp`, `renderer/`, `net/` 이 아직 없다) configure 단계에서 "Cannot find source file" 로 죽는다. 아래 수동 테스트가 `-DTETRIS_BUILD_GAME=OFF` 를 명시하는 이유다.

---

## 부록 B. 이 장의 전체 소스

본문은 규칙과 설계 근거를 따라가느라 함수를 필요한 순서대로 꺼내 썼다. 여기서는 빠진 조각을 채워 **이 장의 산출물 전체**를 한자리에 모은다. 본문에서 이미 전문을 인용한 것(`LockBlock`, `UpdateScore`, `StateHash`, `IsTSpinLock`, `GetRandomBlock`, `RotateBlockImpl`, `MoveBlockDrop`, `MoveBlockLeft`, `InsertGarbage`)은 반복하지 않는다.

### B.1 공개 API — `src/sim_game.h`

**현재 소스 발췌 — `src/sim_game.h`**

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include "sim_grid.h"
#include "sim_block.h"
#include "sim_blocks.h"
#include "../core/rng.h"
#include "../core/input.h"
#include "../core/constants.h"

// 화면도 소리도 파일 I/O 도 없는 순수 테트리스 시뮬레이션.
//
// SimGame is the single source of truth for game logic and must produce the
// same state transitions as the old Game class (verified via ComputeStateHash).
//
// Two action levels:
//   - frame-level (lockstep net play):    SubmitInput(mask) + Tick()
//   - placement-level (RL):                LegalPlacements() + ApplyPlacement(col, rot)
//
// Observations for Python/pybind11 are exposed via accessor methods.
class SimGame
{
public:
    static constexpr int kNextPreviewCount = 3;

    explicit SimGame(uint64_t seed = 0);

    // ---- Placement-level action API (for RL training) ----
    struct Placement
    {
        int col;
        int rot;
    };
    // Enumerates every (col, rot) where the current piece can land via
    // rotate-then-translate-then-hard-drop. col is the piece's columnOffset
    // after moving, rot is the target rotation state.
    std::vector<Placement> LegalPlacements() const;
    // Applies a placement decision atomically (rotate -> translate -> hard drop -> lock).
    // Returns the number of lines cleared, or -1 if the placement is illegal.
    int ApplyPlacement(int col, int rot);

    // ---- Frame-level action API (for lockstep net play) ----
    void SubmitInput(uint8_t inputMask);
    void Tick();
    void MoveBlockDown();

    // ---- Observation accessors ----
    // Returns a const reference to the raw 20x10 grid. Layout matches old
    // Grid::grid for bitwise hash parity.
    const int (&Grid() const)[SimGrid::kRows][SimGrid::kCols] { return sim_grid.grid; }

    const SimBlock& CurrentBlock() const { return currentBlock; }
    const SimBlock& GhostBlock() const { return ghostBlock; }
    const SimBlock& NextBlock() const { return nextBlocks.front(); }
    const std::vector<SimBlock>& NextBlocks() const { return nextBlocks; }

    int CurrentBlockId() const { return currentBlock.id; }
    int CurrentRotation() const { return currentBlock.rotationState; }
    int CurrentRow() const { return currentBlock.rowOffset; }
    int CurrentCol() const { return currentBlock.columnOffset; }
    int NextBlockId() const { return NextBlock().id; }
    int Score() const { return score; }
    bool IsGameOver() const { return gameOver; }

    // ---- Determinism / debugging ----
    // Matches Game::ComputeStateHash bitwise (hash parity gate).
    uint64_t StateHash() const;
    uint64_t RngState() const { return rng.getState(); }

    // DESYNC 원인 특정용 섹션별 해시. 두 인스턴스에서 이 값을 비교하면 어느
    // 부분(그리드/블록/RNG/콤바트)이 달라졌는지 즉시 좁힐 수 있다.
    struct HashBreakdown {
        uint64_t grid;
        uint64_t currentBlock;
        uint64_t nextBlock;
        uint64_t rng;
        uint64_t scoreFlags;    // score, gameOver, gravity/drop timers, level, T-spin setup
        uint64_t combat;        // garbageRng, attackLinesSent, pendingGarbage
    };
    HashBreakdown StateHashBreakdown() const;

    // ---- Combat API (Section I) ----
    // attackLinesSent: 세션 전체 누적 공격 라인 수. 외부에서 델타를 뽑아
    //   상대 SimGame::AddPendingGarbage 로 전달한다. 네트워크 프레임 없음.
    // pendingGarbage: 다음 LockBlock 시점에 하단으로 삽입될 가비지 행 수.
    int AttackLinesSent() const { return attackLinesSent; }
    int PendingGarbage() const { return pendingGarbage; }
    void AddPendingGarbage(int rows) { if (rows > 0) pendingGarbage += rows; }

    // ---- Public mutable state (for renderer wrapper backward-compat) ----
    // main.cpp reads/writes Game::gameOver and reads Game::score via reference
    // members; exposing them here lets the Game wrapper alias them directly.
    bool gameOver;
    int score;

    // ---- One-shot event flags for audio in the Game wrapper ----
    // Set by SimGame when the corresponding event occurs (successful rotate,
    // line clear). The Game wrapper reads and clears them each tick.
    mutable bool rotateSoundEvent  = false;
    mutable bool clearSoundEvent   = false;
    mutable bool dropSoundEvent    = false;  // 하드드롭(Space) 시
    mutable bool garbageSoundEvent = false;  // 가비지 행 수신 시
    // 하드드롭 화면 흔들림(약) 트리거용. dropSoundEvent 와 별개 — 그쪽은
    // 오디오(game.cpp)가 소비·리셋하므로 흔들림이 그것에 의존하면 안 된다.
    // 렌더 전용 1회 플래그 (해시/lockstep/replay 와 무관).
    mutable bool hardDropEvent     = false;  // 하드드롭(Space) 시 (흔들림용)

    // ---- Combat event flags (Section I) ----
    // LockBlock 내부에서 세팅되고 렌더러(쉐이크/이펙트)가 소비 후 클리어.
    mutable int  lastLinesCleared = 0;    // 마지막 LockBlock의 라인 클리어 수 (0..4)
    mutable int  lastTSpinLines = -1;     // T-spin 이벤트면 0..3, 아니면 -1
    mutable int  lastGarbageReceived = 0; // 마지막 LockBlock에서 실제 주입된 가비지 행 수
    mutable bool gameOverEvent = false;   // 이 틱에 gameOver 로 전이한 경우 1회

    // ---- Level system ----
    int totalLinesCleared = 0;  // 누적 클리어 라인 수
    int level = 1;              // 현재 레벨 (10라인마다 +1, 최대 20)

private:
    void MoveBlockLeft();
    void MoveBlockRight();
    void MoveBlockDrop();
    void DropExpectation();
    void RotateBlockImpl();
    void LockBlock();
    void UpdateScore(int linesCleared, int levelUp, bool tSpin);
    void InsertGarbage(int rows);

    bool IsTSpinLock() const;
    bool IsBlockOutside(const SimBlock& block) const;
    bool BlockFits(const SimBlock& block) const;

    SimBlock GetRandomBlock();
    std::vector<SimBlock> GetAllBlocks() const;
    SimBlock MakeGhostBlock(const SimBlock& block) const;

    SimGrid sim_grid;
    std::vector<SimBlock> blocks;
    XorShift64Star rng;
    // 가비지 홀 컬럼용 별도 RNG 스트림. 시드에서 유도되어 양쪽 클라이언트가
    // 동일한 홀 시퀀스를 뽑는다. piece-bag RNG 와 상태가 섞이지 않음이 중요.
    XorShift64Star garbageRng;
    SimBlock currentBlock;
    SimBlock ghostBlock;
    std::vector<SimBlock> nextBlocks;

    int gravityCounterTicks;
    int dropIntervalTicks;

    // Soft-drop (held DOWN) rate limit — 일반 테트리스는 중력보다 빠르지만
    // 프레임레이트(60Hz) 그대로 내리면 60셀/초로 과도. 아래 카운터로 N틱마다
    // 한 번만 MoveBlockDown 호출. 최초 눌림은 즉시 반응(카운터=0 시작).
    int softDropCounterTicks = 0;

    // T-spin 판정 상태. 마지막 "성공한" 위치 변경이 회전이면 다음 lock 에서
    // T-piece pivot 주변 네 모서리 중 3개 이상이 막힌 경우 T-spin 으로 본다.
    bool lastMoveWasRotate = false;

    // Combat state
    int attackLinesSent = 0;
    int pendingGarbage = 0;
};
```

### B.2 테트로미노 — `src/sim_block.h`

**현재 소스 발췌 — `src/sim_block.h`**

```cpp
#pragma once
#include <vector>
#include <map>
#include "position.h"

// 테트로미노 하나의 상태. 그리는 방법은 모른다.
// Holds shape data (cells per rotation), position offsets, and rotation state.
// Used by SimGame for deterministic simulation (Colab training + Windows inference).
class SimBlock
{
public:
    SimBlock() : id(0), rotationState(0), rowOffset(0), columnOffset(0) {}

    void Move(int rows, int columns)
    {
        rowOffset += rows;
        columnOffset += columns;
    }

    std::vector<Position> GetCellPositions() const
    {
        const std::vector<Position>& tiles = cells.at(rotationState);
        std::vector<Position> movedTiles;
        movedTiles.reserve(tiles.size());
        for (const Position& item : tiles)
        {
            movedTiles.emplace_back(item.row + rowOffset, item.column + columnOffset);
        }
        return movedTiles;
    }

    void Rotate()
    {
        rotationState++;
        if (rotationState == static_cast<int>(cells.size()))
        {
            rotationState = 0;
        }
    }

    void UndoRotation()
    {
        rotationState--;
        if (rotationState == -1)
        {
            rotationState = static_cast<int>(cells.size()) - 1;
        }
    }

    // Public data — read by SimGame logic and by rendering wrappers.
    int id;
    std::map<int, std::vector<Position>> cells;
    int rotationState;
    int rowOffset;
    int columnOffset;

    // Accessors kept for state hash parity with old Block::GetRotationState etc.
    int GetRotationState() const { return rotationState; }
    int GetRowOffset() const { return rowOffset; }
    int GetColumnOffset() const { return columnOffset; }
};
```

`Rotate()` 는 시계 방향 한 방향만 돈다. `UndoRotation()` 이 별도로 있는 이유는 §3 에서 본 대로 — 회전해 보고 안 맞으면 되돌리는 방식이라, 되돌리기가 회전의 역연산으로 정확히 맞아떨어져야 한다.

### B.3 그리드 — `src/sim_grid.h`

**현재 소스 발췌 — `src/sim_grid.h`**

```cpp
#pragma once

// 20x10 보드. 그리는 방법은 모른다.
// Layout (int grid[kRows][kCols]) must match the old Grid class so that
// ComputeStateHash produces identical bytes when fnv1a64 is applied to the
// contiguous memory range.
class SimGrid
{
public:
    static constexpr int kRows = 20;
    static constexpr int kCols = 10;

    SimGrid() { Initialize(); }

    void Initialize()
    {
        for (int row = 0; row < kRows; row++)
        {
            for (int column = 0; column < kCols; column++)
            {
                grid[row][column] = 0;
            }
        }
    }

    bool IsCellOutside(int row, int column) const
    {
        if (row >= 0 && row < kRows && column >= 0 && column < kCols)
        {
            return false;
        }
        return true;
    }

    bool IsCellEmpty(int row, int column) const
    {
        // 방어적 경계 검사: 범위 밖 좌표는 '비어있지 않음'(막힘)으로 처리한다.
        // 호출부는 보통 IsCellOutside 로 선검사하지만, 만약 무경계 접근이 들어와도
        // OOB 읽기를 방지한다. 해시 대상은 grid 내용뿐이므로 결정성/리플레이 호환성에
        // 영향이 없다.
        if (IsCellOutside(row, column))
        {
            return false;
        }
        if (grid[row][column] == 0 || grid[row][column] == 8)
        {
            return true;
        }
        return false;
    }

    int ClearFullRows()
    {
        int completed = 0;
        for (int row = kRows - 1; row >= 0; row--)
        {
            if (IsRowFull(row))
            {
                ClearRow(row);
                completed++;
            }
            else if (completed > 0)
            {
                MoveRowDown(row, completed);
            }
        }
        return completed;
    }

    // Public: matches old Grid::grid layout for hash parity.
    int grid[kRows][kCols];

private:
    bool IsRowFull(int row) const
    {
        for (int column = 0; column < kCols; column++)
        {
            if (grid[row][column] == 0)
            {
                return false;
            }
        }
        return true;
    }

    void ClearRow(int row)
    {
        for (int column = 0; column < kCols; column++)
        {
            grid[row][column] = 0;
        }
    }

    void MoveRowDown(int row, int numRowsDown)
    {
        for (int column = 0; column < kCols; column++)
        {
            grid[row + numRowsDown][column] = grid[row][column];
            grid[row][column] = 0;
        }
    }
};
```

`MoveRowDown` 이 원본 행을 0으로 지우는 것에 주목하라. 이걸 빼면 위에서 내려온 행이 복사만 되고 원본이 남아 블록이 복제된다. §5 의 라인 클리어가 아래에서 위로 순회하는 것과 짝을 이루는 세부다.

### B.4 중력 — `SimGame::Tick`

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::Tick()
{
    if (gameOver) return;
    gravityCounterTicks++;
    if (gravityCounterTicks >= dropIntervalTicks)
    {
        gravityCounterTicks = 0;
        MoveBlockDown();
    }
}
```

이 장에서 **시간이 흐르는 유일한 지점**이다. `dropIntervalTicks` 는 아래의 `레벨 시스템과 T-spin` 규칙에 따라 줄어들고, `gravityCounterTicks` 가 거기 도달하면 한 칸 내린다. `Tick()` 을 호출하지 않으면 게임은 영원히 정지해 있다. 이 성질 덕분에 RL 환경은 중력 없이 배치만 반복할 수 있다.

### B.5 이동 — `MoveBlockRight` / `MoveBlockDown`

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::MoveBlockRight()
{
    if (gameOver) return;
    currentBlock.Move(0, 1);
    if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
    {
        currentBlock.Move(0, -1);
    }
    else
    {
        lastMoveWasRotate = false;
        ghostBlock = MakeGhostBlock(currentBlock);
    }
}
```

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
void SimGame::MoveBlockDown()
{
    if (gameOver) return;
    currentBlock.Move(1, 0);
    if (IsBlockOutside(currentBlock) || BlockFits(currentBlock) == false)
    {
        currentBlock.Move(-1, 0);
        LockBlock();
    }
    else
    {
        lastMoveWasRotate = false;
    }
}
```

세 이동 함수(`Left`/`Right`/`Down`)가 같은 형태를 공유한다: 옮겨 보고, 안 되면 되돌리고, 되면 `lastMoveWasRotate = false` 를 세운다. 그 한 줄이 T-spin 판정의 반쪽이다 — 회전으로 끼워 넣은 것과 밀어 넣은 것을 구분한다.

`MoveBlockDown` 만 다른 점이 하나 있다. 되돌린 뒤 **`LockBlock()` 을 부른다.** 아래로 못 간다는 것은 바닥이나 다른 블록에 닿았다는 뜻이므로 거기서 굳는다. `MoveBlockLeft`/`Right` 는 벽에 막혀도 그냥 제자리에 있을 뿐이다.

### B.6 고스트 — `MakeGhostBlock`

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
SimBlock SimGame::MakeGhostBlock(const SimBlock& block) const
{
    SimBlock ghost = block;
    ghost.id = 8;
    return ghost;
}
```

`id = 8` 이 고스트의 표식이다. 실제 테트로미노는 1~7 이므로 렌더러가 8을 보면 반투명으로 그린다([Part 3](./part3-rendering-and-ui.md) 의 색 팔레트 참조). **낙하 위치 계산은 여기 없다** — `DropExpectation()` 이 따로 한다. 이 함수는 색만 바꾼 복사본을 만든다.

### B.7 배치 단위 API — `LegalPlacements` / `ApplyPlacement`

**현재 소스 발췌 — `src/sim_game.cpp`**

```cpp
std::vector<SimGame::Placement> SimGame::LegalPlacements() const
{
    std::vector<Placement> out;
    if (gameOver) return out;

    const int numRotations = static_cast<int>(currentBlock.cells.size());
    for (int rot = 0; rot < numRotations; rot++)
    {
        for (int col = 0; col < SimGrid::kCols; col++)
        {
            // Start from a fresh copy of the live piece.
            SimBlock test = currentBlock;
            // Rotate in place to the target rotation.
            while (test.rotationState != rot)
            {
                test.Rotate();
            }
            // Slide horizontally to the target column offset.
            int delta = col - test.columnOffset;
            test.columnOffset += delta;
            // Reject if the rotated & translated piece is invalid at spawn height.
            if (IsBlockOutside(test) || !BlockFits(test)) continue;
            // Hard drop simulation.
            while (IsBlockOutside(test) == false && BlockFits(test) == true)
            {
                test.rowOffset++;
            }
            test.rowOffset--;
            if (IsBlockOutside(test) || !BlockFits(test)) continue;
            out.push_back({col, rot});
        }
    }
    return out;
}

int SimGame::ApplyPlacement(int col, int rot)
{
    if (gameOver) return -1;

    // Build target configuration from the live currentBlock.
    SimBlock target = currentBlock;
    const int numRotations = static_cast<int>(target.cells.size());
    if (rot < 0 || rot >= numRotations) return -1;
    while (target.rotationState != rot)
    {
        target.Rotate();
    }
    int delta = col - target.columnOffset;
    target.columnOffset += delta;
    if (IsBlockOutside(target) || !BlockFits(target)) return -1;
    // Hard drop
    while (IsBlockOutside(target) == false && BlockFits(target) == true)
    {
        target.rowOffset++;
    }
    target.rowOffset--;
    if (IsBlockOutside(target) || !BlockFits(target)) return -1;

    // Snapshot cleared-line count before lock. Score is level-scaled, so it
    // cannot be inverted back to a line count after level 1.
    int linesBefore = totalLinesCleared;

    // Commit: overwrite currentBlock with the landed configuration and lock.
    currentBlock = target;
    lastMoveWasRotate = false;
    LockBlock();

    return totalLinesCleared - linesBefore;
}
```

이 둘은 게임 플레이에 쓰이지 않는다. [Part 8](./part8-python-rl.md) 의 RL 환경과 [Part 9](./part9-rl-onnx-bot.md) 의 인게임 봇이 쓰는 **배치 단위 액션 공간**이다. 한 틱의 입력 대신 "몇 번째 열에 몇 번 회전해서 떨어뜨릴까" 를 한 번에 지정한다.

`LegalPlacements()` 가 회전 수를 `currentBlock.cells.size()` 로 세는 것에 주의하라. O 블록은 네 회전이 모두 같은 모양이지만 `SimOBlock` 이 동일한 cells 를 4벌 등록하므로 **중복된 배치가 4배로 열거된다.** 중복 제거를 하지 않은 것은 의도적이다 — 액션 인덱스가 `(열, 회전)` 격자에 고정돼야 C++ 과 Python 이 같은 번호를 같은 배치로 해석한다.

`ApplyPlacement` 는 검증 없이 상태를 바꾸지 않는다. 배치가 불법이면 아무 일도 하지 않고 돌아가므로, 학습 코드가 마스킹에 실패해도 시뮬레이션이 망가지지 않는다.

---

## 이 장에서 완성된 것

- `src/sim_game.{h,cpp}`, `src/sim_grid.h`, `src/sim_block.h`, `src/sim_blocks.h`: 순수 C++ 테트리스 시뮬레이션 엔진.
- `core/rng.h`, `core/hash.h`: XorShift64* RNG와 FNV-1a 해시.
- `tests/sim_hash_dump.cpp`: 고정 스크립트로 결정론을 검증하는 회귀 테스트.
- Python 바인딩 (`python/sim/__init__.py`) 이 같은 C++ 심을 pybind11로 노출 — 동일한 스크립트가 Python 에서도 동일한 해시 시퀀스를 산출.

핵심 설계 결정:

- **렌더링 완전 분리**: `SimGame`은 화면을 모른다.
- **RNG 호출 지점 제한**: `GetRandomBlock()` 하나 + `InsertGarbage()`의 별도 `garbageRng` 스트림.
- **상태 해시**: 전체 게임 상태(+ 전투 상태)를 64비트로 압축해 네트워크 검증.
- **역순 라인 클리어**: 이중 시프트 방지.
- **일방향 참조**: 외부가 sim을 읽기만 하고 수정하지 않는다.
- **이벤트 플래그**: 일회성 이벤트를 mutable 플래그로 외부에 알리고, 외부가 클리어 책임.
- **정수만 사용**: 부동소수는 렌더/오디오에만, sim에는 절대 금지.

## 수동 테스트

이 장의 완료 게이트는 골든 해시 일치다. 게임 클라이언트는 아직 없으므로 **`TETRIS_BUILD_GAME` 을 반드시 끄고** 빌드한다 — 기본값이 ON 이라 켜진 채로 두면 아직 만들지 않은 `src/game.cpp`, `renderer/`, `net/` 을 찾다가 configure 단계에서 죽는다.

```bash
cmake -S . -B build -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_TEST=ON
cmake --build build --target sim_hash_dump
./build/sim_hash_dump | diff - python/tests/_sim_hash_dump.txt && echo "결정론 OK"
```

기대 결과: `diff` 가 아무것도 출력하지 않고 `결정론 OK` 한 줄이 찍힌다. Windows(MSVC)라면 `--config Release` 를 붙이고 실행 파일은 `build/Release/sim_hash_dump.exe` 다.

출력 자체를 눈으로 보려면:

```bash
./build/sim_hash_dump | head -8
```

기대 결과: 각 기본 시드마다 `==== seed 0x...` 헤더, 초기 해시, 입력 스크립트의 단계별 해시, 최종 해시가 나온다. 정확한 시드·스텝 목록은 `tests/sim_hash_dump.cpp`와 골든 파일이 소유하며, 문서는 출력 형태만 고정한다. 첫 줄들은 이렇게 시작한다.

```text
==== seed 0x0000000000000001 ====
seed=0x0000000000000001
initial_hash=0xfb7249998a4f8ed6
step=000 mask=0x00 ticks=30 total_ticks=30 score=0 over=0 hash=0x2029ed42eb3493d7
step=001 mask=0x01 ticks=1 total_ticks=31 score=0 over=0 hash=0x9c0bc617492ab83f
```

시드를 바꿔 보고 싶으면 인자로 넘긴다. `tests/sim_hash_dump.cpp` 가 argv 를 16진 시드로 파싱한다.

```bash
./build/sim_hash_dump 0xABCDEF | head -4
```

### 골든 해시가 지키는 것과 지키지 못하는 것

이 게이트는 강력하지만 만능이 아니다. **`kScript` 가 밟는 경로만** 지킨다.

예를 들어 §B.5 의 `MoveBlockLeft` 에서 `lastMoveWasRotate = false;` 한 줄을 지우고 다시 돌려 보면 — 해시는 **여전히 일치한다.** 스크립트에 "회전한 직후 좌우로 밀고 나서 잠그는" 시퀀스가 없어 그 줄이 결과를 바꾸는 상황에 도달하지 못하기 때문이다. 반대로 `RotateBlockImpl` 의 `lastMoveWasRotate = true;` 를 지우면 `step=004` 부터 즉시 갈라진다.

즉 이 테스트는 **회귀 방지 장치**이지 정확성 증명이 아니다. 새 규칙을 추가할 때는 그 규칙을 밟는 스텝을 `kScript` 에 함께 넣어야 그때부터 잠긴다.

### Python 측 교차 검증

완성형 저장소의 pybind11 모듈을 사용하면 Python 쪽에서도 같은 해시를 확인할 수 있다. 이 검증은 C++ 코어와 Python 학습 환경이 동일한 규칙을 실행한다는 계약까지 확인하려는 경우에 선택한다.

```bash
cmake -S . -B build-py -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_PY=ON \
      -Dpybind11_DIR=$(uv run python -m pybind11 --cmakedir)
cmake --build build-py --target tetris_py
cp build-py/tetris_py*.so python/sim/          # Windows: build-py\Release\tetris_py*.pyd
uv run python -m pytest python/tests/test_determinism_crossplatform.py -v
```

기대 결과: `python/tests/test_determinism_crossplatform.py`가 수집한 결정론 계약이 모두 통과한다. 네이티브 모듈이나 골든 파일 부재로 skip되지 않았는지 `-rs`로 함께 확인한다.

### 통합 환경에서 확인하는 것

가비지 교환이 두 클라이언트에서 같은 구멍 컬럼을 만드는지는 relay 통합 테스트와
실제 두 클라이언트 실행으로 확인한다. `garbageRng` 자체의 결정론은 골든 해시의
`combat` 섹션이 고정한다.

소프트 드롭 레이트와 레벨 표시는 화면에서 초시계로도 확인할 수 있다. 헤드리스
환경에서는 `sim_hash_dump` 의 `ticks=` 열을 보고 DOWN 입력의 틱 소비가 기대와
맞는지 확인한다. `SubmitInput`과 `Tick`은 게임 루프와 lockstep의 고정 스텝 단위이고,
`StateHash`는 피어 간 desync 감지에 쓰인다. `ApplyPlacement`는 같은 규칙을 배치 수준
행동으로 표현해 Python 학습 환경이 사용한다.

---

## 참고 자료

1. **Tetris Guideline** (The Tetris Company). 7-Piece Bag Randomizer, Super Rotation System 정의
2. **George Marsaglia**, "Xorshift RNGs" (2003, Journal of Statistical Software, Vol 8, Issue 14). xorshift 계열 알고리즘과 주기 분석
3. **Sebastiano Vigna**, "An experimental exploration of Marsaglia's xorshift generators, scrambled" (2016). xorshift64*의 곱셈 상수 선택과 통계적 품질
4. **Fowler-Noll-Vo hash** (www.isthe.com/chongo/tech/comp/fnv/). FNV-1a 64-bit의 초기값, 소수, 충돌 특성
5. **NES Tetris scoring** (Tetris Wiki, tetris.wiki/Scoring). 원작 NES 점수 체계 (레벨 x 라인 보너스)
6. **"Game Programming Patterns"** (Robert Nystrom, 2014). Chapter 2 "Command" — 입력을 커맨드 객체로 추상화하는 패턴
7. **Tetr.io / Jstris attack tables**. 현대 경쟁 테트리스의 공격 테이블 구조. T-spin / B2B / Combo 보너스 설계 레퍼런스
8. **splitmix64** (Sebastiano Vigna, xorshift.di.unimi.it). 황금비 상수 `0x9E3779B97F4A7C15` 의 유도 — 독립 RNG 스트림 분기에 사용
