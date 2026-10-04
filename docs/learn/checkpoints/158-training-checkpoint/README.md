# 학습 체크포인트: 파일 교체와 실행 재개

PPO 업데이트 사이에 모델·Adam·진행·난수·환경을 저장하고 별도 프로세스에서 계속한다.
한 프로세스가 CPU 훈련과 Torch 전역 난수를 독점하는 실습이다.

## 빌드와 검사

C++17·CMake·Python 개발헤더, pybind11, NumPy, Gymnasium, CPU PyTorch가 필요하다.
선택한 가상환경에 의존성을 설치하고 그 Python의 개발헤더로 바인딩을 빌드한다.
아래 버전은 실습 의존성 선택이다. macOS의 torch 설치는 --index-url을 제외한다.
Windows에서는 셸의 줄바꿈과 Scripts/python.exe 경로를 맞춘다.

```sh
python -m pip install pybind11==3.0.1 numpy gymnasium==1.3.0
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/158-training-checkpoint/bindings -B out/study-158 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-158 --config Release -j2
ctest --test-dir out/study-158 -C Release --output-on-failure
python3 scripts/check_learning_training_checkpoint.py --python /path/to/python
```

## 저장하고 다시 시작하기

새 파일 `python/training_checkpoint.py`의 TrainingRun이 config·모델·Adam·Collector를 소유한다.
아래를 실행할 때 두 import 경로를 현재 작업 위치에 맞게 지정한다.

```python
import sys
sys.path.insert(0, 'out/study-158/python/Release')
sys.path.insert(0, 'docs/learn/checkpoints/158-training-checkpoint/python')
import torch
from training_checkpoint import TrainingRun

torch.set_num_threads(1)
run = TrainingRun()
try:
    run.advance()
    run.save('training.pt')
finally:
    run.close()
```

다른 프로세스에서는 다음을 실행한다.

```sh
/path/to/python docs/learn/checkpoints/158-training-checkpoint/python/checkpoint_demo.py \
  --module-dir out/study-158/python/Release --resume training.pt --out continued.pt
```

출력 옆 evidence 파일은 비교용 rollout/지표다. 학습 재개 입력은 training.pt 하나다.
checkpoint_contract는 연속 실행과 새 프로세스의 다음 rollout·모델·Adam·난수·환경·카운터를 비교한다.

## 구성과 경계

- atomic_save.py: 같은 디렉터리 임시 파일 → 직렬화/flush/fsync/close → replace. 단일 작성자·동기 저장.
- replay_env.py: 현재 에피소드의 seed/성공한 행동 열 재생, native bytes 대조, 미래 seed 생성기 별도 복원.
- training_checkpoint.py: 완료한 업데이트 경계만 저장. 실패한 advance는 추가 진행/저장 금지.
- policy_network.py: 생성자 용량 설정을 속성으로 보관. 계산 그래프는 유지.
- checkpoint_demo.py와 tests/checkpoint_contract.py: 별도 프로세스 재개·오류 후보·외부 RNG 보존 검사.
- 원래 Collector·PPO loss·규칙 C++는 유지한다. 모델 객체 전체 pickle 대신 상태 자료를 저장한다.
- 저장/재개 사이에 외부 코드가 Torch 전역 RNG를 소비하지 않는 계약이다. 전역 NumPy/Python random은 이 trainer에서 사용하지 않는다.
- 환경 내부 Session에 직접 가비지를 넣는 등 journal을 우회한 변경은 지원하지 않는다.
- CPU의 같은 소프트웨어/연산 조건에서 비교한다. runtime 식별자 일치가 서로 다른 장치의 비트 동일성을 증명하지는 않는다.
- 업데이트 중간·대전 상대·CUDA·scheduler·AMP·비동기 worker는 별도 상태 설계가 필요하다.
- parent-directory fsync·다중 파일 트랜잭션·전원 장애 내구성까지 보장하지 않는다.
- 신뢰한 파일만 읽는다. weights_only는 출처 인증과 자원 제한을 대체하지 않는다.
