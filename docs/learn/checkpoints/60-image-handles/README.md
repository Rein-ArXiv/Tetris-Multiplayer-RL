# 이미지 핸들: 저장소가 소유하고 화면이 선택한다

59-image-decode의 PNG 로더·Texture·Quad와 게임 규칙을 유지한다. 세션의 ImageStore가
파일 메뉴 아이콘과 절차적 주황 플레이 아이콘을 소유하며 화면은 Handle로 선택한다.
menu에서 파일 아이콘, playing/game-over에서 주황 아이콘이다. Escape 복귀 시 파일 아이콘.

- handle_pool.h: GPU를 모르는 unique_ptr 슬롯 풀. 64비트 토큰=발급32|슬롯+1의32.
- 한 실행 파일의 모든 풀/타입/번역 단위가 공유하는 단일 렌더링 스레드 카운터.
- clear/재초기화에서도 번호를 되감지 않음. 2^32-1회 발급 이후 추가 삽입 실패.
- image_store.h: decode→Texture 소유→등록. 실패0, size/draw/unload 실패false.
- session_icons.h: 청록 얼굴 색을 주황으로 바꾸는 독립 콘텐츠 생성.
- 등록 실패 시 소비된 Texture 소멸자가 GPU 이름 정리. 컨텍스트/함수표가 저장소보다 오래 살아야 함.
- draw는 고정 Quad에 즉시 제출. 다른 CPU 큐와 연결하면 자원 해제 전 그 사용을 끝낼 것.

```bash
cmake -S docs/learn/checkpoints/60-image-handles -B out/study60 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study60 -j3
ctest --test-dir out/study60 --output-on-failure
out/study60/handles_demo
out/study60/tetris --seed 1 --image out/study60/assets/player.png
python3 scripts/check_learning_handles.py
```

GL 없이 검사하려면 STUDY_PLATFORM=SCRIPTED. 누적 골든은 Python3.10 이상.
--image PATH는 마지막 두 인자이며 생략 시 현재 작업 디렉터리의 assets/player.png.
Windows 다중 구성 빌드는 실행 파일의 Release 등의 구성 디렉터리를 경로에 반영한다.

main은 Texture/GLuint를 직접 보관하지 않는다. 복사한 핸들은 비소유 참조이고 참조 카운팅을
하지 않는다. 유효한 토큰을 넘긴 unload는 이미지를 해제하므로 소유자만 호출하도록 설계한다.
핸들은 같은 실행의 저장소 안에서만 사용하며 영구 콘텐츠 ID나 보안 토큰으로 저장/전송하지 않는다.
풀 소멸자/for_each 콜백이 같은 풀을 변경하는 재진입은 지원하지 않는다.

검사: 오래된/다른 풀/다른 TU/clear/재초기화 토큰, 벡터 확장 중 포인터 유지, 할당 및 등록 실패,
번호 소진, 출력 보존, 실제 두 아이콘×3Layout 전체 픽셀, 루트 공개 API 추출과 규칙 골든.
이전 texture/decode 도구는 각 단계의 독립 진단 도구로 그대로 둔다.
