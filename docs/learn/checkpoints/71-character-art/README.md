# 71 · 아이콘·일러스트: 콘텐츠와 규칙 경계

70-pointer-mapping의 프레임 배치·메뉴·규칙을 유지하고 캐릭터 표를 연결한다.
선택한 ID에서 이름·작은 아이콘·메뉴 일러스트를 조회한다.

## 계약

- `content/characters.h`: 고유 ID·표시 이름·두 이미지 경로. 중복 ID와 기본 ID 누락을
  컴파일 때 거절한다. 정적 리터럴을 참조하며 빈 경로는 대체 그림을 뜻한다.
- `client/menu_model.h`: 선택 ID를 보관하고 인덱스는 현재 표에서 파생한다.
  `select_character`는 알려진 ID를 받아들이면 true이며 임시 입력 버퍼를 보관하지 않는다.
- `content/art_set.h`: 시작 때 역할을 준비하고 같은 경로 문자열은 한 번만 읽는다.
  실패 값 0도 캐시한다. 유효한 fallback이 있으면 역할별 파일 실패는 초기화 성공이다.
- ImageStore가 이미지를 소유한다. ArtSet은 핸들을 빌리며 해제하지 않는다.
  준비 중 예외에서 외부 ImageStore의 자원까지 롤백하지 않는다.
- `presentation/image_fit.h`: 정수 contain. 제한 변·내림·중앙 배치·좌표 범위를 검사한다.
  얇은 변이 1논리 단위 미만이면 생략한다. 정수 양자화로 종횡비에는 작은 차이가 남는다.
- `presentation/art_view.h`: 크기 조회→배치→그리기. 무효/오래된 핸들은 실패,
  그릴 사각형이 없는 배치는 정상 생략이다.

`player.png`와 생성 배지는 유지하고 `bot.png`·`opponent.png`는 저장소
`assets/icons/`의 기존 PNG를 복사했다. 새 일러스트를 생성한 것이 아니다.
CMake configure가 실행 폴더의 assets를 준비한다. 상대 경로는 실행 작업 디렉터리 기준이다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/71-character-art -B out/lesson71 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/lesson71
ctest --test-dir out/lesson71 --output-on-failure
cd out/lesson71
./tetris
# 종료 후 플레이어 이미지 실패를 관찰
./tetris --image missing.png
```

메뉴에서 선택기 화살표 또는 키보드 Up/Down·Left/Right로 캐릭터를 고른다.
이름과 두 그림이 함께 바뀌고 선택한 아이콘은 게임 진입 후에도 유지된다.
파일이 없어도 이름과 ID는 유지한다. 선택은 실행 중 값이며 파일 저장은 없다.
이미지를 교체하면 실행 assets를 갱신하고 재시작한다.

저장소 루트의 `python3 scripts/check_learning_character_art.py`는 누적 빌드와 계약,
실제 offscreen GL 렌더링, 표 재정렬·중복 ID 거절, 루트 표현 회귀를 검사한다.
수동 GUI나 네이티브 Windows/macOS 검증을 대신하지 않는다.
