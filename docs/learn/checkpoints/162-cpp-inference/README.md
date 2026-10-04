# C++ 추론과 텐서 수명

누적 실습에 bot/onnx_policy·policy_choice, C++ 실행기와 실제 Python 비교를 추가한다.
기존 bindings의 Python 확장과 학습 코드는 유지한다. 그래픽 창 없이 추론 경계를 실행한다.

```sh
cmake -S bindings -B build-native -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-native --config Release
cmake -S inference -B build-inference -DCMAKE_BUILD_TYPE=Release -DORT_ROOT=/path/to/onnxruntime-sdk
cmake --build build-inference --config Release
ctest --test-dir build-inference -C Release --output-on-failure
python tests/cpp_inference.py --module-dir build-native/python/Release --probe build-inference/onnx_probe --contract build-inference/inference_contract
```

ORT_ROOT에는 대상 OS/CPU SDK를 지정한다. 필요하면 ORT_LIBRARY로 링크 파일을 직접
지정한다. Windows의 다중 구성 생성기는 실행 파일이 Release 아래에 있으며 .exe를 쓴다.
실행 시 해당 SDK의 공유 라이브러리를 동적 로더가 찾을 수 있어야 한다. Windows에서는
같은 런타임의 DLL을 실행 파일 옆에 두거나 해당 폴더를 PATH에 추가한다.

검사 Python에는 NumPy·CPU PyTorch·Gymnasium·pybind11·ONNX·ONNX Runtime이 필요하다.
짧은 TrainingRun을 저장하고 ONNX로 내보낸 뒤 실제 C++ 실행기의 각 결정에 대해
PyTorch 출력·합법 argmax를 대조한다. 시드와 결정 예산은 이 검사의 예제 조건이다.
대상 Runtime에서 지원하는 IR/opset으로 내보내야 한다.

`onnx_probe /path/to/policy.onnx 91 6`은 이 실습 조건의 시드와 결정 예산으로 실행한다.
각 줄은 행동, 가치, 행동 점수 순서다. 게임 상태와 피스 어휘·행동 의미는 이 누적 실습의
규약을 따른다. 현재 게임용 ONNX 파일과 교환하지 않는다. 로더는 구조적 I/O를 검사하며
학습 출처 인증·사용자 제공 그래프 격리 기능을 제공하지 않는다.

입력은 호출자 소유 배열이고 Run은 동기 실행한다. 출력은 Prediction으로 복사해
Ort::Value 수명이 끝난 뒤에도 읽을 수 있다. 하나의 소유자가 로드/추론/파괴를 직렬화한다.
실패한 로드는 미로드 상태, 실패한 추론은 기존 Prediction 보존으로 처리한다.
