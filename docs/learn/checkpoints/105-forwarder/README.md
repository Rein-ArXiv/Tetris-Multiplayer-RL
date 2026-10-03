# 105 — 방향별 전달과 연결 쌍 종료

104 누적 코드에 net/forward_direction.h와 두 실행 타깃을 추가한다.

```sh
cmake -S docs/learn/checkpoints/105-forwarder -B out/study-105 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-105 --target forward_direction_contract forwarder_probe -j2
ctest --test-dir out/study-105 -R '^(forward_direction_contract|forwarder_probe)$' --output-on-failure
```

SDL은 별도 빌드 폴더에 -DSTUDY_PLATFORM=SDL을 지정한다. 이 두 타깃은 창을 열지 않는다.
contract는6006개 프레임 크기/분할/송신 cap 조합, would_block/interrupted, 잘못된 송신 결과,
부분 수락 뒤 오류, prefix와 파서 손상, EOF의 부분 프레임/미전달 프레임 차이를 검사한다.
probe는 실제 방 조회·수락 인계 뒤 두 방향으로 각각 최대2바이트씩 송신하고 상대 클라이언트가
TYPE41 입력을 받게 한다. 호스트 연결 종료를 관측해 쌍을 닫고 실제 RoundPlay 상태를 비교한다.

FrameParser64바이트+EncodedFrame35바이트와 offset을 방향마다 소유한다.
아직 송신할 프레임이 있으면 소스 수신을 멈춘다. flush는 한 번의 송신만 시도한다.
순수 상태 객체에 타이머/OS 호출은 없으며, probe 호출자가2초 전체 실험 마감을 검사한다.
학습 프레이밍은 서비스 체크섬 형식과 호환되지 않는다. auth·서버 전용 타입 차단·보상 판정은 별도다.
HDAAA·round1·seed77·역할은 fixture 입력이며 실제 접속 협상 전체를 구현하지 않는다.
