# RGBA8 픽셀에서 GPU 텍스처와 플레이어 아이콘으로

57-flush-boundaries의 색상 Stream을 유지하고, 끝난 뒤 고정 위치의 8×8 아이콘을
48×48 논리 사각형으로 표시한다. 파일 디코딩・핸들 저장소・회전/tint는 포함하지 않는다.

- rgba_pixels.h: 비소유 CPU 뷰, 길이와 정수 범위 검사. 실제 접근 가능한 배열은 호출자 계약.
- badge_pixels.h: 불투명 절차적 8×8 자료, 왼쪽 위 빨간 방향 표시.
- texture.h: 단일 업로드・GL 크기/오류 검사・unpack 상태 저장/정규화/복원・소유권 확정.
- texture_quad.h: 위치/UV 16바이트 정점・sampler 유닛0・고정 사각형, 명시적 호출 경계.

```bash
cmake -S docs/learn/checkpoints/58-texture-storage -B out/study58 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study58 -j3
ctest --test-dir out/study58 --output-on-failure
out/study58/texture_demo
out/study58/tetris --seed 1
python3 scripts/check_learning_texture.py
```

GL 없는 계약 검사는 SCRIPTED로 구성 가능하다. 누적 골든에는 Python3.10 이상 필요.
메뉴 Space 시작, 게임 Space 하드 드롭, Escape 메뉴 복귀, 종료 Space 재시작.
색상 Stream의 통계에 아이콘 1draw를 더하면 전체 메뉴2draw・게임3draw이다.
원본 업로드256바이트와 사각형 정점96바이트는 한 번 만들며 매 프레임 재업로드하지 않는다.

업로드는 현재 활성 유닛을 바꾸지 않고 그 유닛의 Texture2D 바인딩과 unpack 상태를
저장/복원한다. 복원 실패도 실패 반환이다. Quad는 유닛0・sampler 객체 없음・blend/depth
비활성의 전용 파이프라인이며 색상 Stream finish 뒤 호출한다. 소유 객체와 함수표보다
GL 컨텍스트가 오래 살아야 한다. Texture.name은 빌린 이름이고 외부에서 삭제하지 않는다.

texture_real은 원본 바이트 복사・정렬/행/PBO 복원・세 크기의 픽셀과 아이콘 밖 보존을
SDL offscreen 문맥에서 검사한다. 일반 모니터 표시 속도나 타 OS 검증과는 구분한다.
