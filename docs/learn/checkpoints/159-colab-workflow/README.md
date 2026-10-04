# Colab 실행 흐름: 준비·작업 프로세스·기록

누적 CPU PPO/재개 코드는 유지하고 새 프로세스에서 실험을 실행하는 작은 감독자를 추가한다.
Colab의 실제 GPU trainer는 현재 프로젝트의 train_model_zoo_colab.ipynb를 참고한다.
이 체크포인트의 TrainingRun은 CPU 재개 계약을 유지한다.

## 빌드

C++17·CMake·Python 개발헤더, pybind11·NumPy·Gymnasium·CPU PyTorch가 필요하다.
실습용 가상환경에서 설치하고 현재 Python/pybind11 절대 경로를 CMake에 지정한다.
아래 의존성 버전은 실습 조합이다. macOS torch 설치는 --index-url을 제외한다.
Windows에서는 셸 줄바꿈과 Scripts/python.exe 경로를 맞춘다.

```sh
python -m pip install pybind11==3.0.1 numpy gymnasium==1.3.0
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/159-colab-workflow/bindings -B out/study-159 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-159 --config Release -j2
ctest --test-dir out/study-159 -C Release --output-on-failure
```

## 실행

```sh
/path/to/python docs/learn/checkpoints/159-colab-workflow/python/experiment.py launch \
  --module-dir out/study-159/python/Release \
  --output-root out/study-experiments --name first-smoke --preset smoke
```

first-smoke/manifest.json, train.log, training.pt, result.json을 확인한다.
manifest는 실제 인자 배열·예산·소스와 모듈 해시·Python/패키지 정보를 기록한다.
정상 종료 전에 런타임이 사라졌다면 result.json이 없을 수 있으며 성공으로 간주하지 않는다.
훈련 상태 파일은 완성한 업데이트마다 저장한다. 복원 자체는 TrainingRun.load 계약을 따른다.

long은 같은 코드/모듈/Python/패키지의 성공한 smoke 폴더를 --smoke-run으로 요구한다.
새 이름을 사용하고 학습량은 PRESETS에서 확인한다. 예제 예산은 봇 난이도를 뜻하지 않는다.
출력 root를 Drive 마운트 경로로 선택할 수 있지만 파일 교체·원격 동기화·전원 장애의
내구성을 같은 보장으로 해석하지 않는다. 구체적인 자원 가용성은 Colab 공식 안내를 확인한다.

## 추가한 책임

- experiment.py: 설정 검증, 독립 run 폴더, 동일 smoke 확인, 작업 프로세스 시작, 종료/산출물 기록.
- process_runner.py: 인자 배열 실행, 로그와 터미널 동시 출력, 비정상 종료 전파, 직접 자식 정리.
- experiment_contract.py: 실제 native CPU 작업과 파일 생성·중복 실행/잘못된 preset/미확인 long 거절.
- 기존 규칙·환경·PPO·TrainingRun은 바꾸지 않는다.

노트북 커널에 확장을 import한 뒤 파일만 다시 빌드해도 로드된 객체는 교체되지 않는다.
현재 프로젝트의 prepare_native/run_smoke는 독립 빌드와 새 프로세스로 이 경계를 확인한다.
소스 해시는 기록한 자료의 동일성 정보다. 진짜 빌드에 쓰인 소스라는 증거에는 빌드 과정도 필요하다.
CPU smoke 성공은 GPU·ONNX·장기 학습/정책 성능 검증을 대신하지 않는다.

```sh
python3 scripts/check_learning_colab_workflow.py --python /path/to/python
```
