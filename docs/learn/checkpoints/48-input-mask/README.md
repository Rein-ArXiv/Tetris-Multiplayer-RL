# 48차시 · 비트마스크와 입력 변환

47-input-edges의 플랫폼·규칙·FrameRunner는 유지하고 PendingControls의 저장 표현을 바꿉니다.
Intent를 별도 헤더로 분리하고 input_mask.h에 비트 규약·검증·변환을 둡니다.
consume_mask는 raw 비트를, consume은 양쪽 방향을 중립으로 해석한 Intent를 반환합니다.
마스크는 발생 여부를 합치며 처리 순서·반복 횟수를 담지 않습니다.

```sh
cmake -S docs/learn/checkpoints/48-input-mask -B out/study-48 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-48 --target mask_demo mask_contract frame_contract
./out/study-48/mask_demo
ctest --test-dir out/study-48 -R '^(mask_contract|frame_contract)$' --output-on-failure
```

SDL 구성의 input_pipeline은 dummy 드라이버에서 실제 SDL 사건을 틱까지 전달합니다.
게임 화면은 SDL 구성의 tetris 타깃과 OpenGL3.3 Core 실행 환경을 사용합니다.
현재 루트의 비트 위치와 맞추되, 실제 SimGame의 좌우 순차 시도 정책까지 같다고 가정하지 않습니다.
