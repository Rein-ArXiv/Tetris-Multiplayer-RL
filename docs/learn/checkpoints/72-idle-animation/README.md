# 72 · 대기 애니메이션: 표현용 시계와 난수

71-character-art의 캐릭터·자원·배치를 유지하고 메뉴의 작은 대기 효과를 추가한다.
테두리 알파·일러스트 알파·장식 회전만 바뀌며 입력 영역과 규칙 상태는 유지한다.

## 계약

- `presentation/idle_animation.h`: 별도 Clock·시작 위상 두 개·순수 Frame 조회.
  주기 4초, 활성 호출당 최대 0.1초 진행. 초과 시간은 버리며 미처리 시간으로 보관하지 않는다.
  음수/비유한 입력은 활성 여부와 관계없이 거절하고 상태를 유지한다.
- main은 `메뉴 && 장식 표시 && 유효 layout`에서만 시계를 진행한다. 게임 화면이나
  최소화에서는 정지한다. 껐다 켜면 보관한 위상에서 이어가며 비활성 표시는 정적인 기본값이다.
- 시작 위상은 독립 로컬 XorShift64Star에서 두 번 추출한다. 생성기는 생성자에서 끝나며
  프레임 조회는 난수를 소비하지 않는다. 고정 기본 시드로 실행마다 같은 위상에서 시작한다.
- `sample`은 const 조회다. 같은 상태에서 여러 번 불러도 상태·결과가 변하지 않는다.
  float/sin/GPU의 결과를 플랫폼 간 비트 단위 동일하다고 보장하지 않는다.
- `presentation/art_view.h`에 불투명도 인자를 추가한다. 기본값 1로 기존 호출을 유지하며
  [0,1] 밖이나 비유한 값은 거절한다. 이미지 자체의 알파에 이 값을 곱한다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/72-idle-animation -B out/lesson72 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/lesson72
ctest --test-dir out/lesson72 --output-on-failure
cd out/lesson72
./tetris
```

메뉴에서 테두리와 일러스트의 밝기·장식 회전을 관찰한다. ‘장식 표시’를 끄면 움직임을
멈추고 선택한 캐릭터 그림은 정적으로 남는다. 선택기/시작 버튼은 동일 위치에서 동작한다.
게임에 진입하면 대기 효과는 멈춘다. 메뉴로 돌아오면 보관한 위상에서 이어 간다.
설정과 효과 시계는 실행 중 값이며 저장 기능을 추가하지 않았다.

저장소 루트의 `python3 scripts/check_learning_idle_animation.py`로 누적 빌드·시간 정책·
실제 진행 중 규칙 해시 분리·offscreen GL 알파와 범위·기존 이미지/위젯/포인터 회귀를 검사한다.
