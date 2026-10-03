# 86 필드 폭·엔디언·범위 검증

85-framing의 누적 게임과 통신 진단을 유지한다.
ByteReader/ByteWriter와 입력 배치 코덱을 추가하고 serialization_probe가
framing_probe 서버에 구조화된 요청을 보낸다.
본문은 [HTML 강의](../../index.html#lesson-86)에서 읽는다.

```sh
cmake -S docs/learn/checkpoints/86-serialization -B out/study86 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study86 -j3
ctest --test-dir out/study86 --output-on-failure
./out/study86/framing_probe listen 0 2
# 다른 터미널에서 LISTEN 포트 사용
./out/study86/serialization_probe 실제포트
```

기대 결과는 PAYLOAD 04 03 02 01 03 00 01 00 10과
VERIFIED first_tick=16909060 count=3 masks=01,00,10이다.
Windows 다중 구성 빌드는 --config Release, 검사 -C Release,
Release/ 아래 exe를 사용한다.

payload는 first_tick:u32LE + count:u16LE + masks:count다.
기준 count1..16·알려진 입력 비트·틱 래핑 없음·정확한 전체 길이를 요구한다.
TYPE1요청/TYPE2응답의 외부 프레임은 현재 게임 wire와 호환되지 않는다.
loopback 블로킹 한 연결의 직렬화 진단이며 게임 규칙 실행·인증·시간 제한은 없다.
`scripts/check_learning_serialization.py`로 기준 및 실제 Session 경로를 검사한다.
