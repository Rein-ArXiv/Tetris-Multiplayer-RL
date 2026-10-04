# PPO 수집·업데이트·평가 체크포인트

실제 누적 환경에서 경험을 수집하고, 고정한 목표값으로 작은 PPO 업데이트를 수행한다.
별도 환경에서 평가하며 종료·외부 제한·수집 구간의 끝을 구분한다.

## 실행

C++17·CMake 3.18 이상·Python 개발헤더와 pybind11, NumPy, CPU PyTorch가 필요하다.
실습 의존성 조합은 CPython 3.12와 PyTorch 2.8이다. 선택한 Python으로 새 가상환경을 만든다.
사용할 가상환경에서 다음을 실행한다. 아래 Python/pybind 경로는 실제 절대 경로로 바꾼다.
설치 버전은 실습 의존성 선택이며 관측의 고정 콘텐츠 개수를 뜻하지 않는다.

```sh
python -m pip install pybind11==3.0.1 numpy gymnasium==1.3.0
python -m pip install torch==2.8.0 --index-url https://download.pytorch.org/whl/cpu
python -m pybind11 --cmakedir
cmake -S docs/learn/checkpoints/157-ppo/bindings -B out/study-157 \
  -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python \
  -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build out/study-157 --config Release -j2
ctest --test-dir out/study-157 -C Release --output-on-failure
/path/to/python docs/learn/checkpoints/157-ppo/python/ppo_demo.py \
  --module-dir out/study-157/python/Release
```

macOS에서는 torch 설치 줄의 --index-url을 제외한다. Linux/Windows는 위 CPU 인덱스를 사용한다.
Windows는 셸의 줄바꿈 문법을 맞추며 확장은 해당 OS/CPU/Python용으로 빌드한다.
가상환경 Python은 POSIX의 bin/python, Windows의 Scripts/python.exe다.
[공식 OS별 안내](https://pytorch.org/get-started/previous-versions/#v280)
독립 bindings 구성에는 그래픽·서버 SDK가 필요하지 않다. 전체 누적 구성에서
`STUDY_BUILD_PY=ON`으로도 같은 바인딩을 포함한다.

## 추가·변경 파일

- python/returns.py: terminated와 boundary를 분리한 GAE·표준화.
- python/ppo_loss.py: 고정 old logp·advantage를 사용하는 clipped surrogate와 표본 진단값.
- python/ppo.py: 관측/마스크 소유, 수집·고정 표적·업데이트·독립 평가.
- python/ppo_demo.py: 작은 결정 예산·남은 마지막 배치·합성 아닌 환경 보상으로 동작 확인.
- python/gym_env.py: 보상과 따로 원래 삭제 줄 사건을 info.lines_cleared에 노출.
- tests/ppo_contract.py, bindings/CMakeLists.txt: 수치·native endpoint·업데이트·평가 경계 검사.

## 실습의 경계

- 보상과 그 시간 단위의 discount를 함께 저장한다. native env의 info.discount가 없으면 명시한 fallback_discount를 쓴다.
- lambda는 결정마다 trace를 감쇠한다. 실제 종료만 bootstrap을 없애고 reset 경계는 trace를 끊는다.
- rollout 끝은 마지막 실제 관측의 가치로 이어 붙인다. reset 관측을 전이의 끝에 넣지 않는다.
- 수집한 old logp·마스크·리턴 표적은 optimizer epoch 동안 유지하고 새 표본을 모은 뒤 갱신한다.
- 확률비 clipping과 gradient norm clipping은 별개이며 정책/성능의 절대 보증이 아니다.
- singleton rollout은 advantage를 그대로 사용한다. 마지막 미니배치도 실제 길이를 사용한다.
- 평가의 argmax와 훈련의 확률 표집은 서로 다른 선택 조건이다. 시드·예산·보상·규칙을 기록한다.
- demo의 평가 환경은 shaping을 끈다. lines/score·환경 reward·종료/제한을 각각 읽는다.
- 현재 PPO는 즉시 배치/대전 공동 결정, 체크포인트는 실제 입력 틱 경로다. 같은 알고리즘의 이름이 같은 전이를 뜻하지 않는다.
- 짧은 CPU 실행은 수집과 gradient 경로의 확인이다. 봇 실력은 별도의 충분한 학습과 평가가 필요하다.

```sh
python3 scripts/check_learning_ppo.py --python /path/to/python
```
