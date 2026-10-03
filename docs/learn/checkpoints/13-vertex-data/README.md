# 13 — CPU 정점 데이터와 바이트 범위

12-gl-loader의 플랫폼·GL 로더·입력 데모를 유지하고 renderer/mesh.h를 추가한다.
layout_demo는 창 없이 정점 수·stride·offset·바이트 수와 vector 용량을 관찰한다.
SDL tetris의 run_session도 같은 make_triangle()을 사용한다. GPU 업로드/그리기는 아직 없다.

```sh
cmake -S docs/learn/checkpoints/13-vertex-data -B out/study-13 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-13
ctest --test-dir out/study-13 --output-on-failure
./out/study-13/layout_demo
```

SCRIPTED를 선택하면 SDL 개발 패키지 없이 CPU 실습을 빌드한다. SDL을 선택하면
누적 tetris도 빌드한다. Linux 단일 구성 생성기 실행 경로 기준이며 다중 구성 환경은
--config Debug 및 Debug 하위 실행 경로를 사용한다.

정점3개·stride8·24바이트·x/y오프셋0/4는 이 실습의 명시적 레이아웃 계약이다.
C++가 모든 환경에서 float를4바이트로 보장한다는 뜻이 아니다. 맞지 않으면 static_assert가
빌드를 거부한다. reserve는 원소를 만들지 않고 clear는 size를0으로 만들며 capacity를 유지한다.

검사: python3 scripts/check_learning_mesh.py. CPU 레이아웃 검사와 SDL 빌드를 구분한다.
실제 GUI·GPU 업로드·Windows/macOS 실행은 이 차시에서 검증하지 않는다.
