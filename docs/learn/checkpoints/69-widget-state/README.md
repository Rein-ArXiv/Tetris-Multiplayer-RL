# 69 · 버튼·선택기: 상태 소유 위치

68-immediate-ui에서 이어지는 누적 프로젝트다. 메뉴에 장식 표시 체크박스와
기존 두 이미지(기본/대체) 선택기를 추가한다. 선택은 메뉴·게임 전이와 재시작을
지나 유지하며, 프로그램을 종료하면 기본값으로 돌아간다.

## 책임과 프레임 순서

- `client/widgets.h`: 현재 값과 입력에서 표시용 결과·변경 요청을 순수 계산한다.
- `client/menu_model.h`: Preferences가 두 표현 값을 보관한다. Focus는 별도 값이다.
  evaluate는 위젯의 press와 키 입력을 중재해 Intent 하나를 반환한다.
  apply는 한 요청을 적용하고 실제 값 변경 여부를 반환한다.
- `renderer/menu_labels.h`: 일곱 라벨의 문단·GPU 텍스트를 준비한다.
  공유 atlas를 비우기 전에 reset한다. 실패한 준비는 ready로 공개하지 않는다.
- `renderer/menu_controls.h`: 이미 적용한 값을 그린다. 반전이나 화면 전이를 수행하지 않는다.
- `src/main.cpp`: 입력→evaluate→apply→Application 갱신→현재 값으로 그리기.
  Preferences는 run_session 수명, Focus는 메뉴 재진입 때 start로 초기화한다.

유효한 위젯 press가 키보다 우선한다. 선택기 중앙·비활성 화살표도 그 프레임을
소비하므로 동시에 눌린 Space가 시작 버튼으로 전달되지 않는다. 포인터 사용 불가는
키보드를 막지 않는다. 키보드 취소는 메뉴 프레임의 명령을 모두 취소한다.
Up/Down과 Left/Right의 양방향 동시 입력은 각각 상쇄한다. 인덱스는 끝에서 멈춘다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/69-widget-state -B out/lesson69 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/lesson69
ctest --test-dir out/lesson69 --output-on-failure
cd out/lesson69
./tetris
```

메뉴에서 Up/Down은 커서 이동, Space는 시작 또는 장식 반전, Left/Right는 아이콘 선택이다.
마우스로 시작·체크 행·화살표를 눌러도 된다. 게임 중 Space는 드롭, Escape는 메뉴 복귀다.
메뉴로 돌아오면 선택 값은 유지하고 키보드 커서만 시작으로 돌아온다.

저장소 루트의 `python3 scripts/check_learning_widgets.py`는 두 플랫폼 누적 빌드·CTest,
독립 키보드 상태표, 루트 위젯 경계 회귀, 렌더 자원 수명과 규칙 해시를 검사한다.
widget_render는 SDL offscreen의 단일 버퍼 시험 컨텍스트를 사용한다. 실제 앱의
이중 버퍼 창 초기화 요구를 바꾸지 않으며 수동 GUI 검사와 구분한다.
