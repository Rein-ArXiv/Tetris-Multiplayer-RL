# 74 PCM과 샘플

설정 저장이 있는 누적 게임에 CPU 오디오 데이터 모듈을 추가한다. 게임 main과 규칙·렌더링은 그대로이며 `pcm_probe`가 실제 1초 파형을 준비하고 검사한다. 장치 출력은 하지 않는다.

```sh
cmake -S docs/learn/checkpoints/74-pcm -B out/study74 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study74 -j3
ctest --test-dir out/study74 --output-on-failure
./out/study74/pcm_probe
```

SDL 게임도 함께 빌드하려면 다른 빌드 디렉터리에서 `-DSTUDY_PLATFORM=SDL`을 쓴다. 실행 파일 경로는 생성기/OS에 따라 Release 하위 폴더나 .exe 확장자가 붙을 수 있다.

```text
rate=44100 channels=2 frames=44100 samples=88200 bytes=176400 seconds=1
landmarks=0,6400,0,-6400,0
```

`audio/pcm_layout.h`: 샘플·프레임·바이트·기간 및 곱셈 전 상한 검사.
`audio/pcm_s16.h`: 포맷과 소유 배열, 완성 프레임 검증, 정수 삼각파.
`tools/pcm_probe.cpp`: 파형과 메타데이터를 관찰하는 콘솔 도구.
`tests/pcm_contract.cpp`: 단위·경계·채널 순서·소유권·파형 검사.

이 체크포인트의 코드가 강의의 안정적인 기준이다. 루트 구현은 학습 뷰어의 현재 소스에서 비교한다.
