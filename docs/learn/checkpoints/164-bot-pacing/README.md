# 추론과 조작의 시간을 분리한다

Pacing/TickGate와 paced_driver를 추가한다. 같은 Session을 ONNX 정책이 관측하고,
Driver가 빈 입력까지 매 틱 실제 규칙에 적용한다. Session::pose는 읽기 전용 C++ 조회다.
기존 Python 관측/학습 규약과 Round 규칙은 유지한다.

```sh
cmake -S bindings -B build-native -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
./build-native/paced_demo
cmake -S inference -B build-inference -DCMAKE_BUILD_TYPE=Release -DORT_ROOT=/path/to/onnxruntime-sdk
cmake --build build-inference --config Release --target onnx_paced_probe
./build-inference/onnx_paced_probe /path/to/policy.onnx 91 180 3 2 8
```

예제 인자는 모델·seed·틱 예산·입력 간격·생각 시간·최소 소비 틱 순서다.
수치는 이 실행을 관찰할 예제 설정이며 게임 전체의 고정 난이도 계약이 아니다.
다중 구성 생성기와 Windows에서는 Release 폴더 및 .exe를 사용한다. SDK의 라이브러리
경로와 실행 시 DLL/SO/dylib 탐색은 inference 실습의 조건을 따른다.

실제 모델 대조는 tests/paced_inference.py에 --module-dir와 --probe를 지정한다.
짧은 TrainingRun→export→C++ 프레임 입력 기록과 Python 모델 선택·native 상태 bytes를
대조한다. 같은 입력 스케줄의 재현성을 검사하며 모든 모델의 난이도를 보증하지 않는다.

Driver는 한 Session의 모든 틱을 소유한다. 호출 간 외부 상태 변경을 발견하면 실행을
거절한다. 새 경기·외부 변경 뒤 계속하려면 reset으로 경계를 명시한다. 시뮬레이션은 빈
입력에도 진행한다. 생각 시간은 모델 안에서 기다리는 시간이 아니며 최소 시간은 자연
중력 잠금을 막지 않는다. 실패 반환과 연습/보상 여부는 상위 실행 정책에서 결정한다.
