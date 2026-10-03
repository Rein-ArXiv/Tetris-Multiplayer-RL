# 73 · 설정 저장: 기본값·검증·복구

72-idle-animation의 장식 표시와 캐릭터 ID를 `study-settings.cfg`에 저장한다.
현재 작업 디렉터리 기준이며 이 파일은 게임의 표현 설정이다.

## 형식과 정책

```text
version=1
decorations=1
character=player
```

- version=1 필수, 나머지 키가 없으면 기본값(true/player). CRLF·빈 줄·# 주석·주변 공백 허용.
- 전체4096바이트/물리적한줄256바이트 한계. NUL·잘못된값·미지원버전·중복/모르는키는
  후보 전체를 거절한다. 버전과 오류 위치를 별도로 보관한다.
- 설정 파서는 owning Config를 만들고 Preferences는 카탈로그의 정적 ID를 참조한다.
  핸들·위상·규칙상태는 저장하지 않는다.
- Session은 loaded/missing에만 저장한다. invalid/io_error에서는 기본값으로 조작하지만
  기존 파일을 덮어쓰지 않는다. 유효한 실제 설정 변경에만 저장하며 매 프레임 저장하지 않는다.
- 직렬화는 주석과 공백을 보존하지 않고 고정 순서의 형식으로 쓴다.

## 저장과 복구

같은 디렉터리에 임시 파일을 완성하고 대상 경로를 교체한다. `settings/private_file.cpp`는
현재 루트 `meta/private_file.cpp`의 writer를 네임스페이스만 바꾸어 재사용했다.
교체 전 실패에서는 기존 파일을 유지한다. POSIX의 교체 후 디렉터리 동기화가 실패하면
false라도 새 파일이 보일 수 있으므로 ‘저장 확인 실패’로 보고한다.
단일 프로세스가 경로를 관리한다는 전제이며 동시 편집 충돌을 합치지 않는다.

잘못된 파일을 복구하려면 프로그램을 종료한 뒤 내용을 고치고 재시작한다. 기본값부터
시작하려면 기존 파일을 다른 이름으로 **보관한 뒤** 재시작한다. 예를 들어 백업 이름이
비어 있는지 확인하고 `study-settings.cfg`를 `study-settings.bad.cfg`로 옮긴다.
새 실행은 missing 상태가 되어 다음 설정 변경을 새 파일에 저장한다. 자동 백업/삭제는 없다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/73-settings -B out/lesson73 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/lesson73
ctest --test-dir out/lesson73 --output-on-failure
cd out/lesson73
./tetris
```

캐릭터를 루크로 고르고 장식을 끈 뒤 종료·재실행한다. 같은 선택이 복원돼야 한다.
그다음 프로그램을 종료하고 version=99로 바꾸면, 재실행 시 기본값으로 동작하면서
파일 보호 진단이 나온다. 이 상태의 조작은 실행 중에만 적용한다.

저장소 루트의 `python3 scripts/check_learning_settings.py`로 누적 빌드·파싱·재시작·복구·
실제 파일 오류 주입·루트 회귀를 실행한다. 오류 주입은 Linux의 별도 검사이며 네이티브
Windows/macOS 저장 검증과 구분한다. 학습 사이트의 답안 기록 이동 기능과 별개다.
