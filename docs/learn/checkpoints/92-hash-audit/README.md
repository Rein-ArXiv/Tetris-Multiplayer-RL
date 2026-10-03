# 92 해시 메시지와 비교 창

91-input-delay에 초기·주기 해시 교환과 같은 검사점의 역할별 비교를 추가한다.
[HTML 강의](../../index.html#lesson-92)에서 상태 번호와 선도착을 읽는다.

```sh
cmake -S docs/learn/checkpoints/92-hash-audit -B out/study92 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study92 -j3
ctest --test-dir out/study92 --output-on-failure
./out/study92/hash_probe listen 0 77 2
# 다른 터미널에서 LISTEN 포트 사용
./out/study92/hash_probe connect 포트번호
```

TYPE21의 payload는 완료 틱·host 해시·peer 해시를 각각 LE u64로 담은 24바이트다.
초기 0과 4의 배수만 교환한다. D2에서는 30틱을 소비하고 0~28의 주기 해시를 비교하며
입력 꼬리 2개가 남는다. 마지막 상태 29~30은 wire에서 비교하지 않는다.
D30은 2틱만 소비해 초기 0만 비교한다. DONE 해시는 별도의 출력 진단이다.

두 출처에 각각 8개 샘플을 보관한다. 누락은 대기, 보관 중인 동일 중복은 허용,
다른 값의 충돌은 거절한다. poll 성공에는 불일치를 발견한 결과도 포함된다.
프로그램은 불일치·문법 오류·누락된 샘플이 있는 EOF에서 실패하며 상태를 자동 복구하지 않는다.

Windows 다중 구성은 --config Release, CTest -C Release와 Release/의 exe를 사용한다.
수신은 블로킹이며 전체 시간 제한이 없다. 고정된 작은 실험으로, 지속 GUI 경기와는 구분한다.
`python3 scripts/check_learning_hash_audit.py`로 코덱·창·통신·현재 main 회귀를 검사한다.
