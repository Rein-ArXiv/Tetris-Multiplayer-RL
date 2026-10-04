# 특징·한 배치 탐색·재현 가능한 기준 정책

heuristic_features.py는 크기를 입력에서 읽는 순수 이진 보드 특징을 제공한다.
heuristic.py는 불변 계수·유한 점수·생존 우선 옵션·명시적 동점 기준으로 후보를 선택한다.
PlanningVersusEnv는 복사한 규칙 모델에 접근하는 상대를 별도 경계로 연결한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy gymnasium==1.3.0
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/155-heuristic/bindings -B out/study-155 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-155 --config Release -j2
ctest --test-dir out/study-155 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/155-heuristic/python/heuristic_demo.py \
  --module-dir out/study-155/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 실습의 경계

- 평가 함수는 정책이 아니다. 합법 후보를 복사해 실행한 뒤 평가와 동점 규칙으로 선택한다.
- 특징은 실제 보드의 높이·구멍·표면 요철/우물이며, 삭제 줄은 전이 사건이다.
- Weights는 이 체크포인트의 실험 설정이다. 출처·최적성·성능 보증을 붙이지 않는다.
- prefer_survival은 생존 여부를 점수보다 먼저 비교하는 정책 선택이다.
- 정확한 동점은 작은 라벨로 정한다. 부동소수점의 플랫폼 간 근접값 차이는 별도 문제다.
- choose는 현재 Session을 보존하며 후보마다 실제 apply_action을 실행한다.
- PlanningVersusEnv의 상대는 규칙 복사본에 접근한다. 일반 VersusEnv는 관측과 마스크만 전달한다.
- 새 _opponent_action 훅은 선택 방식만 분리하고 환경의 반환 검증·공동 반영은 유지한다.
- 데모는 고정된 실험 시드·결정 예산에서 줄/공격/보상/후보 수와 종료 종류를 출력한다.
- 짧은 데모의 평균을 장기 성능이나 승률 보장으로 해석하지 않는다.
- tests/heuristic_contract.py는 작은 특징 사례·수치 거절·원본 보존·동점·실제 혼합 종료 후보·재현을 검사한다.

```sh
python3 scripts/check_learning_heuristic.py --python /path/to/python
```

README·bindings/CMakeLists·python/versus_env 외 대전 체크포인트 소스를 보존한다.
C++ 규칙과 바인딩은 변경하지 않는다. GPU·전체 모델 훈련 없이 실행한다.
