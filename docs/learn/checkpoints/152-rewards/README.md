# 보상의 목표·시간·종료 조건

규칙과 바인딩은 행동 체크포인트 그대로 유지한다. python/rewards.py의 순수 함수가
삭제 줄·종료 항·잠재함수 차이를 분리하여 계산한다. reward_runner.py는 복사한 Session에
실제 행동을 적용하고 보상까지 성공한 후보와 구성 요소를 반환한다. 호출자가 후보를 반영한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/152-rewards/bindings -B out/study-152 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-152 --config Release -j2
ctest --test-dir out/study-152 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/152-rewards/python/reward_demo.py \
  --module-dir out/study-152/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 실습의 경계

- decision은 배치 결정마다 할인한다. tick은 실제 소비 틱 수만큼 할인한다.
- 줄과 종료 항은 마지막 입력 틱에 발생한다고 정의하고 tick 기준으로 가중한다.
- 잠재함수는 궤적 동안 고정한다. 실제 종료의 끝 잠재값은0, 수집 중단은 원래 값을 유지한다.
- 게임 점수와 학습 보상은 다르다. BP 지급과 계정 저장은 이 함수의 책임이 아니다.
- 현재 PPO의 반복 보드 벌점과 BCTS 기반 보조 보상은 이 잠재함수 구현으로 교체하지 않는다.
- tests/reward_contract.py는 텔레스코핑·가변 지속시간·종료 경계·수치 오류를 검사한다.
- tests/reward_binding.py는 실제 확장모듈·규칙 전이·원본 보존·입력 재생을 검사한다.

```sh
python3 scripts/check_learning_rewards.py --python /path/to/python
```

전체 모델 훈련·GPU·ONNX 없이 실행할 수 있다. 기존 C++ 코드는 변경하지 않는다.
README와 bindings/CMakeLists 외 행동 체크포인트 파일은 그대로 보존한다.
