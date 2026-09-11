# 외형과 게임 규칙을 어디서 바꾸는가

> 캐릭터별 모델·속도·일러스트와 서버 검증 BP는 [봇과 Colab 안내](bots-and-colab.md)를 먼저 보세요.

2026-09-11 기준. 엔진을 다시 만들 필요는 없다. **화면에 보이는 것**은 표현 계층에서,
**승패·블록 움직임에 영향을 주는 것**은 `SimGame`에서 수정한다.

## 아이콘·작은 사각 프레임·애니메이션

이미 있는 `assets/images.cfg`에서 파일 경로를 바꾼다:

```ini
player_icon = assets/icons/player.png
opponent_icon = assets/icons/opponent.png
bot_icon = assets/icons/bot.png
icon.ruby = assets/icons/opponent.png
icon.gold = assets/icons/bot.png
```

투명 PNG를 권장한다. 기본 슬롯은 32×32, 대기 화면은 64×64다. 새
`src/presentation.cpp`의 `presentation_draw_avatar()`가 사각 프레임 안에 원본 비율을
보존해 넣는다. 아이콘이 가로로 긴 경우도 찌그러뜨리지 않는다. 파일을 못 읽으면
`src/main.cpp`의 기존 기본 경로 → 내장 아이콘 순서로 복구한다.

`assets/theme.cfg`는 다음을 제어한다. 저장하고 게임을 다시 실행한다:

```ini
font = Font/NanumGothic.ttf
avatar.player = 91,203,238,255
avatar.opponent = 239,118,154,255
avatar.period_ms = 2400
cell.1 = 47,230,23,255
```

색은 RGBA 0~255 네 값이다. 주기는 500~10000ms다. 잘못된 값은 로그를 남기고
기본값을 유지한다. 플레이어와 상대의 테두리는 가만히 있어도 천천히 밝아졌다
어두워진다. 위치·크기·게임 입력 판정은 움직이지 않는다.
`Settings → UI animation`을 끄면 테두리와 기존 상점 회전도 정지한다.
사용자 설정은 `settings.cfg`의 `idle_animation=0/1`로 저장된다.

새 아이콘을 판매하려면 두 군데를 함께 바꾼다:

1. `meta/database.cpp`의 `kIconCatalog`: 안정적인 ID, 표시 이름, BP 가격, 기본 소유 여부.
2. `assets/images.cfg`의 `icon.<같은 ID>`와 실제 이미지 파일.

이미 판매한 ID를 다른 의미로 재사용하면 기존 소유권이 바뀐다. ID는 유지하고 새
항목을 추가한다. 소유권·구매·선택은 서버가 검증한다. 로컬 이미지 교체는 치장일 뿐
서버 구매 기록을 바꾸지 않는다. 이번 변경은 사용자 이미지 업로드 기능을 만들지 않는다.

## 폰트

새 TTF를 `Font/`에 넣고 `theme.cfg`의 `font` 경로를 바꾼다. 로드 실패 시
`NanumGothic.ttf`로 돌아간다. 렌더러는 한 번에 한 폰트를 쓰며 글리프별 다중 폰트
fallback, 복잡한 문자 조형, 컬러 이모지는 구현돼 있지 않다. 한글·숫자·영문과
사용하는 특수문자가 모두 들어 있는 폰트로 실제 화면을 확인한다.

글꼴 폭이 달라지므로 Settings, 긴 봇 이름, Customize, 대전 결과, 720×640 화면을
확인한다. `measure_text()`로 가운데 정렬하는 요소는 자동 반영되지만 고정 좌표
요소는 `src/main.cpp`와 `src/gui.cpp`를 조정해야 한다. 배포 가능한 라이선스의
폰트·아이콘·사운드를 사용하고 필요한 고지를 번들에 포함한다.

## 블록 색과 모양은 서로 다른 변경

| 바꾸려는 것 | 수정 위치 | 대전/AI 영향 |
|---|---|---|
| 블록 색 | `assets/theme.cfg`의 `cell.<ID>` | 없음 |
| 칸 그리기·광택·모서리 | `src/game.cpp`: `DrawGrid`, `DrawBlock`, `DrawBlockMini` | 표현만 변경하면 없음 |
| 한 블록을 이루는 칸 좌표 | `src/sim_blocks.h`, `src/sim_block.h` | 규칙·회전·충돌·해시 변경 |
| 블록 종류·출현 비율 | `src/sim_game.cpp`: `GetAllBlocks`, `GetRandomBlock` | bag/RNG·리플레이·관측/행동 변경 |
| 보드 가로/세로 칸 수 | `src/sim_grid.h`: `kCols`, `kRows` | 충돌·화면·AI 관측 크기 변경 |
| NEXT 표시 개수 | `kNextPreviewCount`/`NextBlocks()` 사용 지점과 UI | 큐 길이를 바꾸면 RNG 소비·해시까지 영향 |
| 낙하 속도·잠금·가비지 공격 | `src/sim_game.cpp`, `core/constants.h` | 양쪽 클라이언트·봇에 동일 규칙 필요 |

현재 ID는 **0 빈칸 / 1 L / 2 J / 3 I / 4 O / 5 S / 6 T / 7 Z / 8 고스트 / 9 가비지**다.
새 8번째 블록에 ID 8을 배정하면 고스트 의미와 충돌한다. `cellColors` 인덱스,
`SimGrid::IsCellEmpty`, 회전 특례, 바인딩, Python 관측·행동 인코딩을 함께 검토한다.
단순히 `GetAllBlocks()`에 클래스를 추가하고 끝내면 안 된다.

규칙을 바꾸는 작업은 다음 순서로 한다:

1. 새 규칙의 블록 ID·좌표·보드 크기·출현 정책을 먼저 확정한다.
2. `SimGame`과 headless 테스트에서 충돌·회전·가비지·종료를 검증한다.
3. `bot/placement.cpp`, `bindings/tetris_py.cpp`, `python/common/`, `python/sim/`의
   고정 크기·ID 가정을 맞추고 C++/Python parity를 확인한다.
4. 골든 해시와 replay 호환성 변경을 검토한다. 단순히 실패한 해시를 덮어쓰지 않는다.
5. 모델은 새 관측/행동/보상 규칙에 맞게 다시 학습하고 export한다.
6. 서로 다른 규칙의 클라이언트가 매칭되지 않도록 **ruleset 버전 협상/거절**을 추가한다.
   현재는 그런 호환성 게이트가 없어 같은 빌드 배포가 운영 전제다.

이번 폴리싱은 게임 규칙을 그대로 두고 표현 설정만 분리했다. 아직 정해지지 않은
새 블록 개수나 규칙을 임의로 결정하지 않았다.

## 전체 코드를 읽지 않고 작업하는 방법

`src/main.cpp`는 화면 전환·입력·네트워크 상태를 함께 관리하는 큰 파일이다.
아이콘을 바꾸려고 여기서부터 모두 읽을 필요는 없다. 현재의 작은 확장 경계는:

```text
assets/images.cfg + theme.cfg
             ↓
src/presentation.cpp → renderer/* (그림)
             ↑
src/main.cpp (어느 화면/누구의 아이콘인가)
src/game.cpp (어느 칸을 그리는가) ← SimGame (규칙)
```

다음 UI 작업은 `presentation_draw_avatar()`처럼 그리기 함수부터 분리한다.
네트워크 세션·시뮬레이션 상태를 새 위젯 안으로 옮기지 않는다. 향후 화면별 분리는
Menu → Settings → Customize 순으로 해도 온라인 lockstep을 다시 설계할 필요가 없다.
