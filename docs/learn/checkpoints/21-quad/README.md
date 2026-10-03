# 21차시 기준 코드 — 사각형과 공유 경계

`20-raster`에 정점 여섯 개의 사각형, 범위 검사, CPU 경계 소유권과 면 제거 실험을 추가한 누적 체크포인트다. HTML 강의의 핵심 스니펫과 함께 읽는다.

## 빌드와 실행

저장소 루트에서 SDL2 개발 패키지·pkg-config·OpenGL 3.3 Core 환경을 사용한다.
단일 구성 생성기 기준이다. 다중 구성 생성기는 구성 이름과 실행 경로를 맞춘다.

```sh
cmake -S docs/learn/checkpoints/21-quad -B out/study-21 -DSTUDY_PLATFORM=SDL
cmake --build out/study-21
ctest --test-dir out/study-21 --output-on-failure
./out/study-21/quad_demo
./out/study-21/tetris quad
./out/study-21/tetris reverse
./out/study-21/tetris cull
./out/study-21/tetris reverse-cull
./out/study-21/quad_probe
```

창은 하나씩 실행하고 Escape로 닫는다. `quad_probe`에 기존 폴더 안의 파일 접두사를
인자로 주면 네 결과를 PPM으로 저장한다. Linux에서 지원하는 경우
`SDL_VIDEODRIVER=offscreen`으로 probe를 실행할 수 있다.
별도 빌드 디렉터리에서 `STUDY_PLATFORM=SCRIPTED`를 선택하면 CPU 도구와 계약 검사를 사용한다.

## 바뀐 책임

- `quad.h`: ABC/ACD 정점 여섯 개, 방향 반전, CPU top-left 경계 소유권.
- `vertex_buffer.*`: 포인터/개수 업로드와 배열 오버로드, 성공한 정점 수 보관.
- `triangle.*`: available/first/count 검사 후 clear와 draw. 기존 세 정점 래퍼 유지.
- `gl_api.*`: Enable/Disable/FrontFace/CullFace 네 함수 추가, 총41개 필수 엔트리.
- `quad_sources.h`: 여섯 정점에 맞는 진단 색. 세 원소 색 배열을 재사용하지 않는다.
- `main.cpp`: 두 삼각형을 한 호출로 그리고 방향 반전·면 제거 조합을 선택한다.
- `quad_demo.cpp`, `quad_contract.cpp`, `quad_real.cpp`: CPU 정책·실제 GL의 개별 마스크·네 조합 확인.
- 버퍼·그리기·로더의 기존 계약 검사에 개수/범위/새 함수 누락 검사를 추가한다.

## 계약과 적용 범위

- 위치와 stride는 정점당8바이트, 전체 업로드48바이트, draw count6이다.
- raw 포인터는 호출자가 count개를 읽을 수 있도록 보장한다. 메타데이터만으로 메모리를 검증하지 않는다.
- available은 VAO가 참조하는 버퍼의 업로드 개수여야 한다. draw가 VAO 내부를 조회하지 않는다.
- 각 draw 호출은 clear한다. 여러 도형 누적 제출에는 프레임 준비/제출의 분리가 필요하다.
- CPU 경계는 y-up CCW 기준 top-left다. 시계 방향은 값 복사본에서 정규화한다.
- nullopt는 잘못된 입력/퇴화 계산, 값이 있는 false는 비소유다.
- CPU 규칙이 GL의 특정 공유 변 소유 방향이라고 가정하지 않는다. GL 검사는 두 개별 마스크의 합을 비교한다.
- 부동소수점 exact-zero 정책과 수치 범위는 raster.h를 따른다. 하드웨어 subpixel/MSAA는 재현하지 않는다.
- 앞면 분류와 면 제거 활성화는 별개다. 기본/반전/정상 면 제거는 사각형, 반전+면 제거는 둘째 삼각형만 남긴다.
- 새 컨텍스트의 단일 샘플·기본 채움·전체 색 쓰기·블렌드/깊이/스텐실 비활성 전제다.
- main은 더블 버퍼, probe는 읽을 버퍼를 실제 속성에 맞춰 선택하는 단일 버퍼 요청을 사용한다.
- 기존 CPU 도구와 probe는 각각의 세 정점 계약을 유지한다.
