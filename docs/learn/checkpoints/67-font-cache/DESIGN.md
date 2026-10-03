# 67 설계 결정

이 파일은 설계 기록이며 학습 본문은 `lessons/067.json`에서 읽는다.

- 크기 계획: 논리/화면 단위 분리, 1/8 양자화 뒤 정수 굽기 높이로 키를 결정한다.
  높이는 ascent−descent 메트릭 요청이며 em 또는 실제 bitmap row 수가 아니다.
- Font: 기존 논리 API 1~128 유지, 수평 메트릭 전용 API와 정수 device 1~2048 API 추가.
  공통 래스터 구현·실제 상자 int64 차이·축 1024 제한 후 할당.
- CPU 캐시: Font를 독점 소유하고 변경 경로를 load_trusted 하나로 제한한다.
  실패는 옛 상태 유지, 성공은 lookup 삭제. shared_ptr<const CachedGlyph> 스냅샷은
  외부 소유 중 생존한다. 캐시 주소만으로 폰트 정체성을 판단하지 않는다.
- GPU 캐시: shared_ptr를 보유해 할당 주소 재사용을 막고, atlas revision 변경 시
  lookup을 버린다. 실패한 Region을 정상 항목으로 확정하지 않는다.
- 수용량: 각 lookup 64, 실제 atlas 512×512. 숨은 자동 clear/축출 없음.
  lookup clear는 shelf 회수가 아니다. 성공한 부분 준비는 다음 시도에서 재사용한다.
- main: 실제 viewport/logical 배율, 정수 device 높이 변경 시 두 라벨을 한 갱신 단위로
  재준비한다. old draw는 이미 제출한 상태이며 같은 GL 컨텍스트의 순서를 사용한다.
  준비 실패 시 세션 오류; 후보 페이지를 통한 무중단 전환은 이 단계의 범위 밖이다.
- 검수: `check_learning_font_cache.py`, 1단계 root/정책 회귀는 `check_learning_font_policy.py`.
  구체 실행 결과는 REVIEW_LOG.md에 기록한다. 설명/메타데이터에 검수 환경 보고를 섞지 않는다.
