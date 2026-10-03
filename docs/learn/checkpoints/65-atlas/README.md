# 65 · 아틀라스: 작은 이미지들을 한 텍스처에

64-glyph의 문자별 RGBA 텍스처를 하나의 R8 아틀라스로 바꾼다.
게임 규칙·문자 메트릭·폰트 로더·기준선은 유지한다.

- text/shelf.h: 순수 CPU shelf 배치, 네 방향 1텍셀 여백, 실패 시 커서 보존.
- renderer/mask_texture.h: R8 생성·부분 업로드·pixel unpack 상태 복원.
- renderer/glyph_atlas.h: 배치/업로드의 성공 확정, 단조 세대 번호, 명시적 초기화.
- renderer/atlas_line.h: 아틀라스를 빌리는 라벨, 잉크 영역의 UV, 오래된 세대 거절.
- renderer/image_quad.h: RGBA/coverage 샘플링 선택. 기본 이미지 경로는 유지한다.

## 실행

```sh
cmake -S docs/learn/checkpoints/65-atlas -B out/study65 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study65 -j3
ctest --test-dir out/study65 --output-on-failure
cd out/study65
./atlas_demo
./tetris --seed 1
```

SCRIPTED는 창 없이 CPU 데모와 대역 검사를 제공한다. 다중 구성 빌드는 해당
Release 등 실행 파일 위치를 사용하고 assets의 부모를 작업 디렉터리로 맞춘다.

## 계약

페이지 크기는 각 축 3~4096, 실제 GL 한도 이하여야 한다. R8/RED/UNSIGNED_BYTE,
level 0, GL_LINEAR이며 밉맵을 만들지 않는다. 여백은 주소만 예약하지 않고 매번
0으로 업로드한다. 전체 비우기는 CPU 참조를 새 세대로 바꾼다. 비우기 전에는
옛 UV를 가진 모든 CPU 그리기 큐를 제출해야 한다. 이는 GPU의 실행 완료 대기와 다르다.

공간 부족에는 자동으로 비우지 않는다. clear 후 라벨을 다시 준비하는 정책은
호출자가 정한다. 개별 영역 반환은 없고, 실패한 라벨의 앞부분이 등록한 영역도
clear 전까지 남는다. 이미 그릴 수 있는 다른 라벨은 유지된다. 글리프 중복 캐시와
한 번의 draw로 여러 글리프를 제출하는 배칭은 이 저장소의 책임이 아니다.

## 검사

```sh
python3 scripts/check_learning_atlas.py
```

배치 겹침/경계/거절 복원, 상태 오류 주입, UV 세대·소진, 실제 GL의 R8/선형
샘플링/기준선/합성, 루트 재사용 여백·업로드 실패와 누적 게임 회귀를 확인한다.
