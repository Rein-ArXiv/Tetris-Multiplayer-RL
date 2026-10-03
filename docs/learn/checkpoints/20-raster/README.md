# 20차시 — 샘플·보간·프래그먼트

19-coordinates의 정점/버퍼/VAO/draw 경로를 유지하고, CPU 삼각형 샘플러와
smooth/flat/cutout GLSL 실험을 추가한다. CPU 도구는 예측을 위한 독립 도구이며
main의 GL 래스터화를 대체하지 않는다.

## 연결할 파일

- renderer/raster.h: Point, edge, 가중치·영역 판정, 픽셀 중심, RGB 가중합.
- renderer/raster_sources.h: 세 실험의 셰이더 소스와 선택 표.
- src/raster_demo.cpp: 16×16 샘플 격자·가중치 출력.
- src/main.cpp: 선택한 vertex/fragment를 링크하고 기존 draw 경로로 표시.
- tests/raster_contract.cpp: 수치·방향·평행 이동·퇴화·경계·실패 입력 검사.
- tests/raster_real.cpp: 실제 128×128 framebuffer와 CPU 예측 색을 비교.
- CMakeLists.txt: CPU demo/contract와 SDL probe 추가.

## 실행

저장소 루트, SDL2 개발 패키지·pkg-config·OpenGL 3.3 Core 사용:

```sh
cmake -S docs/learn/checkpoints/20-raster -B out/study-20 -DSTUDY_PLATFORM=SDL
cmake --build out/study-20
ctest --test-dir out/study-20 --output-on-failure
./out/study-20/raster_demo
./out/study-20/tetris smooth
./out/study-20/tetris flat
./out/study-20/tetris cutout
./out/study-20/raster_probe
```

창은 하나씩 실행하고 Escape로 종료한다. Linux offscreen GL이 가능하면 probe 앞에
SDL_VIDEODRIVER=offscreen을 붙일 수 있다. 선택 인자로 기존 폴더의 경로 접두사를
주면 접두사-smooth.ppm/flat.ppm/cutout.ppm을 저장한다. 예: `/tmp/raster`.
Windows 다중 구성 생성기는 구성 이름과 실행 경로를 맞춘다.
SCRIPTED 빌드는 CPU demo와 9 CTest를 제공하며 실제 GL 실행 파일은 만들지 않는다.
coordinates_demo와 기존 좌표 실험 probe/검사는 그대로 보존한다.

## 계약과 관찰

- CPU sample_triangle은 GL과 동일한 래스터라이저가 아니다. boundary는 경계 진단이며
  공유 변의 소유권을 선택하지 않는다. 내부/바깥 전에 유한성·좌표 절댓값≤1,000,000 검사.
- 계산된 면적 0은 nullopt. 임의의 거의 퇴화한 실수 기하를 정확히 판정한다고 보장하지 않는다.
- pixel_center는 기본 단일 샘플 위치 x+.5/y+.5를 계산한다. MSAA는 구현하지 않는다.
- RGB 가중합은 이번 동일한 clip w=1의 smooth 보간과 대응한다. 일반 원근 보간은 아니다.
- main은 DrawArrays(TRIANGLES,0,3)으로만 사용한다. 셰이더 색 배열은 gl_VertexID0..2 전제.
- flat은 새 컨텍스트의 기본 LAST_VERTEX_CONVENTION에서 마지막 C의 파랑을 쓴다.
- cutout은 8×8 창 픽셀 격자의 짝수 칸 조각을 버린다. 알파 합성과 다른 동작이다.
- 새 컨텍스트의 기본 채우기/쓰기 상태, 단일 샘플, 깊이·스텐실·블렌드·면 제거 비활성 전제.
- probe는 single buffer를 요청하고 실제 버퍼 속성에 맞춰 읽는다. main은 double buffer 유지.
- CPU16×16 삼각형 내부32개. GL128×128 삼각형 내부2048개, cutout은1024개를 버린다.
  이 선택된 도형에는 샘플 중심의 정확한 경계 접촉이 없어 모든 픽셀의 색을 비교한다.
