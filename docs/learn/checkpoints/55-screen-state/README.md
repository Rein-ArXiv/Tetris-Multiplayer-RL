# 화면 전환과 재시작

54-game-adapter에서 이어지는 누적 C++17 게임이다. `client/screen_transition.h`는
순수 전이표, `client/application.h`는 라운드 수명·입력 게이트·화면 진행을 소유한다.
Game/FrameRunner/규칙과 GPU 자원 구현은 유지한다.

```bash
cmake -S docs/learn/checkpoints/55-screen-state -B out/study55 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study55 -j3
ctest --test-dir out/study55 --output-on-failure
out/study55/screen_demo
out/study55/tetris --seed 1
```

메뉴의 주황색 재생 삼각형에서 Space로 시작한다. 플레이 중 Space는 하드 드롭,
Escape는 메뉴 복귀다. 종료된 보드에서 Space는 같은 초기 설정으로 재시작한다.
메뉴에서 Escape는 종료한다. 게임 조작 키를 모두 떼는 프레임을 거친 뒤 새 조작을
받으며, 시작·재시작·복귀 프레임에는 규칙을 진행하지 않는다. 처음 막힌 보드처럼
종료 상태인 기준으로 시작하면 곧바로 종료 화면으로 간다.

CPU 데모/계약 검사는 SCRIPTED로도 빌드 가능하다. 실제 창과 GL 게임은 SDL 경로다.
누적 골든 검사를 위해 Python 3.10 이상이 필요하다. 화면은 메뉴 삼각형·보드·종료 X와
창 제목의 조작 안내로 구분한다. 폰트·버튼 메뉴를 완성한 단계는 아니다.

재시작 기준은 생성자에 준 Round 값이다. Game을 다시 만들면 시계 위상·보류 입력·
표현 난수도 초기 상태가 된다. GL 객체는 유지하되 모든 보드·조각·고스트·미리보기
표시 자료를 갱신한다. Application::game()의 빌린 포인터는 전환을 넘어 보관하지 않는다.

검사: `python3 scripts/check_learning_screen.py`.
