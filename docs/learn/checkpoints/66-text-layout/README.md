# 66 · 글자 배치: advance·베이스라인·커닝

65-atlas에 CPU Layout·Font::kerning·Paragraph·AtlasText를 추가한다.
기존 단일 행 Line과 AtlasLine은 비교와 누적 검사에 남긴다. main은 Paragraph를
한 번 준비해 폭과 그리기 위치를 공유하고, 패널의 논리 폭 기준으로 가운데 정렬한다.

- text/layout.h: 순수 메트릭 누적·LF에 해당하는 newline·최종 행 advance 폭·비트맵 상자 합집합.
- text/paragraph.h: 최대16스칼라를 글리프·커닝·배치 결과로 변환. LF허용, CR/TAB거절.
- renderer/atlas_text.h: 준비된 펜/기준선 좌표로 공유 아틀라스에 그린다.
- Font::kerning: 같은 폰트의 글리프 번호 쌍, 높이1~128, 픽셀 단위의 signed 조정량.

## 실행

```sh
cmake -S docs/learn/checkpoints/66-text-layout -B out/study66 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study66 -j3
ctest --test-dir out/study66 --output-on-failure
cd out/study66
./text_layout_demo assets/NanumGothic.ttf
./tetris --seed 1
```

SCRIPTED에서는 텍스트 데모·CPU 검사가 가능하다. 다중 구성 빌드는 실제 실행 파일
위치를 사용하되 assets의 부모를 작업 디렉터리로 둔다.

## 좌표와 정책

첫 기준선은0, 다음 기준선은 ascent-descent+line_gap만큼 아래다.
빈 문자열도 한 줄이고 마지막 LF 뒤의 빈 줄도 유지한다. width는 각 줄 최종
advance의 최댓값, height는 메트릭 상자 높이다. ink는 비트맵 사각형의 합집합으로
실제 비영 coverage의 최소 경계를 뜻하지 않는다. 공백은 advance만 차지한다.
Layout의 수량/수치 범위는 학습 구현의 정책이며 전체 폰트 형식의 제약이 아니다.
스칼라별 수평 커닝이며 복잡한 shaping·fallback·자동 줄바꿈은 제공하지 않는다.

## 검증

`python3 scripts/check_learning_text_layout.py`는 독립 kern 테이블과 배치 산술,
실패 보존/빈 줄/후행 줄바꿈, 실제GL 픽셀, 현재 renderer 측정의 GL무호출·반환범위를
검사한다. 기존 게임 규칙·해시 골든을 유지한다.
