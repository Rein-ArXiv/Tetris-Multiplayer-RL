# Gymnasium: 초기화·전이·에피소드 경계

규칙·바인딩·관측·보상 코드는 보상 체크포인트에서 이어받는다.
python/gym_env.py의 RoundEnv가 spaces·reset·step·close를 제공한다.
python/gym_contract.py는 정수 입력 경계를 검증하고, 후보 Session을 성공한 전이 뒤 반영한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy gymnasium==1.3.0
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/153-gym-environment/bindings -B out/study-153 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-153 --config Release -j2
ctest --test-dir out/study-153 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/153-gym-environment/python/gym_demo.py \
  --module-dir out/study-153/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 실습의 경계

- reset(seed=...)은 환경 RNG와 네이티브 시작 시드를 재설정한다. reset()은 난수열을 이어간다.
- 생성자 시드는 첫 reset에만 적용한다. action_space RNG는 독립적이다.
- 행동 공간은 스키마의 라벨 도메인, legal_mask는 그 순간의 실행 가능성이다.
- 잘못된 타입·범위는 ValueError, 도메인 안의 막힌 배치는 보드를 유지하는 한 결정이다.
- decision 기준 무효 요청에도 gamma가 적용된다. tick 기준에서는 소비 틱 0·할인 1이다.
- max_steps는 게임 밖의 결정 예산이다. 실제 종료와 예산 경계가 겹치면 두 플래그가 모두 참이다.
- reset 전·에피소드 경계 뒤·close 뒤 step은 ResetNeeded로 거절한다.
- 외부 TimeLimit을 쓰면 호출자가 가장 바깥 step의 종료 플래그를 따른다.
- 출력 배열은 관측 복사본이다. info의 진단 상태는 정책 관측에 자동으로 추가하지 않는다.
- 현재 SimGame 환경의 즉시 배치·0 보상 무효 행동과 학습용 틱 실행·잠재함수 shaping은 구별한다.
- tests/gym_contract.py는 실제 모듈로 공식 검사·시드열·타입·종료·실패 보존·출력 소유를 검사한다.

```sh
python3 scripts/check_learning_gym_environment.py --python /path/to/python
```

독립 구성의 누적 CTest에는 CPU PyTorch 관측 검사도 포함한다. 모델 훈련이나 GPU는 필요 없다.
README와 bindings/CMakeLists 외 보상 체크포인트 파일은 그대로 보존한다.
