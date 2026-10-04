# 정책 네트워크 체크포인트

보드와 피스 관측을 받아 정책 logit·상태 가치를 출력하는 작은 모델을 만든다.
누적 규칙·바인딩·보상·환경·휴리스틱은 유지하고 Python 모델과 검사를 추가한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy gymnasium==1.3.0
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/156-policy-network/bindings -B out/study-156 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-156 --config Release -j2
ctest --test-dir out/study-156 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/156-policy-network/python/policy_demo.py \
  --module-dir out/study-156/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 실습의 경계

- PolicySchema는 native 관측/행동 스키마에서 축 크기를 읽는다. 피스 순서는 observation.encode의 카탈로그 순서다.
- PolicyNet은 잠긴 보드의 NCHW 한 채널과 두 피스 one-hot을 받는다. CPU 실습용 폭은 생성자 설정이다.
- 합성곱→flatten→피스 결합→공유 MLP→정책/가치 head를 만들고 출력 의미를 손실과 연결한다.
- model_contract는 eager 실행의 shape·dtype·device를 검사한다. 이진 값/one-hot 검사는 관측 생성 경계의 책임이다.
- 추적 그래프에는 Python 예외 검사가 들어가지 않는다. 이를 그대로 ONNX/배포 입력 검증으로 쓰지 않는다.
- 마스크는 정책 분포에 따로 적용한다. 모든 행동이 불법인 상태는 분포를 만들기 전에 종료 경계에서 처리한다.
- demo의 한 번 업데이트는 합성 표적을 사용한 역전파 연결 검사다. 학습된 게임 성능을 나타내지 않는다.
- eval()과 no_grad()의 역할을 나누고 backward() 이후에는 아직 파라미터가 바뀌지 않았음을 확인한다.
- 현재 TetrisPolicyNet과 축 구성 원리는 같지만 용량·피스 순서·정확한 규칙 전이는 다르다. 가중치를 바로 교환하지 않는다.
- checkpoint.py의 저장/복구 계약과 ONNX 런타임 배포는 여기서 구현하지 않는다.

```sh
python3 scripts/check_learning_policy_network.py --python /path/to/python
```
