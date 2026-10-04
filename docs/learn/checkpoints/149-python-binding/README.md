# Python에서 같은 규칙 호출하기

`bindings/session.h`는 누적 Round를 값으로 소유하고 입력 검증·한 틱 진행·reset·clone을 제공한다.
`bindings/study_py.cpp`가 그 계약을 Python에 노출한다. 규칙·난수를 Python으로 재구현하지 않는다.
`grid()`는 잠긴 셀의 중첩 list 복사이며 현재 게임의 NumPy 관측 API와 구별한다.

## 빌드

C++17, CMake 3.18 이상, 선택한 Python의 개발 헤더와 pybind11이 필요하다.
그 Python에서 `python -m pip install pybind11==3.0.1`을 실행하고
`python -m pybind11 --cmakedir`로 경로를 찾는다. 이 버전은 실습 의존성 선택이다.
아래 명령의 두 경로를 실제 절대 경로로 바꾼다.

```sh
cmake -S docs/learn/checkpoints/149-python-binding/bindings -B out/study-149 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-149 --config Release -j2
ctest --test-dir out/study-149 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/149-python-binding/bindings/demo.py \
  --module-dir out/study-149/python/Release
```

Windows에서는 셸의 줄바꿈 문법과 Python 경로를 바꾼다. 확장 파일은 해당 OS/CPU/Python용으로
빌드한다. CMake가 모듈 접미사를 정하며 단일/다중 구성 모두 python/Release 아래로 모은다.
원하는 Python을 바꿀 때는 별도 빌드 디렉터리를 사용한다.

단독 bindings 구성에는 그래픽·서버 의존성이 없다. 전체 누적 체크포인트에서는
`STUDY_BUILD_PY=ON`으로 같은 하위 디렉터리를 포함한다. 전체 구성의 SDK는 기존대로 필요하다.

## 확인

실습의 고정 시드·입력열은 예제 조건이다. `tests/binding_contract.py`는 직접 Round를 호출하는
`binding_oracle`과 전이마다 비교하며 복사/clone/reset/실패 보존도 검사한다. 이 대조가 공유한
규칙 전체의 정확성을 증명하지는 않는다. 독립적인 규칙 회귀도 계속 유지한다.

```sh
python3 scripts/check_learning_python_binding.py --python /path/to/python
```

위 저장소 검수기는 Linux에서 C++ 어댑터 ASan/UBSan도 실행한다. 확장 모듈/CPython 전체를
계측하는 것은 아니다. 이 체크포인트는148의 README/CMake 외 누적 파일을 그대로 보존한다.
