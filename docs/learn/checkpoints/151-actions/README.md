# 행동 라벨을 실제 입력 틱으로 실행하기

누적 Round 규칙을 유지하고 action_space.h에 원점 열·방향 라벨을 정의한다.
action_plan.h는 복사한 Round에서 회전 → 이동 → 드롭을 실제 tick으로 진행한다.
Session은 성공한 계획만 적용하고 입력열·소모 틱 수·마지막 삭제 줄 수·종료 여부를 공개한다.
Python actions.py는 스키마 기반 인덱스·합법 마스크와 안전한 확률·엔트로피 계산을 제공한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/151-actions/bindings -B out/study-151 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-151 --config Release -j2
ctest --test-dir out/study-151 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/151-actions/python/action_demo.py \
  --module-dir out/study-151/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 실습의 경계

- 경로는 회전 → 수평 이동 → 드롭으로 제한된다. 전체 도달 가능한 위치의 탐색이 아니다.
- 준비 틱에서 회전/이동이 막히거나 피스가 잠기면 적용하지 않는다.
- 여러 논리 틱을 한 호출에서 실행하며 현실 시간의 지연을 만들지 않는다.
- 현재 SimGame::ApplyPlacement의 즉시 끝점 배치와 학습용 경로의 의미는 다르다.
- 마스크가 비었을 때는 분포를 만들지 않는다. 종료와 경로 제한 등의 원인을 호출자가 구분한다.
- 엔트로피는 불법 log 확률을 곱셈 전에 가려 기울기까지 유한하게 유지한다.

```sh
python3 scripts/check_learning_actions.py --python /path/to/python
```

검수기의 C++ ASan/UBSan은 Linux 도구다. Python/PyTorch 전체를 계측하지 않는다.
전체 모델 학습·GPU·ONNX 추론은 필요하지 않다. 관측 체크포인트의 README 및
session.h/study_py.cpp/bindings CMake 외 누적 파일은 그대로 보존한다.
