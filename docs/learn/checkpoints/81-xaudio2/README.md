# 81 XAudio2: 같은 계약의 Windows 경로

80-voice-pool의 게임·사건 투영을 유지하며 XAudioPlayer를 추가한다.
Session이 의존하는 제어 계약은 open/replace/play/stop/unload/close다.
SDL의 state/cursor_frames 진단 API나 최종 출력 바이트의 동일성을 약속하지 않는다.

## 빌드 선택

- STUDY_PLATFORM=SDL/SCRIPTED/WIN32: 창과 입력 구현.
- STUDY_AUDIO=AUTO/SDL/XAUDIO2/NONE: 소리 구현.
- AUTO: SCRIPTED이면 NONE, 나머지는 Windows면 XAUDIO2, SDL이면 SDL.
- XAUDIO2는 Windows 타깃·Windows SDK 필요. Windows 10/XAudio2 2.9를 대상으로 한다.
- SDL 창+XAUDIO2가 누적 GL 게임 경로다. WIN32 플랫폼은 초기 input_demo용이다.
- SDL2 CMake 패키지를 우선 사용하고 없으면 pkg-config를 사용한다.

Linux에서 SDL 누적 게임과 계약 검사:

```sh
cmake -S docs/learn/checkpoints/81-xaudio2 -B out/study81 -DSTUDY_PLATFORM=SDL -DSTUDY_AUDIO=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study81 -j3
ctest --test-dir out/study81 --output-on-failure
cd docs/learn/checkpoints/81-xaudio2
../../../../out/study81/tetris
```

Windows 개발 셸에서 Visual Studio 생성기를 사용하는 예시:

```sh
cmake -S docs/learn/checkpoints/81-xaudio2 -B out/study81-win -DSTUDY_PLATFORM=SDL -DSTUDY_AUDIO=XAUDIO2 -DCMAKE_PREFIX_PATH=C:/deps/SDL2
cmake --build out/study81-win --config Release
ctest --test-dir out/study81-win -C Release --output-on-failure
out/study81-win/Release/xaudio_probe.exe
cd docs/learn/checkpoints/81-xaudio2
../../../../out/study81-win/Release/tetris.exe
```

SDL2 설치 경로는 자신의 환경으로 바꾼다. 단일 구성 생성기는
-DCMAKE_BUILD_TYPE=Release를 사용하고 실행 파일 경로의 Release/를 뺀다.
실제 xaudio_probe는 장치를 요구하므로 기본 CTest에 넣지 않았다.
xaudio_contract/xaudio_session_contract는 OS 호출 경계를 대역으로 바꾼 검사이며
실제 Windows SDK ABI·장치 출력·지연 검사는 별개다.
