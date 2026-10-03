# UTF-8: 바이트와 코드포인트

62-rounded-corners의 게임·이미지·패널을 유지하고 화면 이름을 해석하는 CPU 경로를 추가한다.

- text/utf8.h: string_view의 첫 스칼라·소비 바이트·상태. 빈 입력0, 정상1~4, 오류1바이트.
- client/labels.h: 최대16스칼라의 표현 데이터. 용량초과 nullopt, 잘못된바이트는 U+FFFD와 개수로 기록.
- main: 메뉴/플레이 이름을 한번 해석하고 화면 전환 때 코드포인트를 콘솔에 출력한다.
- utf8_demo: ASCII/한글/보충평면, 잘린 접두부 뒤 ASCII, 길이 안의 NUL을 관찰한다.

```bash
cmake -S docs/learn/checkpoints/63-utf8 -B out/study63 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study63 -j3
ctest --test-dir out/study63 --output-on-failure
out/study63/utf8_demo
out/study63/tetris --seed 1 --image out/study63/assets/player.png
python3 scripts/check_learning_utf8.py
```

UTF-8은8비트바이트/최대4바이트, 최소길이·surrogate 제외·10FFFF상한을 검사한다.
unsigned char로 바이트를 해석한다. 읽을 수 있는 view 수명/크기는 호출자가 보장한다.
복구는 바이트별1소비 정책이다. U+FFFD정상문자와 invalid대체는 상태로 구별한다.
네트워크 식별자·정규화·grapheme·글리프 shaping·텍스트폭을 처리하는 API는 아니다.

대기실/플레이는 각각9바이트·3스칼라다. 현재 화면의 한글 글리프 출력은 아직 없다.
이번 산출물은 글꼴에 넘길 스칼라 자료이며 콘솔의 U+표기로 터미널 글꼴과 독립적으로 확인한다.
GPU 형식/그리기 횟수·규칙 상태/해시는 그대로다. C++17 u8리터럴과 MSVC /utf-8 설정을 사용한다.

전체1112064스칼라 roundtrip/잘린입력,256단일/65536바이트쌍,200000결정적바이트열,
별도 RFC바이트범위 기준,보호페이지 바로앞 입력,루트 어댑터 수정전후를 검사한다.
수정 전 text_gl.cpp 스냅샷은 out/learning-checkpoints/63-utf8-check에 보존한다.
