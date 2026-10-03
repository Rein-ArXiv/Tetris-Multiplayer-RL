# 69 · 상태 소유와 입력 중재

Preferences(장식 표시,기존2개 아이콘 선택)는 run_session에서 보관하여 메뉴/게임 전이와
분리한다. 메뉴focus는 별도의 Focus값이며 메뉴 재진입 때 start로 초기화한다.
위젯은 의도만 반환, evaluate는 focus와Action 하나를 반환, apply는 변경 여부를 반환한다.
마우스활성화가 키보드보다 우선; 동시에Up/Down 또는Left/Right는 상쇄; focus 이동 후
Space/좌우 해석. pointer unavailable/cancel은 mouse만억제하며keyboard는동작. Keys.cancelled(키보드포커스상실)는전체메뉴명령취소.
Space=start면confirm,장식이면toggle,선택기면아무설정변경없음.
경계는clamp,한항목/0항목/잘못된index는움직임없음. mouse/keyboard가함께들어와도한명령.
렌더링은apply후최신값을그리며추가명령을소비하지않음. 게임규칙과 seed/hash는불변.
일시적인focus와지속선택값·GPU이미지핸들·시뮬레이션상태의수명을구분한다.


## 기준 코드와 현재 소스의 차이

현재 src/gui.cpp는 그리기와 마우스 판정을 함께 호출한다. gui_checkbox 반환 후
호출자가 값을 반전하므로 체크 모양은 다음 프레임에 새 값을 반영한다. 학습은
순수 판정과 그리기를 분리해 갱신된 값을 같은 프레임에 그린다.
루트 선택기는 방향의 사용 가능 여부를 호출자가 전달하며 학습은 index/count에서
계산한다. 양쪽 모두 값 자체의 적용 책임은 호출자에게 남긴다.
루트 마우스는 최종 좌표, 학습은 첫 press 원점을 사용한다. 이는 입력 수집 계약의
차이이며 상태 소유 분리와 별개의 정책이다.

## 보존한 경계

Preferences는 저장 파일·규칙·GPU 핸들을 소유하지 않는다. 게임 규칙과 초기 해시,
키 입력의 화면 전이 가드, GL·텍스트 캐시·플랫폼 구현은 누적 계약을 유지한다.
위젯 수나 고정 좌표를 바꾸어도 입력 판정과 그리기에 같은 bounds를 사용한다.
반전은 비멱등이므로 하나의 Action을 한 번만 적용한다. apply의 반환은 실제 값의
변경 여부이고, start의 화면 전이는 Application에 별도로 전달한다.
