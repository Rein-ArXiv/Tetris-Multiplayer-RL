# 70 · 마우스 역매핑: 그림과 클릭을 일치

69-widget-state의 메뉴·설정·규칙을 유지하고 프레임 배치의 수명을 보강한다.
window·drawable·logical 크기를 한 번의 FrameMapping.update에 전달한다.
결과 layout은 그림에, 같은 layout으로 계산한 Input은 메뉴 판정에 사용한다.

## 계약

- `client/frame_mapping.h`: 직전 여섯 크기를 보관한다. 첫 호출·크기 변경·복구 때
  포인터를 취소하되 유효한 새 layout은 그리기에 제공한다. 키보드는 별도 경로다.
- `platform/sdl.cpp`: 이 창의 SIZE_CHANGED/RESIZED 사건도 포인터 취소로 처리한다.
  같은 펌프에서 크기가 바뀌었다 돌아오는 경우는 끝 크기 비교만으로 알 수 없다.
- `src/main.cpp`: FrameMapping은 프레임 반복문 밖에 둔다. 프레임당 한 번 update하고
  반환한 layout과 input을 함께 사용한다. 설정 값이나 규칙 상태를 초기화하지 않는다.
- `renderer/letterbox.h`: 확정된 정수 viewport를 두 축의 변환에 사용한다.
  창·픽셀·논리 단위와 GL 원점 변환을 구분한다. 역변환 계산 자체는 유지한다.

이 계약은 OS 호출 전체를 원자적으로 읽거나 각 사건 당시 배치를 복원하지 않는다.
프레임 관찰 값과 사건에서 발견한 변경을 취소하는 정책이다. 크기 변경 프레임의
클릭이 버려질 수 있으며, 안정된 배치에서 새로 눌러야 한다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/70-pointer-mapping -B out/lesson70 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/lesson70
ctest --test-dir out/lesson70 --output-on-failure
cd out/lesson70
./tetris
```

메뉴를 키보드·마우스로 조작하고 창을 가로/세로로 바꾼다. 그림과 클릭이 같은
배치로 움직이고, 크기 변경 뒤 새 클릭을 받아야 한다. 첫 프레임과 복구 프레임은
포인터를 취소하지만 키보드 메뉴 조작은 허용한다.

저장소 루트에서 `python3 scripts/check_learning_pointer_mapping.py`로 누적 빌드,
독립 정수 부등식/배치표·SDL 사건 취소·실제 픽셀 비교·현재 루트 포인터 회귀를 실행한다.
픽셀 시험은 offscreen 단일 버퍼 컨텍스트와 통제한 크기 쌍을 사용한다. 실제
모니터 DPI 전환·네이티브 Windows 창·수동 GUI 검증과 구분한다.
