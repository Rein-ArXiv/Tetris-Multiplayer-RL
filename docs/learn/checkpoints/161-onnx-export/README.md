# ONNX: 검사한 그래프를 배포한다

`160-model-zoo`의 누적 파일을 유지하고 ONNX 형식 검사·실행 비교·내보내기를 추가한다.
환경에는 pybind11, NumPy, CPU PyTorch, Gymnasium, ONNX, ONNX Runtime이 필요하다.
실습은 `dynamo=False`의 TorchScript exporter 경로를 명시한다.

```sh
cmake -S bindings -B build-native -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
python python/export_policy.py /path/to/training.pt /path/to/policy.onnx \
  --module-dir build-native/python/Release --seeds 41 93 --probe-pieces 3
```

위 seed/탐색 길이는 수치 대조를 위한 예제 설정이다. 이전 experiment/TrainingRun으로
저장한 CPU checkpoint를 사용한다. 현재 Python ABI와 일치하는 study_py의 폴더를
지정한다. 실제 native 관측을 수집하고 PyTorch/ONNX Runtime의 float32 출력과
합법 argmax를 대조한다. 종료 이후에는 더 진행하지 않는다.

출력 이름·순서·dtype·고정 shape, 그래프 구조, 포함된 weights와 실제 CPU 추론이
통과한 후보만 같은 디렉터리의 목적지로 원자적으로 교체한다. exporter/checker/
수치 비교/교체 이전 실패는 기존 목적지를 보존한다. 단일 작성자 계약이며
부모 디렉터리 fsync나 전원 장애 이후 내구성까지 보장하지 않는다.

ONNX는 이 추론 그래프와 가중치의 산출물이다. optimizer, rollout, RNG를 복구하는
훈련 checkpoint와 다르다. 검사한 관측 집합의 수치/행동 일치와 전체 게임 성능,
모든 입력·다른 하드웨어에서의 동일성은 구별한다. 이 실습 그래프는 누적 실습의
규칙/행동 의미를 따르며 현재 게임용 모델과 교환하지 않는다.
