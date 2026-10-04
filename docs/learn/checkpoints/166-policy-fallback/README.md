# 실패를 보존하는 연습과 엄격한 경기 재현

누적 캐릭터·Session·ONNX 정책·틱 실행기에 RunStatus와 두 보드 Match를 연결한다.
Session의 읽기 전용 Round 조회로 기존 Duel과 전이를 대조한다. Driver.queue_garbage는
자신이 소유한 상태를 확인한 뒤 경기 소유자의 틱 후 공격만 받아 생각 시간을 유지한다.

```sh
cmake -S bindings -B build-native -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
cmake -S inference -B build-inference -DCMAKE_BUILD_TYPE=Release -DORT_ROOT=/path/to/onnxruntime-sdk
cmake --build build-inference --config Release --target policy_match_probe
./build-inference/policy_match_probe /path/to/policy.onnx 91 240 practice
./build-inference/policy_match_probe /path/to/policy.onnx 91 240 strict
```

예제 인자는 이 Session 규약의 모델·시드·틱 예산·대체 허용 모드다. practice 실행은
정책 실패 시 첫 합법 후보를 사용하지만 보상 적격성은 잃는다. strict는 첫 정책 오류에서
진행을 중단한다. 둘 다 마지막에 원래 정책만 사용한 독립 재현의 verdict를 출력한다.
정상 모델과 비유한 출력 모델의 차이는 tests/policy_match_inference.py로 비교한다.

각 프레임은 봇 입력·선택 출처·적격성·선택된 행동·양쪽 점수/종료·두 상태 bytes를
기록한다. probe의 사람 입력은 이 실험에서 빈 입력으로 고정한다. 연습의 승리 표시와
서버가 확인한 보상은 별개다. 이 콘솔은 BP를 저장하거나 서버 티켓을 발급하지 않는다.
현재 게임의 실제 발급·검증·저장은 meta/bot_challenges.cpp와 bot/reward_replay.h를
레퍼런스로 대조한다. 누적 지급 저장 실습은 meta/의 책임을 유지한다.

Windows/다중 구성 생성기는 Release 경로와 .exe를 맞춘다. ONNX SDK/로더 경로는
대상 OS와 일치해야 한다. 서버 운영에서 모델 배포와 진행 티켓 수명은 함께 관리한다.
