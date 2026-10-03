# 14 — CPU 배열을 GL 버퍼 저장소로 복사하기

13-vertex-data의 입력·플랫폼·mesh.h를 유지한다. GL 테이블을9함수로 늘리고
renderer/vertex_buffer.h와 cpp를 추가한다. VertexBuffer는 하나의 GL 이름을 소유하며
원본 배열이나 컨텍스트를 소유하지 않는다. 테이블과 current 컨텍스트가 더 오래 살아 있어야 한다.

upload는 빈 소유자에서만 시작한다. 실패하면 얻은 이름을 정리하고, reset은 반복 호출할 수 있다.
GL_ARRAY_BUFFER 바인딩은 이 초기화 단계의 작업 상태로 남긴다. 호출 전 바인딩 복원은 제공하지 않는다.

```sh
cmake -S docs/learn/checkpoints/14-vbo -B out/study-14 -DSTUDY_PLATFORM=SDL
cmake --build out/study-14
ctest --test-dir out/study-14 --output-on-failure
./out/study-14/tetris
```

SDL2 개발 패키지와 pkg-config가 필요하다. Linux 단일 구성 생성기 경로 기준이다.
성공 로그: `GL vertex buffer stores 24 bytes (not drawn)`. 입력 루프 동안 버퍼를 유지하고
run_session이 반환될 때 정리한다. 그 다음 main이 플랫폼을 종료한다.

창 표시 없이 데이터 복사를 관찰하려면 Linux의 offscreen GL 지원 환경에서 실행한다.

```sh
SDL_VIDEODRIVER=offscreen ./out/study-14/buffer_probe
```

원본 배열 수정/소멸 후24바이트 일치, unbind 후 내용 유지, 바인딩0에서 업로드 오류,
같은 이름에16바이트 저장소 재정의, 반복 reset을 관찰한다. 이 진단은 표시용 창의
더블 버퍼 설정을 바꾸지 않는다. 매 프레임 readback을 넣는 용도로 사용하지 않는다.

SCRIPTED 빌드에서는 GL 호출 대역을 사용하는 소유권·로더 계약 검사와 CPU 데모만 실행한다.
전체 검사 명령은 `python3 scripts/check_learning_vbo.py`다.
