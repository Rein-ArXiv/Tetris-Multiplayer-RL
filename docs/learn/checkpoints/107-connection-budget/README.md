# 107 — 연결·단계·선언 버퍼의 예산

106 누적 코드에 net/connection_budget.h와 두 실행 타깃을 추가한다.

```sh
cmake -S docs/learn/checkpoints/107-connection-budget -B out/study-107 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-107 --target connection_budget_contract connection_budget_probe -j2
ctest --test-dir out/study-107 -R '^connection_budget_(contract|probe)$' --output-on-failure
```

SDL은 별도 빌드 폴더에서 -DSTUDY_PLATFORM=SDL을 지정한다. 두 타깃은 창을 열지 않는다.
contract는0·정확한상한·SIZE_MAX 경계·이동·반복해제·마감관측·객체수명,
24스레드의 상한과12000연산 독립 행 모델을 검사한다.
probe는 실제 TCP의 두 참가자 뒤 추가 연결을 거절해 EOF를 관찰한다.
게스트 퇴장으로 자리를 반납한 뒤 같은 코드 재입장, 수락·입력 전달·RoundPlay 정규
상태 일치까지 진행하고 끝에 모든 세션·핸드셰이크·선언 버퍼·키 예약이0임을 확인한다.

Limits는 전체/출처별 세션·핸드셰이크와 전체/연결별 바이트의 여섯 점유 상한이다.
0은 허용하지 않음을 뜻한다. 생성 시 State와 고정 키 표를 할당하고 입장당 할당은 없다.
Lease는 이동 전용이며 소켓 소유자와 함께 이동한다. 핸드셰이크만 먼저 반납하고
세션과 바이트는 소유자가 정리할 때 반납한다. issued lease는 Budget보다 오래 살 수 있다.

바이트 회계는 활성 서버 연결의 FrameParser 타입 크기 한 개를 대상으로 한다.
임시 파서·이동 후 인라인 공간·게임·스레드·커널 버퍼를 포함한 RSS 계수는 아니다.
출처 키1은 loopback 실습 입력이며 IP 정규화/인증을 구현하지 않는다.
학습 프레임은 서비스 wire와 비호환이고 HDAAA·round1·seed77·역할은 fixture다.
개수 제한은 속도 제한이 아니며 시간 제한은 호출자의 FirstAdmission 관측/정리를 사용한다.
