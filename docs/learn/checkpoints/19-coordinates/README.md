# 19차시 — clip·NDC·viewport

18-triangle의 VBO·VAO·Program·draw 경로를 유지한다. 좌표 예측용 CPU 함수와
네 가지 정점 셰이더 실험을 더해 수치와 픽셀 결과를 비교한다.

## 파일과 책임

- renderer/coordinates.h: 서로 다른 공간의 타입, 점 포함·원근 나눗셈·viewport·UI 변환.
- renderer/projection_cases.h: base/w2/scaled/oversized의 GLSL과 CPU 예측 계수.
- src/coordinates_demo.cpp: viewport (10,20,200,100)에 대한 창 없는 수치 출력.
- src/main.cpp: 실행 인자로 셰이더를 선택하고 drawable 크기 변화 때 예상 위치 출력.
- tests/coordinates_contract.cpp: 수치·실패 입력·동차 배율·단위 검사. NDEBUG에서도 동작.
- tests/coordinates_real.cpp: 동일 VBO의 네 실험을 읽어 CPU 경계·내부/배경 픽셀과 비교.

## 실행

저장소 루트 기준, SDL2 개발 패키지·pkg-config·OpenGL 3.3 Core 사용:

```sh
cmake -S docs/learn/checkpoints/19-coordinates -B out/study-19 -DSTUDY_PLATFORM=SDL
cmake --build out/study-19
ctest --test-dir out/study-19 --output-on-failure
./out/study-19/coordinates_demo w2
./out/study-19/tetris base
./out/study-19/tetris w2
./out/study-19/tetris scaled
./out/study-19/tetris oversized
./out/study-19/coordinates_probe
```

창을 하나씩 실행하고 Escape로 닫는다. Linux에서 offscreen GL을 지원하면 probe 앞에
SDL_VIDEODRIVER=offscreen을 붙일 수 있다. probe에 기존 폴더 안의 파일 경로 접두사를
주면 네 PPM을 저장한다. 예: `coordinates_probe /tmp/coordinates`.
main의 더블 버퍼 요청은 유지하고 probe만 단일 버퍼로 픽셀을 읽는다.
Windows 다중 구성 생성기에서는 구성 이름과 실행 파일 경로를 맞춘다.

SCRIPTED 빌드도 CPU 도구와 8 CTest를 제공한다. 창과 GL probe는 SDL 빌드에서 만든다.

## 결과와 계약

- base: 창 위치 (60,45), (160,45), (110,95); 깊이 모두 0.5.
- w2: (85,57.5), (135,57.5), (110,82.5). 폭·높이가 절반.
- scaled: base와 같은 나눈 위치. 네 성분에 같은 양수 배율을 적용.
- oversized: (-40,-5), (260,-5), (110,145). 세 원래 점이 밖이어도 도형 일부는 남는다.
- 위 좌표는 CPU 도구의 viewport 기준이며 main의 실제 drawable 크기와 다를 수 있다.
- inside_clip_volume은 w>0 정책의 단일 점 술어다. 삼각형 클리핑/가시성 판정이 아니다.
- perspective_divide는 음수 w의 산술도 허용한다. 산술 성공은 clip 통과가 아니다.
- to_window는 연속 위치를 반환하고 기본 깊이 범위를 가정한다. 빈 크기는 거부한다.
  GL 최대 viewport 제한·깊이 범위 변경·클리핑을 재현하는 함수는 아니다.
- UI 좌표와 실제 drawable 픽셀은 다른 단위다. w=1이라 clip xyz와 NDC의 수치가 같다.
- 새 컨텍스트의 기본 framebuffer/테스트/쓰기/clip 상태를 유지하는 전용 실습이다.
