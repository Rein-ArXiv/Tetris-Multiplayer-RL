# 75 MP3 디코딩

누적 게임에 CPU MP3 로더를 추가한다. 게임 main은 그대로다. 공통 로더는 압축16MiB/PCM64MiB 기본 예산, 학습 어댑터는 Pcm16에 맞춰 PCM16MiB 예산을 사용한다.

```sh
cmake -S docs/learn/checkpoints/75-mp3 -B out/study75 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study75 -j3
ctest --test-dir out/study75 --output-on-failure
./out/study75/mp3_probe docs/learn/checkpoints/75-mp3/assets/rotate.mp3
```

예상: `rate=48000 channels=2 frames=16128 samples=32256 bytes=64512 seconds=0.336`.
음원을 바꾸면 형식과 길이도 바뀐다. 장치 재생은 하지 않는다. SDL 누적 게임도 빌드하려면 별도 빌드 디렉터리에서 STUDY_PLATFORM=SDL을 선택한다.

`audio/mp3_decode.h/.cpp`: 파일 읽기·소유 결과·제한 있는 증분 디코딩.
`audio/load_clip.h`: 공통 결과를 Pcm16에 연결.
`tools/mp3_probe.cpp`: 파일별 메타데이터 출력.
`tests/mp3_contract.cpp`: 실제샘플 동등성/경계/잘린입력/파일오류.
`third_party/dr_mp3.h`: 고정한 의존성, 포함된 라이선스 원문 유지.
