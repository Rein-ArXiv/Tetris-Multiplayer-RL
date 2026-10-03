# 49 — 7-bag와 조건부 선택

48-input-mask의 누적 구현은 유지합니다. 추가 파일은 simulation/seven_bag.h,
src/bag_demo.cpp, tests/bag_contract.cpp이며 CMake에 두 CPU 타깃을 등록합니다.

SevenBag는 남은 종류의 순서와 개수를 소유합니다. take(index)는 선택한 값을 복사하고
뒤 원소를 안정적으로 당깁니다. 인덱스는 호출자가 정하고, 빈 가방은 유효한 take에서
채웁니다. 실패는 빈 상태를 포함해 보존합니다. 모든 가능한 선택 경로는 7!개입니다.

bag_demo는 의도적으로 정한 인덱스로 뽑은 한 순열을 ScriptedSource에 복사한 뒤 실제
Round의 현재/미리보기/하드드롭 승계에 연결합니다. 이 공급기는 한 패턴을 반복합니다.
tetris의 공급기 선택은 그대로이며 자동 난수 가방으로 교체하지 않습니다.

```sh
cmake -S docs/learn/checkpoints/49-seven-bag -B out/study-49 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-49 --target bag_demo bag_contract queue_contract
./out/study-49/bag_demo
ctest --test-dir out/study-49 -R '^(bag_contract|queue_contract)$' --output-on-failure
```

항상 첫 원소를 고르면 I J L O S T Z, 끝 원소를 고르면 Z T S O L J I가 됩니다.
가방의 중복 방지와 선택의 균등성은 서로 다른 계약입니다.
