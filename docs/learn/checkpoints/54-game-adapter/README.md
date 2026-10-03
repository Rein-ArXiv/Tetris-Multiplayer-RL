# 54 — 어댑터·값 뷰·소유권

client/game.h가FrameRunner와표현난수를값으로소유합니다. presentation/game_view.h는
보드·활동조각·고스트·미리보기·점수·종료이유를값으로복사합니다. view호출은규칙이나
표현엔진을소비하지않습니다. main의실제GPU준비가이뷰를사용합니다.
Round/FrameRunner/골든규칙은유지하며GL자원은렌더스코프가소유합니다.

```sh
cmake -S docs/learn/checkpoints/54-game-adapter -B out/study-54 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-54 --target adapter_demo adapter_contract golden_trace
./out/study-54/adapter_demo
ctest --test-dir out/study-54 -R '^(adapter_contract|golden_)' --output-on-failure
```

0.017초하드드롭요청은1틱을진행합니다. 이후100번조회는규칙해시·표현엔진을유지합니다.
한배치두잠금은표현샘플두번이며마지막색을유지합니다. 실패프레임은효과도소비하지않습니다.
표현값뷰는미래규칙복원파일이아닙니다. CPU Game은값멤버여서독립복사가가능합니다.

```sh
cmake -S docs/learn/checkpoints/54-game-adapter -B out/study-54-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-54-sdl --target tetris
./out/study-54-sdl/tetris --seed 1
```

화면에는SDL/OpenGL3.3 Core가필요합니다. CPU검사는장치를요구하지않습니다.
루트의Game은오디오핸들·자기sim참조별칭을소유하므로복사/이동을삭제하고unique_ptr로관리합니다.
전체검증은 `python3 scripts/check_learning_adapter.py`. 골든도구에는Python3.10이상필요.
