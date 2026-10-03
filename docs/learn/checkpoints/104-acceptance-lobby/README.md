# 104 — 수락 로비와 파서 인계

103 누적 코드에 net/acceptance_lobby.h와 두 검사/실습 타깃을 추가한다.
창을 여는 main은 그대로 유지하며, 두 소켓의 수락은 별도 TCP 실행 파일에서 조립한다.

```sh
cmake -S docs/learn/checkpoints/104-acceptance-lobby -B out/study-104 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-104 --target acceptance_lobby_contract acceptance_lobby_probe -j2
ctest --test-dir out/study-104 -R '^acceptance_lobby_(contract|probe)$' --output-on-failure
```

SDL 구성은 별도 빌드 폴더와 -DSTUDY_PLATFORM=SDL을 사용한다. 두 타깃은 창을 열지 않는다.
contract는 READY 값256개·메시지 길이·모든 분할 위치/양쪽 순서·절대 마감·EOF·버퍼 상한·
이전 단계 prefix·한 번의 인계와 이동 후 원본 비우기를 검사한다.
probe는 방 코드 조회로 얻은 두 TCP 연결에 READY를 추가하고, 게임 바이트의 꼬리를 인계해
실제 RoundPlay의 양쪽 정규 상태 바이트를 비교한다. 클라이언트의 READY 알림 수신도 확인한다.

학습 프레이밍은 길이+타입+본문이며 서비스 체크섬 프레이밍과 호환되지 않는다.
코드 HDAAA, round1, seed77, 역할은 fixture가 제공한다. MATCH_FOUND/auth는 구현하지 않는다.
기준 로비는 준비 전 unknown 타입을 즉시 거절하지만 현재 서비스 로비는 보존하고 기다릴 수 있다.
수락한 쪽의 후속 바이트는 다음 단계의 데이터다. 로비는 수락 취소를 다시 파싱하지 않는다.

상태 기계는 통지할 action을 반환한다. 호출자가 송신 실패를 감지하면 close하고 자원을 정리한다.
실습의 작은 loopback 송신은 진행 실패를 검사 실패로 처리한다. 제품 코드의 비동기 출력 큐는 아니다.
