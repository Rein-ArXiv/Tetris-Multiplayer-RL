# 두 보드의 공동 전이와 상대 정책 경계

RoundEnv의 스키마·관측·시드·수명 계약을 이어받아 VersusEnv를 만든다.
Session에 누적 공격 조회와 수신 큐 API를 연결하고 A 배치→B 응답을 한 결정으로 묶는다.
상대는 B 관측과 합법 마스크의 복사본을 받으며, 실제 두 보드는 성공한 공동 전이만 반영한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy gymnasium==1.3.0
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/154-versus/bindings -B out/study-154 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-154 --config Release -j2
ctest --test-dir out/study-154 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/154-versus/python/versus_demo.py \
  --module-dir out/study-154/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 실습의 경계

- 한 step은 A 배치와 B 응답의 순서가 있는 결정이다. 실시간 동시 틱과 다르다.
- A 공격은 B의 이번 잠금, B 공격은 살아 있는 A의 다음 잠금에서 주입된다.
- A가 종료되어도 이번 결정에서 살아 있는 B가 응답한 뒤 최종 승패를 판정한다.
- 이미 끝난 수신 보드에는 새 공격을 큐에 넣지 않는다.
- 막힌 A 행동은 두 보드가 그대로인 한 결정이며 상대 호출 없이 예산만 소비한다.
- 상대의 None은 합법 행동이 없을 때만 허용한다. 타입·범위·현재 합법성을 검사한다.
- 보드·보상·출력을 후보에서 만든 뒤 함께 반영한다. 콜백 오류는 reset을 요구한다.
- 보드 보존과 상대의 외부 상태 롤백은 다르다. 임의 Python 코드를 격리하는 구조가 아니다.
- 두 논리 시계의 ticks는 따로 기록한다. 대전 보상은 공동 결정 기준이며 tick 할인을 제공하지 않는다.
- 보상은 줄·공격·승패 항의 합이다. 단일 보드의 RewardSpec 잠재함수는 이 환경에 적용하지 않는다.
- 기본 랜덤 상대는 환경이 에피소드마다 reseed한다. 관측 schema는 A 보드만 유지한다.
- tests/versus_contract.py는 실제 생성 공격의 수신/잠금·두 상태·오류 보존·시드 재현을 검사한다.
- tests/versus_session.cpp는 추가한 C++ 조회·큐·복사·종료 거절 경계를 검사한다.

```sh
python3 scripts/check_learning_versus.py --python /path/to/python
```

누적 규칙은 보존한다. README·bindings/session.h·study_py.cpp·CMakeLists 외
Gym 체크포인트 파일은 그대로 유지한다. 독립 CTest에는 누적 CPU PyTorch 관측 검사가 포함되며
GPU와 모델 훈련 없이 실행한다.
