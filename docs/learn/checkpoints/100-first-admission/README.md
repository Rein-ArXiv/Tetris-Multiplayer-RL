# 100 첫 요청과 연결 인계

99-relay-choice의 소켓·프레임·규칙 계층에 첫 요청의 제한과 인계를 추가한다.
[HTML 강의](../../index.html#lesson-100)를 보며 단계 전환에서 보존할 상태를 추적한다.

```sh
cmake -S docs/learn/checkpoints/100-first-admission -B out/study100 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study100 -j3
ctest --test-dir out/study100 --output-on-failure
./out/study100/admission_probe
./out/study100/admission_contract
python3 scripts/check_learning_first_admission.py
```

FirstAdmission은 시간과 바이트를 받아 요청 경로와 파서 꼬리를 한 번 인계한다.
실제 loopback에서 소켓까지 옮겨 후속 프레임과 RoundPlay의 첫 틱을 처리한다.
TYPE50·128바이트·낯선 프레임4개는 학습용 정책이다. 인증·실제 매칭은 포함하지 않는다.
Windows 다중 구성은 --config Release와 CTest -C Release를 사용한다.
