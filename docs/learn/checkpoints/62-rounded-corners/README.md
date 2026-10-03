# 둥근 모서리: 거리 함수와 경계

61-image-transform의 이미지 소유·UV/tint/회전·게임 규칙을 유지하고 RoundedQuad를 추가한다.

- rounded_distance.h: 순수 double 거리/알파 마스크 실습. 경계는 부호로 구별한다.
- rounded_geometry.h: image Draw + 논리 radius, 52바이트 정점(기존32 + local2/half2/radius).
- rounded_quad.h: smooth local과 flat half/radius, SDF와 논리 폭1 smoothstep, straight 알파 합성.
- image_store.h: RoundedQuad 오버로드. 기존 ImageQuad는 진단/비교용으로 유지한다.
- main: 한 번 만든 1×1 흰 텍스처로 패널, 같은 아이콘으로 둥근 사각형·캡슐·원을 그린다.

```bash
cmake -S docs/learn/checkpoints/62-rounded-corners -B out/study62 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study62 -j3
ctest --test-dir out/study62 --output-on-failure
out/study62/rounded_demo
out/study62/tetris --seed 1 --image out/study62/assets/player.png
python3 scripts/check_learning_rounded.py
```

radius는 유한한 0~min(width,height)/2, 이미지 크기는 양수이며 각 축 최대1e6 논리 단위다.
수치 범위 정책이며 창 크기나 물리 해상도 제한이 아니다. 잘못된 요청은 GL 호출 전에 거절한다.
local은 회전 전 사각형 중심 기준이고 UV의 crop/flip과 독립적이다. position은 회전된 NDC다.
반지름0은 마스크를 생략한다. 작은 양수 반지름도 그대로 계산하며 root의 1미만 직각 근사와 다르다.

정점6개312바이트를 매 요청 갱신한다. 색상 Stream.finish 뒤 패널1+이미지3draw를 제출해
메뉴5/game6draw가 된다. GL 상태 경계는 ImageQuad와 같다(독점 패스, arbitrary-state 복구 아님).
SDF 마스크는 생성된 조각 안에서만 작동한다. 논리 폭1은 물리1픽셀/정확한 면적 coverage가 아니다.

CPU 거리236196사례를 선분/원호 경계 기준과 비교하고, GL형식/실패 주입을 검사한다.
24 GPU사례는 독립 역변환과 구간별 거리/알파 계산을 사용한다. 기하·텍셀 경계 근처는
제외 개수를 기록한다. root 비유한 roundness는 수정 전/후 실제 함수 추출로 확인한다.
