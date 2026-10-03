# 22차시 기준 코드 — 색과 알파 합성

21-quad를 이어 프레임 초기화와 제출을 분리하고 반투명 사각형 두 개를 합성한다.
HTML 강의의 핵심 스니펫, 짧은 복습, 즉시 해설과 함께 읽는다.

## 빌드와 실행

저장소 루트에서 SDL2 개발 패키지·pkg-config·GL 3.3 Core 환경을 사용한다.
단일 구성 생성기 기준이다. 다중 구성은 구성 이름과 실행 파일 경로를 맞춘다.

```sh
cmake -S docs/learn/checkpoints/22-blending -B out/study-22 -DSTUDY_PLATFORM=SDL
cmake --build out/study-22
ctest --test-dir out/study-22 --output-on-failure
./out/study-22/blend_demo
./out/study-22/tetris ab
./out/study-22/tetris ba
./out/study-22/tetris premul
./out/study-22/tetris double-alpha
./out/study-22/tetris replace
./out/study-22/tetris legacy-alpha
./out/study-22/blend_probe
```

창은 하나씩 실행하고 Escape로 닫는다. Linux가 offscreen GL을 지원하면 probe에
SDL_VIDEODRIVER=offscreen을 설정할 수 있다. probe에 기존 폴더 안의 파일 접두사를
인자로 주면 여섯 모드의 불투명 검정 배경 RGB 결과를 PPM으로 저장한다.
별도 폴더에서 SCRIPTED를 선택하면 CPU 도구와 계약 검사를 실행한다.

## 추가·변경 파일

- renderer/blend.h: CPU LDR source-over, straight→선곱 변환과 입력 거부.
- renderer/triangle.*: begin_color_frame/submit_triangles 분리. 기존 wrapper는 매번 clear한다.
- renderer/blend_scene.h: 두 사각형의12정점, 진단 shader, 합성 상태와 제출 순서.
- renderer/gl_api.*: BlendEquation/BlendFuncSeparate 추가, 총43엔트리.
- src/main.cpp: 두 도형을 한 프레임에서 그리는 실험 모드.
- src/blend_demo.cpp: 두 순서·두 배경 알파의 CPU 수치 비교.
- tests/blend_contract.cpp: 수식, alpha0/1, 표현 동등성, 유효하지 않은 입력.
- tests/triangle_contract.cpp: clear 한 번/두 draw, 실패 중단·해제·범위 거부.
- tests/blend_real.cpp: 여섯 모드×배경alpha0/1, 모든 픽셀의 RGBA 읽기.
- tests/loader_contract.cpp·CMakeLists.txt: 새 함수 누락 및 도구/검사 타깃 연결.

## 유지하는 계약

- 정점은 Vertex2,12개96바이트. stride는8바이트. VAO의 buffer와 available은 일치해야 한다.
- source-over CPU 출력은 선곱 RGBA. RGB≤alpha는 비음수 LDR 정책이며 HDR 일반 규칙이 아니다.
- 알파0의 straight RGB는 복원 불가. 알파가 양수일 때만 나눌 수 있다.
- 포인터/정점 버퍼 소유권과 컨텍스트 종료 순서는 그대로다.
- begin은 clear, submit은 clear 없이 draw와 program/VAO 선택 해제만 한다.
- 단일 샘플·전체 색 쓰기·FILL·scissor/depth/stencil 비활성인 전용 패스다.
- configure는 culling/sRGB/dither를 끄고 블렌드 상태를 명시한다. 이전 상태를 복원하지 않는다.
- 이 수치 합성 실험은 모니터까지의 sRGB 색 관리 시스템이 아니다.
- main은 더블 버퍼를 유지한다. probe는 단일 버퍼를 요청하고 실제 버퍼를 읽는다.
- probe는 alpha8비트 이상을 요구한다. RGBA8 readback의 채널별 허용 오차는2이다.
- 실제 창의 투명도나 GPU 완료/화면 표시 시점은 framebuffer alpha와 별개다.
