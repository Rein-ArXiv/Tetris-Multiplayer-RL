# 68 설계 결정

즉시모드 UI의 매 프레임 기술과 입력 판정, 현재 위치/눌림 사건 위치, 반열린 hit test,
앱의 상태 소유와 전이 시 입력 소비를 누적 메뉴의 시작 버튼으로 연결한다.
새 UI 키보드 확인은 기존 Space 유지. SDL left pointer 입력은 프레임 내 최초 누름
위치와 현재 위치를 분리하고 같은 프레임 down/up도 보존한다. focus loss/leave 취소.
SCRIPTED/WIN32의 pointer는 기존 mouse와 같이 unavailable (학습 input-only backend).
메뉴에서만 클릭 의도를 app.advance의 confirm으로 넘기며 게임 입력으로 재소비하지 않는다.
루트 gui_hover_rect의 int 덧셈 overflow와 Part17 즉시모드 비용/상태 단정을 교정한다.
코드·마우스 어댑터·자동 검사·본문을 검토해 reviewed로 등록했다.

## 추가로 교정한 현재 코드

- gui_hover_rect: 양수 크기 검사·int64 덧셈으로 int overflow 거절/회피.
- root SDL/Win32 마우스: snapshot down 차이 대신 KeyEdges<3>로 down/up 사건 누적.
  같은 프레임 짧은 클릭의 pressed/released를 보존. init/shutdown에서 reset.
- Win32: 마지막 held가 풀린 경우에만 capture 반환. 정상 ReleaseCapture의
  WM_CAPTURECHANGED는 완료된 엣지를 유지하고 강제로 held 캡처를 잃으면 취소.
- root 마우스 좌표 API는 최종 좌표를 쓰며, 학습 Edges의 최초 press origin과 다르다.
  강의의 현재 소스 비교에서 이 계약 차이를 정확하게 설명한다.
- Part2/3/5의 현재 발췌와 상태·비용·반열린 경계 설명 교정.

## 원고에서 다룰 것

현재 값으로 매 프레임 UI 기술; immediate UI와 GL immediate mode 구별;
메뉴 상태는 Application 소유, 입력 이력은 platform 소유; 논리 hit rectangle;
좌표 변환 단계와 letterbox의 unavailable; current hover와 press 사건 좌표;
유한 상태 기계·edge vs level·프레임 내 첫 press로 압축하는 정책;
순수 판정 결과를 application command로 바꾸고 화면 전이 프레임에 한 번 소비;
부작용(그리기·앱상태전이)의 경계; 그리기 순서와 입력 우선순위가 별도임;
눌렀을 때 발동하는 한 버튼, drag capture/release-confirm/겹친위젯 dispatch는 별도계약.

## 초안 편집 기록

DeepSeek 068-outline-draft.json은 구성/문제 후보이며 원고가 아니다. 16개의 용어별 절을
그대로 나열하지 말고 메뉴 버튼을 만들어 가는 흐름으로 묶는다. 같은 프레임 down/up과
최초 press 정책은 하나의 상태 표로 설명하고 반복을 줄인다. 질문은 모두a정답이고
answer에 정답 문자열 대신a가 들어갔다. 형식/정답 위치를 고치고 w=0 문항처럼
구현 한 줄 재진술에 치우친 문제를 상태 소유·중복 dispatch 시나리오로 교체한다.
이 후보를 직접 편집해 068.json의 14절·5문제로 완성했다. 구현 한 줄 재진술을 상태 소유·중복 dispatch 문제로 교체하고 answer에 선택지의 전체 문자열을 넣었다.
