# 47차시 · 입력의 두 보관 경계

46-catch-up에서 플랫폼 키 저장소를 KeyEdges로 바꾸고 포커스 취소를 틱 버퍼에 연결합니다.
core/key_edges.h는 현재 루트의 같은 파일과 동일한 구현입니다. 이전 체크포인트는 보존합니다.

```sh
cmake -S docs/learn/checkpoints/47-input-edges -B out/study-47 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-47 --target key_edges_contract frame_contract
ctest --test-dir out/study-47 -R '^(key_edges_contract|frame_contract)$' --output-on-failure
```

SDL2 개발 라이브러리가 있다면 SDL 구성의 input_pipeline·hard_drop_input을 실행합니다.
CTest가 dummy 드라이버를 지정하며 창·GL 컨텍스트는 생성하지 않습니다.
실제 tetris 타깃은 SDL과 OpenGL3.3 Core 실행 환경이 필요합니다.

프레임은 전이 기록을 초기화하고, 틱은 pending 요청을 소비합니다.
취소는 오래된 pending을 비운 뒤 같은 프레임의 새 요청을 수집합니다.
bool은 발생 여부를 합치며 물리적인 모든 탭의 횟수·순서를 보존하지 않습니다.
