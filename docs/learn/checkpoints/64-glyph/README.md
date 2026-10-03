# 64 · 글리프: 폰트에서 비트맵으로

UTF-8 화면 이름을 실제 한글 라벨로 표시한다. 게임 규칙과 입력은 63-utf8 그대로다.

- text/font.h/.cpp: 패키지 TTF 바이트 소유, glyph index·메트릭·coverage.
- text/line.h: 단일 행, 최대 16스칼라, advance 누적과 straight white RGBA.
- renderer/text_line.h: 글리프별 이미지 핸들, 부분 실패 정리, 기준선 배치.
- src/main.cpp: 메뉴/플레이 라벨을 한 번 준비하고 화면 상태로 선택.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/64-glyph -B out/study64 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study64 -j3
ctest --test-dir out/study64 --output-on-failure
cd out/study64
./glyph_demo assets/NanumGothic.ttf ga.pgm
./tetris --seed 1
```

CMake는 assets의 이미지·폰트·라이선스를 빌드 디렉터리로 복사한다. 다중 구성
도구의 실행 파일은 Release 등 해당 경로에서 찾고 작업 디렉터리는 assets의 부모로 둔다.
SCRIPTED 구성은 창 없이 CPU 예제와 검사를 제공한다.

## 계약

폰트는 신뢰하는 패키지 자산만 받는다. stb_truetype는 업로드 폰트의 안전성을 검증하는
파서가 아니다. 16MiB 입력·1~128 요청 높이·한 변 1024 출력 제한은 자원 정책이다.
폰트 바이트는 info가 사용하는 동안 수정/재할당하지 않는다.

coverage는 위에서 아래 순서의 8비트 마스크다. 빈 공백도 advance를 유지한다.
미지원 문자는 glyph 0과 missing 상태로 표현하고, 생성 실패는 nullopt다.
Glyph/Line의 사용하지 않는 멤버도 값 초기화해 이동 시 불확정 값을 읽지 않는다.

Font를 파괴해도 Glyph가 소유한 coverage는 남는다. TextLine보다 ImageStore와
GL 컨텍스트가 오래 살아 있어야 한다. TextLine은 예외/실패/파괴 때 등록한 핸들을 회수한다.
고정 높이 16·글리프당 텍스처를 사용하며, 커닝·shaping·fallback·아틀라스·DPI 재굽기는
이 구현에 포함하지 않는다. 폰트 이름과 라이선스 출처는 assets/FONT-SOURCE.md에 있다.

## 검사

```sh
python3 scripts/check_learning_glyph.py
```

독립 SFNT 테이블 기준 메트릭/상자, 공백/누락 문자, 소유권 이동/재로드 실패,
부분 업로드 회수, UBSan, 실제 GL의 마스크/기준선/합성과 루트 실패 회귀를 검사한다.
