# 78 믹싱·게인·클리핑

77-callback에서 누적한다. Mixer가 기존 Voice를 최대9개 빌려 int32에 누산하고 최종한번int16로 제한한다.
Player 공개API는단일소유음원그대로며내부에서Mixer0번슬롯을사용한다. 게임main/규칙은유지한다.

```sh
cmake -S docs/learn/checkpoints/78-mixing -B out/study78 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study78 -j3
ctest --test-dir out/study78 --output-on-failure
./out/study78/mix_probe
```

예상 `sum=30000 mixed=30000 legacy=2767`. CPU 믹싱도구는SCRIPTED에서도빌드한다.
장치재생도구/콜백검사는SDL선택에서추가된다. CTest는SDLdummy를사용한다.
공통 audio/mix_s16.h는root와동일:비유한게인0,유한값[0,1],보이스별0방향절삭,int32누산후최종포화.
Mixer는자원을소유하지않는다. 음원은연결된동안생존/불변이어야하며호출자가동시접근을조정한다.
