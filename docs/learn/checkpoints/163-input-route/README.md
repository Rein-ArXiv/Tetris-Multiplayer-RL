# 목표 경로와 실제 틱 결과

누적 실습에 bot/route_player.h를 추가한다. 기존 action_plan이 회전·이동·드롭을
값 복사본에서 예측하고, Player는 그 요청을 한 번씩 내보낸 뒤 실제 전이를 확인한다.
이 경로는 연속 틱을 가정한다. 중간 대기/가비지 등으로 상태가 달라지면 stale이 된다.

```sh
cmake -S bindings -B build-native -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-native --config Release
ctest --test-dir build-native -C Release --output-on-failure
./build-native/route_demo
```

다중 구성 생성기의 실행 파일은 Release 폴더에서 찾는다. Windows에서는 .exe를 쓴다.
기존 Python 검사의 의존성은 누적 실습과 동일하다. C++ 경로 검사 자체는 그래픽·ORT가
필요하지 않으며 route_contract 타깃만 빌드해 실행할 수 있다.

start → issue → 실제 Round.tick 한 번 → observe 순서다. issue가 돌려준 optional이
비어 있으면 마스크를 실행하지 않고 status를 확인한다. awaiting_feedback에서 다시
issue해도 같은 입력을 중복 제공하지 않는다. observe 성공만 confirmed를 전진시킨다.
마지막 실제 전이까지 일치해야 complete다. 경로가 오래되면 새 상태에서 start한다.

데모의 시드와 결정 예산은 재생을 관찰할 예제 조건이다. 각 입력을 출력하고 실제 틱에
적용한다. 실행 시간의 지연이나 손 속도는 이 경로 실행기의 계약에 포함하지 않는다.
현재 게임의 placement 끝점 API와 이 누적 실습의 실제 틱 경로를 구별한다.
