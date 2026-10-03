# 17차시 — 프로그램 링크와 stage 인터페이스

16-shader에 Program 소유자를 추가한다. 컴파일된 두 셰이더를 빌려 연결하고,
링크 상태·진단·정리를 확인한다. main은 프로그램을 남기고 임시 셰이더를 정리한다.
실제 그리기는 다음 단계에서 추가한다.

## 파일과 책임

- renderer/program.h/cpp: 프로그램 단독 소유. 성공 후 detach, 실패/예외 후 자기 이름 정리.
- renderer/shader_sources.h: vertex의 out vec3 v_color를 fragment의 in vec3로 연결.
- renderer/gl_api.h/cpp: 프로그램8함수를 추가한31진입점 테이블.
- src/main.cpp: 짧은 Shader 범위와 긴 Program 범위, 컨텍스트 이전 자원 정리.
- tests/program_contract.cpp: 생성0·API 경계·링크 실패·로그·예외·재시도·차용 소유권.
- tests/program_real.cpp: 개별 컴파일 성공/링크 실패, 선택 보존, 셰이더 정리 후 사용.
- tests/loader_contract.cpp: 비트 이동 대신 누락 인덱스를 사용해 함수 수 증가에 대응.

## 실행

저장소 루트, SDL2 개발 패키지·pkg-config와 GL3.3 Core 필요. Linux 단일 구성 경로:

```sh
cmake -S docs/learn/checkpoints/17-program -B out/study-17 -DSTUDY_PLATFORM=SDL
cmake --build out/study-17
ctest --test-dir out/study-17 --output-on-failure
./out/study-17/tetris
SDL_VIDEODRIVER=offscreen ./out/study-17/program_probe
```

main은 더블 버퍼 창을 사용하며 링크 성공 로그까지만 보여 준다. probe는 표시 없이
링크를 관찰하기 위해 단일 버퍼를 요청한다. SDL offscreen GL이 없는 환경에서는
그래픽 세션에서 환경변수 없이 probe를 실행한다. Windows 다중 구성 생성기에서는
빌드/CTest 구성과 Debug/Release 실행 경로를 맞춘다.

SDL 없이 대역/CPU 계약을 검사하려면 STUDY_PLATFORM=SCRIPTED로 별도 빌드한다.
이 경로에서는 실제 GL 컨텍스트를 만들지 않는다.

## 중요한 구별

- COMPILE_STATUS·LINK_STATUS·API 오류·정보 로그는 별개다.
- AttachShader는 차용 관계이며 C++ 객체 복사나 소유권 이전이 아니다.
- 성공한 링크 결과는 셰이더 detach/delete 이후에도 남는다.
- 새 프로그램 링크는 현재 프로그램을 선택하는 호출이 아니다.
- Program은 사용을 끝낸 뒤 정리하는 정책이다. GL 자체는 현재 프로그램 삭제를
  허용하지만 실제 해제를 미룬다. reset은 다른 프로그램의 선택을 바꾸지 않는다.
- 실제 렌더러의 link_program은 소스부터 받아 컴파일하고, 성공 시 셰이더를 붙인 채
  삭제 요청한다. 학습 코드와 소유 범위가 다르므로 이름보다 책임을 비교한다.
