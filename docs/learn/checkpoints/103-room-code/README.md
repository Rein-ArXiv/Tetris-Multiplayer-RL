# 103 — 방 코드·충돌·소유권

102 누적 코드에 net/room_directory.h와 두 독립 타깃을 추가한다.
고정 슬롯의 방 등록소, 5자리 코드의 인코딩, 제한된 충돌 재시도와 세대별 제거를 만든다.

```sh
cmake -S docs/learn/checkpoints/103-room-code -B out/study-103 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-103 --target room_directory_contract room_directory_probe -j2
ctest --test-dir out/study-103 -R '^room_directory_(contract|probe)$' --output-on-failure
```

SDL 구성에서는 별도 빌드 디렉터리에 -DSTUDY_PLATFORM=SDL을 지정한다.
두 타깃은 창을 열지 않으며 TCP probe는 loopback 연결을 쓴다.

contract는16384 모델 연산·64 동시 충돌·재사용 세대·상한·난수원 실패·수명 계약을 검사한다.
probe는 host 입장→코드 HDAAA 발급/송신→guest가 같은 코드를 전송→조회/인계→실제 게임 입력을 연결한다.

코드 후보103은 재현용 fixture다. 서비스용 난수원이 아니다. Type52는 이 실습의 코드 응답이다.
round1/seed77/역할도 fixture 공급이며 실제 인증·READY 로비 전체를 구현하지 않는다.
현재 서비스는 별도 server/room_code.cpp의 OS 난수원을 사용한다.
