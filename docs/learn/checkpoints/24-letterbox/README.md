# 24차시 누적 체크포인트 — 창 크기와 입력 좌표

고정 논리 320×240의 두 사각형을 drawable 안에 배치하고, 같은 사각형으로
SDL 창 좌표를 논리 좌표로 되돌린다. main은 Escape/닫기까지 지속 실행한다.

```sh
cmake -S docs/learn/checkpoints/24-letterbox -B out/study-24 -DSTUDY_PLATFORM=SDL
cmake --build out/study-24
ctest --test-dir out/study-24 --output-on-failure
./out/study-24/letterbox_demo
./out/study-24/tetris
./out/study-24/letterbox_probe
./out/study-24/presentation_demo submit 1
```

저장소 루트·C++17·SDL2 개발 패키지·pkg-config·GL3.3 Core가 필요하다.
다중 구성 생성기는 구성 이름과 실행 경로를 맞춘다. SCRIPTED/축소WIN32는 입력
전용이며 별도 빌드 폴더에서 CPU 도구를 사용한다. presentation_demo는 이전
시간 비교 도구로 유효한 swap 요청120개 뒤 종료한다. main의 GL더블버퍼 요구는 유지한다.

## 구현 지도

- renderer/letterbox.h: 순수 CPU fit·GL y·양방향 좌표 변환. 창/drawable/논리 크기 구별.
- renderer/letterbox_scene.h: 논리 사각형 정점·전체 검정 clear·scissor 내부 clear·두 draw.
- renderer/gl_api.*: Scissor 추가, 총46개 필수 엔트리와 ABI.
- platform/platform.h 및 세 백엔드: WindowSize/WindowMouse 조회 계약.
- platform/sdl.cpp: 이벤트 처리 후 SDL 창 크기·마우스 포커스/좌표를 조회한다.
- src/main.cpp: 같은 Layout으로 렌더링과 입력 검사, 크기 변경/마우스 로그.
- src/letterbox_demo.cpp: 합성 크기 입력과 여백/중심 숫자를 출력한다.
- src/presentation_demo.cpp: 제출/완료/교체 시간 실험을 독립 실행 파일로 보존한다.
- tests/letterbox_contract.cpp: 반열린 입력·홀수 여백·축별 배율·극단값·왕복 검사.
- tests/letterbox_real.cpp: 다섯 크기의 새 창에서 실제 GL RGB 전체 픽셀 비교.
- tests/platform_letterbox.cpp: SDL 호출 대역으로 창/픽셀/입력의 서로 다른 단위를 검사.

## 적용 범위

Layout은 make_layout이 만든 값을 변경하지 않고 사용한다. 입력은 영역 밖이면
nullopt이며 임의로 0에 고정하지 않는다. 정방향 경계와 반열린 hit 범위를 구분한다.
내림 정책으로 정수 사각형을 고르므로 종횡비에는 픽셀 양자화 오차가 남는다.
GL 패스는 기본 framebuffer·기본 쓰기 마스크·꺼진 depth/stencil을 전제로 하며
viewport/scissor 상태를 남긴다. 지원 viewport 크기 초과는 거부한다.

GL probe는 단일 샘플 픽셀 실험이며 모니터 표시·실제 DPI·native resize 검사가 아니다.
지원되는 Linux의 offscreen 실행은 `SDL_VIDEODRIVER=offscreen`을 앞에 붙인다.
통합 검사는 저장소 루트에서 `python3 scripts/check_learning_letterbox.py`로 실행한다.
환경별 증거와 한계는 PROGRESS.md·REVIEW_LOG.md에 기록한다.
