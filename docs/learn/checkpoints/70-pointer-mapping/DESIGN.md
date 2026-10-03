# 70 · 공간 변환과 프레임 배치

## 추가 이유

640×480 배치의 원시 클릭(240,180)은 논리(120,90)의 장식 체크박스다. 같은 사건을
1200×617 배치에서 해석하면 약(19.85,70.02)로 시작 버튼이 된다. 변환 공식을
반복 구현하기보다 사건의 기준이 바뀐 프레임을 취소한다.

## 변경점

FrameMapping은 make_layout을 한 번 호출하고 같은 결과로 map_pointer를 계산한다.
이전 window/drawable/logical의 여섯 값을 모두 비교한다. 첫 호출도 changed로 취급하며
유효하지 않은 치수도 기록한다. 변경 시 Input만 비우고 cancelled를 세운다.
SDL 크기 사건은 pointer.cancel로 같은 펌프의 A→B→A도 취소한다. SDL Edges의
프레임 취소는 뒤의 새 press로 해제되지 않는다. keyboard.cancelled는 별도 상태다.
main은 mapping을 run_session에서 보관하고 각 프레임 layout/input을 const참조로 사용한다.
규칙·설정·텍스트캐시·viewport공식·선택기 코드는 유지한다.

## 비교와 한계

root는 정수 논리좌표와 최신 마우스사건 위치를 제공한다. 학습은 연속논리좌표와
최초press 위치를 보관한다. root Win32 버튼 사건에서도 위치를 갱신하도록 수정했다.
rootSDL은 window 기반 1:1 전제이며 별도 drawable 조회/크기 변경 취소는 학습의 계약이다.
FrameMapping은 OS 원자 조회나 이벤트별 배치 이력을 보장하지 않는다.

## 검증 기준

독립 viewport 사례표, 정수 곱셈 부등식 hit oracle, 내부점 왕복, 정확히 표현 가능한 경계,
resize/DPI/0 크기 복구, 같은 펌프의 크기 왕복과 키보드 독립을 검사한다.
GL ReadPixels에서 실제 축 정렬 사각형 표본과 역변환 hit를 비교한다. 경계 표본은 부동소수
판정과 래스터 경계 규칙을 분리해 집계하고 hit 영역에 epsilon을 추가하지 않는다.
루트 Win32 분기는 소스 추출과 capture API 모형이며 네이티브 Windows 검증이 아니다.
