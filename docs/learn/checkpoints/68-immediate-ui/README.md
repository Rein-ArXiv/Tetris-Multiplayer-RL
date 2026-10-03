# 68 · 즉시모드 UI: 그리기와 입력 판정

67-font-cache의 메뉴에 ‘시작’ 버튼을 연결하는 누적 프로젝트다. Space도 같은
confirm 경로로 들어간다. 버튼 판정은 논리 좌표의 `(14,64,64,28)`을 사용하고
그 사각형으로 둥근 배경과 캐시한 글자를 그린다.

## 책임과 계약

- `platform/pointer_edges.h`: 왼쪽 버튼의 held와 프레임 내 첫 press 위치.
  같은 펌프의 down/up도 보존한다. current position과 press origin을 구분한다.
- `platform/platform.h`·`sdl.cpp`: SDL 사건을 수집하고 최종 커서/포커스와 결합한다.
  다른 창·오른쪽 버튼은 제외, focus loss/leave/minimize/종료는 취소한다.
  좌표는 window 단위다. SCRIPTED/WIN32 학습 backend는 pointer unavailable이다.
- `client/immediate_ui.h`: optional layout을 통한 논리 변환, 반열린 사각형 판정,
  hovered/held/activated의 순수 계산. layout은 make_layout 결과를 그대로 사용한다.
- `src/main.cpp`: 전이 전 메뉴에서 클릭을 한 번 confirm으로 소비하고 현재 화면을 그린다.
  글리프 캐시·규칙·화면 상태의 소유권은 유지한다. UI가 직접 규칙 틱을 진행하지 않는다.

버튼은 press 위치에서 발동한다. release 확정·드래그 대상 capture·겹친 UI dispatch는
자동으로 제공하지 않는다. 두 번 판정하면 같은 결과가 나올 수 있으므로 호출자가
동작을 한 번만 실행한다. 현재 포인터가 안에 있고 down이면 held 표현을 사용하며,
누름이 그 버튼에서 시작했는지를 의미하지 않는다. hit 영역은 둥근 배경의 바깥 사각형이다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/68-immediate-ui -B out/lesson68 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/lesson68
ctest --test-dir out/lesson68 --output-on-failure
cd out/lesson68
./tetris
```

저장소 루트의 `python3 scripts/check_learning_immediate_ui.py`는 순수 UI·SDL 사건
라우팅·루트 hit-test와 mouse edge 회귀를 실행한다. pointer_sdl은 실제 SDL 사건 큐에
입력을 넣되 최종 커서/포커스를 통제하는 입력 전용 테스트 세션이다. GL 초기화나
실제 데스크톱 포커스 이동을 검증하는 도구는 아니다. Win32 분기는 소스 추출과
capture API 모형으로 검사하며 Windows 네이티브 빌드와 구별한다.
