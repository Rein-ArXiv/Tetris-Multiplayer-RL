# 52 — 상태·바이트·해시

LRND/1은 고정 규칙 아래 지속적인 Round 상태를 명시적 little-endian 바이트로 기록합니다.
core/canonical_bytes.h와 simulation/state_hash.h를 추가하고 공급기의 const 관찰 함수를
보강합니다. Round 전이는 유지하며 main은 틱 배치 뒤 최종 규칙 상태를 한 번 기록합니다.
중간 틱의 해시로 읽지 않습니다. 표현·파생 ghost·마지막 보고·FrameRunner 상태는 제외합니다.

```sh
cmake -S docs/learn/checkpoints/52-state-hash -B out/study-52 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-52 --target hash_demo hash_contract
./out/study-52/hash_demo
ctest --test-dir out/study-52 -R '^hash_contract$' --output-on-failure
```

시드1의 tick0/1/2는342바이트, 행0, 중력0/1/2이며 해시는
c799419b70bbe2de / c3409b53b6d47de1 / 32a79d4c3c185c58입니다.
규칙 또는 바이트 필드의 의미를 바꾸면 형식 버전도 검토합니다.

```sh
cmake -S docs/learn/checkpoints/52-state-hash -B out/study-52-sdl -DSTUDY_PLATFORM=SDL
cmake --build out/study-52-sdl --target tetris
./out/study-52-sdl/tetris --seed 1
```

SDL/OpenGL3.3 Core가 있는 환경에서 실제 게임을 실행합니다. CPU검사는 창을 요구하지 않습니다.
전체 검증은 저장소 루트에서 `python3 scripts/check_learning_hash.py`로 실행합니다.
루트게임의 StateHash는legacy 통신 형식을 유지하고 DiagnosticStateHashV2는SIMH/2로
가방을 더합니다. LRND/1과 SIMH/2는서로다른 규칙/형식이므로 숫자 자체를 비교하지 않습니다.
해시는 동등성 증명·역직렬화·클라이언트 인증 기능이 아닙니다.
