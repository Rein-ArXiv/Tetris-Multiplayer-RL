# 선택한 상태를 관측 텐서로

누적 Session에 값 snapshot과 카탈로그 schema를 추가한다. 규칙은 그대로 유지한다.
`python/observation.py`는 이진 그리드·종류 ID를 CHW/one-hot 배열로 변환한다.
`stack_batch`는 표본 축을 만들고 `to_tensors`는 실제 CPU PyTorch 텐서로 연결한다.
현재 SimGame의 색 ID 그리드 및 ID-minus-one 입력 규약과 실습 스키마를 구별한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/150-observation/bindings -B out/study-150 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-150 --config Release -j2
ctest --test-dir out/study-150 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/150-observation/python/observation_demo.py \
  --module-dir out/study-150/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 실습의 경계

관측은 잠긴 보드·현재 종류·첫 예고만 담는다. active 위치·중력·나머지 예고·가비지·RNG를
복구하는 형식이 아니며, 같은 관측에 다른 전체 상태가 대응할 수 있다. 대기 가비지처럼
전이에 영향을 주는 정보가 누락될 수 있음을 현재 코드의 관측 회귀에서도 확인한다.

- 비대칭 작은 보드/불연속 ID fixture는 축·범주 대응을 검증한다.
- 실제 Session으로 생성·고정·종료를 거치며 snapshot이 게임을 바꾸지 않는지 확인한다.
- float32 변환 전 원래 값 검사로 반올림에 의한 잘못된 값 통과를 막는다.
- NumPy/PyTorch 저장소 공유는 의도적이다. C++ 상태와 입력 관측/배치는 별도로 복사된다.

```sh
python3 scripts/check_learning_observation.py --python /path/to/python
```

위 저장소 검수기의 C++ ASan/UBSan은 Linux 도구다. Python/PyTorch 전체를 계측하지 않는다.
관측 생성은 학습·GPU·ONNX 추론을 요구하지 않는다. 149의 README 및 바인딩 통합에 필요한
session.h/study_py.cpp/bindings CMake 외 누적 파일은 그대로 보존한다.
