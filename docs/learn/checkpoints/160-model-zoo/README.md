# 모델 zoo: 같은 조건으로 후보 비교

`159-colab-workflow`의 누적 소스를 유지하고 `python/model_zoo.py`,
`evaluation_summary.py`, `tests/model_zoo_contract.py`를 추가한다.
저장된 CPU TrainingRun을 읽고 별도 RoundEnv에서 greedy 평가한다.

```sh
cmake -S bindings -B build-native -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
```

실행할 Python에는 pybind11, NumPy, CPU PyTorch, Gymnasium이 필요하다.
`--module-dir`에는 현재 Python으로 빌드한 study_py 파일의 폴더를 지정한다.
누적 experiment로 만든 training.pt나 TrainingRun.save 산출물을 사용한다.

```sh
python python/model_zoo.py /path/to/run-a/training.pt /path/to/run-b/training.pt \
  --module-dir build-native/python/Release --seeds 101 202 303 \
  --limit 30 --split validation --out /path/to/comparison.json
```

위 시드와 예산은 이 실습의 예제 설정이다. 실제 평가 목록과 예산은 실행 전에 정한다.
report에는 모델/코드/확장 식별값, 동일 프로토콜, 개별 시드 결과, 요약,
첫 후보 대비 줄 수 차이가 남는다. 종료·환경 제한·평가 예산 종료를 구별한다.
동일한 보고서 경로를 다시 쓰지 않는다. JSON 기록은 학습 실험 산출물이며
HTML 학습 진도 이동 기능과는 관계없다.

학습 결정 수는 난이도 등급이 아니다. 검증 시드는 후보 선택에, 별도 테스트 시드는
선택 완료 뒤 평가에 사용한다. split 이름만으로 시드 중복이나 테스트 반복 사용을
자동 방지하지 않는다. 단일 보드 배치 결과를 속도 제한이 있는 대전 승률로 확대하지 않는다.
