# 16차시 — GLSL 소스와 셰이더 컴파일

15-vao에 두 GLSL330 소스와 Shader 소유자를 추가한다. 셰이더 각각의 컴파일까지
진행하며 프로그램 링크·사용·그리기는 다음 단계에서 연결한다.

## 파일 연결

- `renderer/shader_sources.h`: location0의 vec2 위치와 고정 색상 fragment 소스.
- `renderer/shader.h/cpp`: 단독 소유·명시적 source 길이·상태와 로그·실패 정리.
- `renderer/gl_api.h/cpp`: 셰이더6함수 추가. 로더는 총23개 진입점을 요구한다.
- `src/main.cpp`: vertex/fragment를 각각 컴파일. 진단을 출력하고 예외 뒤에도 플랫폼 종료.
- `tests/shader_contract.cpp`: 길이·로그·실패·정리·예외 전파의 함수 대역 검사.
- `tests/shader_real.cpp`: 원본 소스 변경/소멸 후 컴파일, 잘못된 문법, 실패 후 재시도.
- `tests/loader_contract.cpp`, `CMakeLists.txt`: 늘어난 진입점과 타깃 연결.

## 실행

저장소 루트, Linux 단일 구성 생성기 기준. SDL2 개발 패키지와 pkg-config 필요.

```sh
cmake -S docs/learn/checkpoints/16-shader -B out/study-16 -DSTUDY_PLATFORM=SDL
cmake --build out/study-16
ctest --test-dir out/study-16 --output-on-failure
./out/study-16/tetris
SDL_VIDEODRIVER=offscreen ./out/study-16/shader_probe
```

`tetris`는 더블 버퍼 창을 만들고 두 stage의 컴파일 로그를 보여 준다. shader_probe는
표시 없이 컴파일만 관찰하므로 단일 버퍼를 요청한다. SDL offscreen GL을 사용할 수
없는 환경에서는 그래픽 세션에서 환경변수 없이 shader_probe를 실행한다.
Windows 다중 구성 생성기에서는 빌드·CTest의 구성과 Debug/Release 실행 경로를 맞춘다.

SDL 없는 계약 검사는 STUDY_PLATFORM=SCRIPTED로 별도 빌드한다. 이 경우 실제 GL
컨텍스트는 만들지 않으며 입력·mesh·로더·각 소유자의 대역 검사만 실행한다.

## 확인할 계약

- ShaderSource가 반환되면 원본 바이트는 바꿔도 된다. string_view는 원본을 소유하지 않는다.
- 명시적인 GLint 길이를 넘기며 문자열 개수와 바이트 수를 구분한다.
- API 오류, COMPILE_STATUS, 정보 로그는 서로 다른 진단이다.
- 컴파일 실패는 객체 이름을 정리하고 진단을 남긴다. 성공 로그가 있어도 성공일 수 있다.
- string/vector 메모리 할당은 예외를 던질 수 있다. compile은 noexcept가 아니며 이름을
  정리한 뒤 예외를 다시 전달한다. 호출자의 세션 경계는 예외 후 플랫폼을 정리한다.
- reset은 이름만 지우며 진단을 보존한다. 새 compile 시도에서 진단을 갱신한다.
  이미 이름을 소유한 경우 새 compile을 거부하며 기존 결과를 유지한다.
