# 상태가 바뀌기 전에 배치를 제출하기

56-color-batch의 색상 정점·화면·규칙을 유지하면서 보드와 HUD의 시저 영역을 나눈다.
`clip_box.h`는 유효한 letterbox Layout의 논리 영역을 바깥쪽으로 반올림한 픽셀 Clip으로
변환한다. `flush_stream.h`는 같은 Clip을 묶고, 상태 변경·용량·finish에서 제출한다.
`flush_device.h`는 GL 연결, `flush_scene.h`는 장면 순서와 영역 선택을 맡는다.

```bash
cmake -S docs/learn/checkpoints/57-flush-boundaries -B out/study57 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study57 -j3
ctest --test-dir out/study57 --output-on-failure
out/study57/flush_demo
out/study57/tetris --seed 1
```

메뉴 Space 시작, 게임 Space 하드 드롭, Escape 메뉴 복귀, 종료 화면 Space 재시작.
초기 시드1은 보드·고스트·조각 1248정점 / 미리보기·점수 108정점, 총1356정점·2draw.
메뉴는3정점·1draw. 규칙과 정점 순서는 동일하다. 용량2304를 넘는 단일 입력은 거부하고,
여러 입력의 합이 잔여 용량을 넘으면 먼저 제출한다. 각 입력 그룹은 통째로 복사한다.

Sink가 Stream보다 오래 살아야 하며 Stream 사용 중 다른 렌더링 상태를 바꾸지 않는다.
시저 테스트와 뷰포트는 letterbox 프레임 시작이 설정한다. 실패 뒤 Stream은 재시도하지
않는다. CPU 입력 검증의 원자성과 이미 제출한 프레임 일부의 롤백을 구별한다.
소멸자는 제출하지 않는다. scene과 finish 반환값 모두 검사한 뒤 present한다.

CPU 검사는 SCRIPTED로도 구성 가능하다. 누적 골든에는 Python3.10 이상이 필요하다.
flush_real은 SDL offscreen GL 문맥에서 게임 장면 동등성과 겹치는 시저의 픽셀을 검사한다.
전체 검증: `python3 scripts/check_learning_flush.py`.
