# 색상 정점을 모아 한 번에 제출하기

55-screen-state의 화면·입력·규칙은 유지하고 불투명 렌더링 경로를 교체한다.
`color_batch.h`는 CPU의 순서 보존 정점 배열, `batch_scene.h`는 GameView→정점 구성,
`batch_device.h`는 공통 색상 셰이더·VAO/VBO와 업로드·제출 수명을 담당한다.

```bash
cmake -S docs/learn/checkpoints/56-color-batch -B out/study56 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study56 -j3
ctest --test-dir out/study56 --output-on-failure
out/study56/batch_demo
out/study56/tetris --seed 1
```

메뉴 Space 시작, 플레이 Space 하드 드롭, Escape 메뉴 복귀, 종료 화면 Space 재시작.
위치2+색상4 float의24바이트 정점을 사용한다. GameView에서 매 프레임 유효 prefix를
재구성하고 그 범위만 업로드·제출한다. 배치 용량2304정점은 현재 장면의보수적상한2172를
포함한다. 추가 콘텐츠로 초과하면 실패를 처리하며 도형을 조용히 버리지 않는다.

알파1만 허용한다. 순서: 빈/고정 셀→고스트→활동 조각→미리보기→종료X→점수.
삼각형마다 색상을 동일하게 복제한다. 현재 SDL 게임의 메뉴/보드는 비어 있지 않은
프레임당한draw로제출한다. 정점/픽셀/GPU 완료 시간이 그 비율로 감소한다는 뜻은 아니다.
이 단계는 텍스처·블렌드 상태를 바꿔가며 자동 flush하는 범용 배처가 아니다.

CPU 검사는 SCRIPTED 빌드로도 가능하다. 누적 골든 검사에는Python3.10이상이 필요하다.
`batch_real`은 SDL GL 문맥에서 이전 개별 제출과 새 배치의 프레임버퍼를 비교한다.
검사 전체: `python3 scripts/check_learning_batch.py`.
