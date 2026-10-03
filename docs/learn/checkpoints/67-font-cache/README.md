# 67 · 폰트 캐시: 해상도와 키 설계

`66-text-layout`에서 출발하는 누적 프로젝트다. 논리 메트릭과 화면용 비트맵을
분리하고, CPU 비트맵과 GPU 아틀라스 영역을 각각 재사용한다.

## 구현 경로

- `text/raster_policy.h`: 1/8 배율 양자화·정수 굽기 높이·검증한 필드의 키 포장.
- `text/font.h/.cpp`: 논리 메트릭 조회, 기존 논리 래스터 API, 새 device 래스터 API.
  래스터 두 입구는 공통 구현을 사용하며 실제 비트맵 축을 할당 전에 제한한다.
- `text/glyph_cache.h`: Font 독점 소유·64항목·공유 불변 스냅샷. 재로드 실패 보존,
  성공 시 lookup 제거. 외부 소유 스냅샷은 살아 있으므로 프로세스 전체 메모리 상한은 아니다.
- `text/cached_paragraph.h`: 논리 advance/kerning/baseline과 환산한 비트맵 상자로 배치.
- `renderer/glyph_cache.h`: 스냅샷을 소유한 채 Region 재사용·revision 변경 감지.
- `renderer/cached_text.h`: 전체 준비 성공 뒤 라벨 확정·옛 revision 그리기 거절.
- `src/main.cpp`: 실제 viewport/logical 높이 비율로 계획. 정수 굽기 높이 변경 시
  옛 두 라벨·문단·lookup 해제 → 페이지 한 번 clear → 두 라벨 함께 준비.

기존 Paragraph/AtlasText와 이전 검사들은 비교·회귀용으로 보존한다. 논리 크기
1~128, 정수 굽기 높이 1~2048, 실제 비트맵 축 1024 이하를 구별한다. 아틀라스는
512×512여서 CPU 비트맵 정책 안에서도 페이지에 들어가지 않을 수 있다.

## 실행

```sh
cmake -S docs/learn/checkpoints/67-font-cache -B out/lesson67 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/lesson67
ctest --test-dir out/lesson67 --output-on-failure
cd out/lesson67
./tetris
```

`assets/NanumGothic.ttf`와 라이선스는 빌드 디렉터리에 복사된다. 신뢰한 패키지
폰트만 읽는다. 사용자 업로드 폰트를 검증하는 파서가 아니다.

## 검증과 실패 정책

저장소 루트에서 `python3 scripts/check_learning_font_cache.py`로 두 backend와
캐시·메트릭·수명·실제 GL 픽셀 회귀를 검사한다. 별도 `check_learning_font_policy.py`는
정수 정책 oracle과 루트 렌더러의 키 충돌·할당 전 거절·실패 재시도를 다룬다.

새 항목을 넣을 공간이 없으면 캐시는 거절한다. 자동 페이지 clear나 개별 축출은 없다.
실패한 항목은 lookup에 확정하지 않으며, 이미 성공한 항목은 유지해 재시도에 사용한다.
GPU lookup clear만으로 shelf 공간을 반환하지 않는다. 폰트 교체 후 이미 만든 문단은
옛 스냅샷이며, 호출자가 새 문단으로 교체할 때까지 유지할 수 있다.

main은 페이지 갱신 지점에서 옛 CPU UV 큐가 없으며, 준비 실패 시 세션 오류로 끝낸다.
GL 컨텍스트·아틀라스는 이를 빌린 캐시와 텍스트보다 오래 살아야 한다. 모든 캐시는
렌더링 스레드 하나에서 사용한다. 스칼라별 수평 텍스트·LF를 지원하며 shaping,
fallback, 자동 줄바꿈은 추가 설계가 필요한 별도 기능이다.
