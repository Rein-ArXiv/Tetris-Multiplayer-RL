# 102 — 등록과 취소를 함께 설계하는 매칭 큐

101의 누적 게임·입장·worker 구현을 보존하고 net/pair_queue.h를 추가한다.
고정 슬롯 큐는 성공한 등록 순서에서 취소된 항목을 제외한 앞의 두 자원을 인계한다.

## 실행

아래 명령은 저장소 루트에서 실행한다. 이번에 추가한 두 타깃을 검사한다.

```sh
cmake -S docs/learn/checkpoints/102-match-queue -B out/study-102 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-102 --target pair_queue_contract pair_queue_probe -j2
ctest --test-dir out/study-102 -R '^pair_queue_(contract|probe)$' --output-on-failure
```

SDL 구성을 사용할 때는 별도 빌드 디렉터리에 -DSTUDY_PLATFORM=SDL을 지정한다.
추가 타깃 자체는 창을 열지 않는다. TCP 실험은 loopback 소켓을 사용한다.

pair_queue_contract: 32768개 모델 비교 연산, 128회 취소/선택 경합, 단독 항목 재검사.
pair_queue_probe: 세 실제 TCP 연결 중 버퍼에 취소를 보낸 2를 제외하고 1·3 선택.
그 두 연결의 게임 입력으로 RoundPlay의 정규 상태 바이트가 같아지는지 비교한다.

실험의 round=1·seed=77·역할은 fixture가 공급한다. 실제 서버의 인증·MATCH_FOUND·READY
교환을 구현한 실험은 아니다. 입력은 선택 완료 뒤 보내 큐 단계의 소비 정책과 분리한다.
입장 실험은 보낸 바이트 수를 알고 cap3으로 모아 parser에 넘긴다. TCP가 한 프레임씩
돌려주거나 연속 송신을 항상 한 번에 합친다고 가정하지 않는다.
