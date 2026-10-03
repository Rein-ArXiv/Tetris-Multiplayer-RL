# 15차시 — VAO와 정점 속성

14-vbo에 정점 해석 규칙을 추가한다. `Vertex2`의 위치는 location 0, float 두 개,
stride 8바이트, offset 0이다. GL 버퍼는 데이터를, VAO는 읽는 설정과 버퍼 연결을 갖는다.

## 추가·교체 파일

- `renderer/vertex_array.h/cpp`: VAO 하나의 소유자. 입력 버퍼는 빌린다.
- `renderer/gl_api.h/cpp`: VAO 생성·바인딩·삭제, 속성 형식·활성화·조회8함수 추가.
- `src/main.cpp`: VBO 다음 VAO를 선언하여 VAO → VBO → 컨텍스트 순서로 정리한다.
- `tests/vao_contract.cpp`: 형식·호출 순서·부분 실패·재시도·정리 횟수.
- `tests/vao_real.cpp`: 두 VAO의 독립적인 상태와 ARRAY_BUFFER 캡처를 관찰한다.
- `tests/loader_contract.cpp`:17개 진입점의 개별 누락·전체 누락·재로딩을 확인한다.
- `CMakeLists.txt`:study_vertex_array·vao_contract·vao_probe 타깃 연결.

## 빌드와 실행

저장소 루트에서 Linux 단일 구성 생성기를 기준으로:

```sh
cmake -S docs/learn/checkpoints/15-vao -B out/study-15 -DSTUDY_PLATFORM=SDL
cmake --build out/study-15
ctest --test-dir out/study-15 --output-on-failure
./out/study-15/tetris
SDL_VIDEODRIVER=offscreen ./out/study-15/vao_probe
```

SDL2 개발 패키지와 pkg-config가 필요하다. `tetris`는 창과 더블 버퍼 GL 컨텍스트가
필요하다. `vao_probe`는 표시 없이 상태를 읽는 진단이라 단일 버퍼를 요청한다.
SDL의 offscreen GL을 사용할 수 없다면 그래픽 세션에서 `./out/study-15/vao_probe`를
실행한다. 셰이더와 draw는 아직 없으므로 도형이 표시되는 단계가 아니다.
Windows 다중 구성 생성기는 빌드·CTest 구성과 실행 파일의 Debug/Release 경로를 맞춘다.

SDL 없는 대역 검사:

```sh
cmake -S docs/learn/checkpoints/15-vao -B out/study-15-scripted -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-15-scripted
ctest --test-dir out/study-15-scripted --output-on-failure
```

SCRIPTED는 GL 컨텍스트를 만들지 않는다. CPU 데이터·함수 테이블·소유자 계약을 검사한다.
실제 상태 관찰은 별도 SDL 타깃으로 한다.

## 관찰할 것

1. location 0의 성분 수2·FLOAT·normalized0·stride8·offset0·enabled1.
2. ARRAY_BUFFER에 B나0을 연결해도 첫 VAO 속성의 버퍼는 A.
3. VAO 교체는 속성 상태를 선택하지만 일반 ARRAY_BUFFER 바인딩을 복원하지 않음.
4. y만 읽는 관찰 속성의 offset4·stride8. 다른 VAO에는 이 활성화가 전파되지 않음.
5. 잘못된 stride4도 API가 받아들일 수 있음. 오류 없음과 형식 일치는 다른 조건.
6. VAO0에서 VertexAttribPointer 형식 설정은 오류. VAO를 삭제해도 별도 소유한 VBO는 남음.

`configure`는 유효한 버퍼·로드된 함수 테이블·current GL3.3 Core 컨텍스트가 전제다.
비어 있는 소유자에서만 설정하며, 실패하면 VAO 이름을 정리한다. 입력 VBO는 삭제하지
않고 바인딩 상태를 이전 값으로 복원하지 않는다. 이 예제에서는 VAO보다 VBO를 먼저
만들고 나중에 없애 수명 계약을 단순하게 유지한다.
