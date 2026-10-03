# 82 오디오 실패와 게임 진행의 분리

81-xaudio2에서 누적한다. SDL/XAudio2의 장치 코드는 유지하고 Session의 정책을 분리한다.
BasicSession<Device, Supported>는 unprepared/disabled/ready/unavailable 상태를 가지며
prepare를 객체 수명에 한 번만 시도한다. 새 초기화 시도에는 새 Session이 필요하다.

```sh
cmake -S docs/learn/checkpoints/82-audio-failure -B out/study82 -DSTUDY_PLATFORM=SDL -DSTUDY_AUDIO=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study82 -j3
ctest --test-dir out/study82 --output-on-failure
cd docs/learn/checkpoints/82-audio-failure
../../../../out/study82/tetris
```

장치 실패 실습은 실행할 때 SDL_AUDIODRIVER=study-driver-does-not-exist를 설정한다.
새 out/study82-none 빌드를 STUDY_AUDIO=NONE으로 구성하면 의도적 무음 상태를 비교할 수 있다.
NONE은 초기화 실패 통지를 만들지 않는다. 실제 장치 제거 자동 복구는 구현하지 않았다.

send의 결과 started/skipped/failed/invalid_kind를 구별한다. play(bool)는 기존 도구와의
호출 호환용이며, main은 Batch를 drain해 각 사건을 한 번 시도하고 남기지 않는다.
Failure 종류별 통지는 한 번만 꺼낼 수 있다. 통지를 읽어도 seen 플래그는 지워지지 않는다.
이 통지는 사건의 시간순 로그가 아니며 enum 순서로 반환된다.

CueFactory 함수 포인터는 기본 합성 음원 준비를 교체하는 경계다. 검사에서는 각 자리에서
nullopt/bad_alloc/그 밖의 예외를 주입해 부분 초기화 정리를 확인한다.
테스트 대역 Device는 실제 소리의 증거가 아니다. 실제 규칙에 같은 dt/입력을 넣어
오디오 정책에 따른 결과 해시와 시간 잔여량이 같은지 비교한다.
