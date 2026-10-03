# 23차시 기준 코드 — 제출·완료·표시

22-blending의 합성 그림을 유지하며 GL 진행/완료 경계, swap 요청 진단,
CPU 호출 구간 시간을 추가한다. HTML 강의의 스니펫과 함께 읽는다.

## 빌드와 실행

저장소 루트·SDL2 개발 패키지·pkg-config·GL3.3 Core, 단일 구성 생성기 기준이다.
다중 구성 생성기는 구성 이름과 실행 경로를 맞춘다.

```sh
cmake -S docs/learn/checkpoints/23-present -B out/study-23 -DSTUDY_PLATFORM=SDL
cmake --build out/study-23
ctest --test-dir out/study-23 --output-on-failure
./out/study-23/timing_demo
./out/study-23/tetris submit 1
./out/study-23/tetris flush 1
./out/study-23/tetris finish 1
./out/study-23/tetris submit 0
./out/study-23/present_probe
```

창은 하나씩 실행한다. 유효한 swap 요청120개 뒤 종료하며 Escape/닫기로 먼저 끝낼 수 있다.
최소화로 생략된 요청은120개에 포함하지 않는다. SCRIPTED는 별도 폴더에서 CPU 도구와
계약 검사를 실행한다. 입력 전용 WIN32/SCRIPTED에는 GL 창을 추가하지 않았다.
Linux가 지원하면 probe에 SDL_VIDEODRIVER=offscreen을 설정할 수 있다.
Linux SDL 호출 대역 검사는 저장소 루트의 scripts/check_learning_present.py로 실행한다.

## 구현 지도

- renderer/cpu_timing.h: 같은 시계의 네 정수 시점을 검증하고 세 경과 구간으로 변환한다.
- renderer/submission.h: submit/flush/finish 파싱과 GL 오류/관찰 경계.
- renderer/gl_api.*: Flush/Finish void(void), 총45개 필수 엔트리.
- platform/platform.h: SwapIntervalReport와 bool present의 정확한 의미.
- platform/sdl.cpp: 세션/current 창·컨텍스트/양수 drawable을 확인하고 요청한다.
- platform/scripted.cpp·win32.cpp: 입력 전용 백엔드의 미시도/미제출 반환.
- src/main.cpp: 같은 그림의 세 구간 시간을 저장하고 측정 구간 밖에서 출력한다.
- src/timing_demo.cpp: 큰 시점의 작은 차이를 보여 주는 합성 입력.
- tests/present_contract.cpp: 큰 시점·역행·주파수0·동일시점·모드와 오류 경계.
- tests/present_real.cpp: 실제 GL 세 모드의 시간 구간 뒤에 RGB를 읽어 확인한다.
- tests/platform_present.cpp: Linux 전용 SDL 대역으로 호출 정책을 검사한다.
  대역 구현은 저장소 tests/learning/present_probe.c이고 공통 gl_probe.c를 사용한다.
- tests/loader_contract.cpp·CMakeLists.txt: 새 엔트리와 실행/검사 타깃 연결.

## 계약

- GL Flush는 진행을 보장하지만 완료까지 기다리는 Finish와 다르다. 둘 다 모니터 표시 완료가 아니다.
- glb_flush는 애플리케이션의 정점 배치 제출 함수이며 glFlush가 아니다.
- interval0/1 요청과 SDL 보고값을 분리한다. GetSwapInterval의0에는 알 수 없음도 포함된다.
- present의true는 SDL swap API를 호출했다는 뜻이다. SDL2 함수가 void이므로 native 성공을 인증하지 않는다.
- current 세션이 다르거나 inactive/closing/양수drawable이 아니면 요청하지 않는다.
- false 경로에서도 end_frame의 소프트웨어 페이싱을 건너뛰지 않는다.
- end_frame의60Hz목표는 interval0에서도 유지된다. interval0이 무제한FPS를 의미하지 않는다.
- 다음 back buffer 내용 보존을 가정하지 않고 매 프레임 다시 그린다.
- 시간은 정수 차이를 먼저 구한다. 단위 나노초가 실제1ns해상도를 뜻하지 않는다.
- CPU 경과 시간에는 드라이버 대기/스케줄링 등이 포함될 수 있다. GPU시간·CPU사용시간·표시시각과 다르다.
- probe의ReadPixels는 측정 뒤에 있다. 읽기 자체가 기다릴 수 있어 앞선 반환시각의 완료 증거로 쓰지 않는다.
- main은 doublebuffer1을 요구한다. probe는 실제 속성을 보고 단일버퍼이면 swap 경로를 생략한다.
- 실제화면/물리GPU/OS별 검증 증거는 PROGRESS·REVIEW_LOG에 분리해 기록한다.
