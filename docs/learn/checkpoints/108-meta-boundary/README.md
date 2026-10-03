# 108 — 전달과 영속성의 경계

107 누적 코드에 고정된 경기 요청과 제출 상태를 추가한다.

```sh
cmake -S docs/learn/checkpoints/108-meta-boundary -B out/study-108 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-108 --target match_submission_contract match_submission_probe -j2
ctest --test-dir out/study-108 -R "^match_submission_(contract|probe)$" --output-on-failure
```

두 새 타깃은 창을 열지 않는다. SDL 구성은 별도 빌드 폴더에 만든다.
contract는 잘못된 준비·복사 소유·영수증 연결·시도 소진·확정 후 무호출을 검사한다.
probe는 실제 TCP 두 연결의 입력을 서버와 두 클라이언트의 누적 RoundPlay로 실행한다.
서버 쪽 종료 상태에서 경기 값을 만들고, 첫 반영 뒤 미확인·같은 요청 재시도를 관찰한다.
입장/방/예산 흐름은 보존된 connection_budget_probe에서 별도로 실행할 수 있다.

key17·round1·seed77·player101/202는 fixture다. 학습 wire는 서비스 wire와 다르다.
ReceiptService는 단일 행 메모리 대역으로 DB·계정 인증·보상 지급·재시작 복구를 구현하지 않는다.
실제 post_match는 내부 최대3회 HTTP 시도가 있어 학습 submit의 한 콜백과 시도 단위가 다르다.
