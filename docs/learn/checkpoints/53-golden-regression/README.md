# 53 — 기준 기록과 최초 차이

게임·규칙·LRND/1 구현은52-state-hash와 같습니다. 비교 도구에는 Python 3.10 이상이 필요합니다. 실제Round를실행하는golden_trace,
검토한두골든파일, Python비교기를추가합니다. 각 Request는한틱에한번적용되며가비지는
틱전에대기합니다. tick번호는호출횟수이고종료후stopped도기록합니다.

```sh
cmake -S docs/learn/checkpoints/53-golden-regression -B out/study-53 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-53 --target golden_trace
ctest --test-dir out/study-53 -R '^golden_' --output-on-failure
```

mixed-v1/seed1은초기+48틱(49행), overflow-v1/seed1은초기+3틱(4행)입니다.
두기준의초기해시는c799419b70bbe2de. mixed의DOWN1/5회,고정11/14/17/48,
30틱중력47, overflow의game_over→stopped×2를읽어봅니다.

```sh
python3 docs/learn/checkpoints/53-golden-regression/tools/check_golden.py capture out/study-53/golden_trace out/study-53/candidate.json
```

capture는기존파일을덮어쓰지않습니다. check는기준을수정하지않습니다.
비교기종료0=일치,1=유효기록차이,2=읽기/형식/프로세스실패.
생성기의종료0=기록성공,1=생성/출력실패,2=인자오류입니다.
기준채택전작은규칙사례·입력·형식·환경을검토합니다.

규칙라벨checkpoint-52/1과바이트LRND/1은고정학습계약입니다. 태그만으로빌드출처를
보장하지않으며실제기록의revision/컴파일러/환경근거를별도로남겨야합니다.
전체검증: `python3 scripts/check_learning_golden.py` (루트Python검사에는.venv의pytest/pybind11사용).
