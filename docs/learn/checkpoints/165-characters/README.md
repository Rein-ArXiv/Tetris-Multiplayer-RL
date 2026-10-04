# 캐릭터의 표시와 행동을 분리한다

누적 Session·정책·틱 실행기는 유지한다. bot/characters.h는 명시 캐릭터 목록을
읽어 id·Appearance·Behavior로 나누고, 선택 시 값 복사로 경기 설정을 고정한다.
bot/opponent_profile.h는 게임의 명시 프로필 파서와 같은 기준 코드다.
자동 모델 탐색과 legacy 덮어쓰기는 이 실습에 포함하지 않는다.

```sh
cmake -S bindings -B build-native -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
cmake -S inference -B build-inference -DCMAKE_BUILD_TYPE=Release -DORT_ROOT=/path/to/onnxruntime-sdk
cmake --build build-inference --config Release --target character_probe
./build-inference/character_probe characters.cfg calm 91 180
```

예제 characters.cfg에는 이 Session용으로 export한 모델 경로를 넣는다. 상대 경로는
프로세스 실행 폴더 기준이다. 예제 숫자는 시간표를 비교할 조건이다.

```text
calm|Calm|model/policy.onnx|assets/calm.png|assets/calm-portrait.png|Patient|3|2|8
swift|Swift|model/policy.onnx|assets/swift.png|assets/swift-portrait.png|Quick|1|0|1
```

character_probe는 실제 ONNX 정책·Driver·Session을 연결하고 표시 정보와 틱 기록을
출력한다. 이미지 경로는 표시 계층에 넘길 데이터로 확인하며 이 콘솔 도구에서 이미지를
로드하지 않는다. 누적 GUI의 이미지 로더/비율 유지 그리기에 전달할 필드는 Appearance다.
이 도구는 @heuristic 프로필 실행 요청을 명시적으로 거절한다. 모델 실패를 다른 정책으로
바꾸는 결정과 보상 서비스는 별도 책임이다.

Windows/다중 구성 생성기는 Release 경로와 .exe를 사용한다. ONNX SDK의 대상 OS와
프로세스 공유 라이브러리 경로 조건은 inference 실습을 따른다. Python 실제 모델 검사는
 tests/character_inference.py --module-dir 빌드경로/python/Release --probe 실행파일로 실행한다.

설정 파싱은 파일 존재·모델 의미·출처 인증을 검사하지 않는다. 로컬 배포 도구에서
리소스 허용 경로와 누락을 검사하고, 정책 로더에서 입력/출력 규약을 확인한다.
