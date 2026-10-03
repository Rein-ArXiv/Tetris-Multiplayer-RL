# 같은 이미지로 UV·tint·회전 표현하기

60-image-handles의 소유/핸들·PNG 디코더·Texture와 게임 규칙을 유지한다.
ImageQuad가 32바이트 position2/UV2/tint4 정점6개를 만들고 사각형을 그린다.

- image_geometry.h: Draw(rect/uv/tint/pivot/angle) 검증, double 회전 계산, NDC 변환.
- image_quad.h: 셰이더 sampled*tint, 기존 VBO에192바이트 갱신, straight alpha source-over.
- image_store.h: ImageQuad용 draw 오버로드. 이전 고정 Quad는 기존 진단 도구용으로 유지.
- main: 화면별 핸들로 기울어진 전체 아이콘, 위쪽 절반+색상곱, 반투명 회전 아이콘을 표시.

```bash
cmake -S docs/learn/checkpoints/61-image-transform -B out/study61 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study61 -j3
ctest --test-dir out/study61 --output-on-failure
out/study61/transform_demo
out/study61/tetris --seed 1 --image out/study61/assets/player.png
python3 scripts/check_learning_transform.py
```

Draw는 회전 전 좌상단과 양수 크기, [0,1] UV/tint/pivot, 유한 각도를 받는다.
UV 끝점 반전/일치는 허용. 각도는 논리 y-down에서 시계방향 양수이며 pivot은 사각형 안의 비율.
큰 유한 각도는 먼저360으로 축약, 결과NDC가float에 표현되지 않으면 거절한다.
화면 밖 위치는 클리핑 대상이며 아주 작은 도형이 float 정밀도로 사라지지 않는다는 보장은 없다.

ImageQuad는 독점 이미지 패스다. viewport/scissor는 호출자가 정하고 depth/stencil/cull/
dither/framebuffer-sRGB는 끈다. 유닛0에 sampler객체가 없고 쓰기 마스크는 기본값이다.
정상 종료 시 VAO/VBO/program/유닛0 텍스처 바인딩을 비우고 blend를 끈다.
임의 호출자의 모든 상태를 복원하는 API가 아니며 GL오류 후에는 실패를 처리해야 한다.

색상 배치를 finish한 뒤 이미지를 그린다. 이미지마다192바이트 정점 갱신/1draw이며
이 단계는 다중 이미지 배처가 아니다. main의 전체 메뉴4draw/게임5draw, 기존 opaque stream
통계는 색상 부분만 센다. PNG 파일/소유 Texture는 매 프레임 다시 만들지 않는다.

CPU 54회전·pivot 사례/범위/UV, GL실패 주입, 24픽셀 장면의 독립역변환·샘플·알파 비교,
루트의 큰 유한 각도/NaN/Inf 처리와 규칙 골든을 검사한다. 비스듬한 도형 경계와 텍셀 경계의
작은 비교 제외 영역은 로그에 개수로 표시하며 전체 래스터 경계 일치로 주장하지 않는다.
