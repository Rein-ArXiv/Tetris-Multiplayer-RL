# 18차시 — 첫 삼각형과 프레임 경계

17-program에 draw_triangle과 픽셀 크기 조회·표시를 추가한다. VBO·VAO·Program은
초기화 때 준비하며 매 프레임 재생성하지 않는다. 새 GL 컨텍스트의 기본 그리기
상태를 유지하는 전용 실습이다. GL 상태를 임의로 바꾼 다른 렌더러와 바로 섞지 않는다.

## 연결할 파일

- renderer/triangle.h/cpp: 소유권 없는 선택 → viewport → clear → draw → 선택 해제.
- renderer/gl_api.h/cpp: draw4함수와 관찰용 read2함수, 총37개 진입점.
- renderer/shader_sources.h: fragment 색상 출력 location0 명시.
- platform/platform.h, sdl.cpp: DrawableSize와 present, 고해상도·리사이즈 창.
- platform/scripted.cpp, win32.cpp: 입력 전용 백엔드의 크기0·present 무동작 구현.
- src/main.cpp: 픽셀 크기가 양수일 때 draw/present하고 프레임을 마무리.
- tests/triangle_contract.cpp: 인자 단위·호출 순서·8개 실패 경계·정리 검사.
- tests/triangle_real.cpp: 정상 내부/배경 픽셀, count2, draw 뒤 clear, viewport0 실험.

## 실행

저장소 루트, SDL2 개발 패키지·pkg-config·GL3.3 Core 필요. Linux 단일 구성 경로:

```sh
cmake -S docs/learn/checkpoints/18-triangle -B out/study-18 -DSTUDY_PLATFORM=SDL
cmake --build out/study-18
ctest --test-dir out/study-18 --output-on-failure
./out/study-18/tetris
SDL_VIDEODRIVER=offscreen ./out/study-18/triangle_probe
```

창에는 남색 배경과 청록색 삼각형이 표시된다. Escape로 종료한다. 전체 drawable을
사용하므로 창 종횡비에 따라 모양이 늘어난다. 비율 유지는 별도 구현 대상이다.
Windows 다중 구성 생성기는 구성 이름과 Debug/Release 실행 경로를 맞춘다.

probe는 표시 대신 픽셀을 읽는다. 단일 버퍼를 요청하고 실제 버퍼 속성에 맞춰 읽기
대상을 고른다. main의 더블 버퍼 정책은 유지한다. offscreen GL을 지원하지 않는
환경에서는 그래픽 세션에서 환경변수 없이 probe를 실행한다. 인자로 PPM 경로를
주면 첫 정상 프레임을 저장한다. 이것은 프레임 버퍼 결과이며 모니터 화면 캡처가 아니다.

## 실습의 전제

- 기본 프레임 버퍼와 새 컨텍스트의 기본 테스트/쓰기 상태를 유지한다.
- draw는 정점3개를 읽는다.24바이트나 float6개를 count로 넘기지 않는다.
- clear는 viewport 크기로 제한되지 않는다. scissor와 쓰기 마스크는 별개다.
- frame 함수는 이전 프로그램/VAO를 복원하지 않는다. 선택을0으로 해제하는 정책이다.
- ReadPixels는 pixel-pack buffer 없이 CPU에 읽으며 기본 pack 설정을 유지한다.
- 픽셀 읽기·draw 반환·플랫폼 표시·모니터 표시 시각은 서로 다른 관찰이다.

SCRIPTED 빌드는 입력·CPU·GL 함수 대역 검사만 제공하며 실제 GL draw를 수행하지 않는다.
