# 화면 표시와 서비스 수명

- 121 누적 계정·보관함·게임을 유지하며 실제 tetris 메뉴에 계정 패널을 추가한다.
- View는 비밀 없는 값 복사본이다. Controller만 UI 스레드에서 쓰고 렌더러는 같은 스레드에서 읽는다.
- BootstrapState/Result를 bootstrap_types로 추출한다. 서비스 알고리즘은 AccountBootstrap을 재사용한다.
- AccountTask는 API/폴더/Bootstrap/Store를 소유한다. 목적지는 실행 수명 동안 고정이다.
- Store 생성은 worker에서 수행한다. 획득한 폴더 잠금은 Task 수명 동안 유지한다. 획득 실패만 재시도한다.
- Controller는 unique_ptr Worker와 단일 async future를 소유한다. 작업 중 Worker를 다른 스레드가 읽지 않는다.
- request는 중복 작업을 거절한다. poll은 valid+ready만 get하고 결과를 한 번 반영한다.
- View에서 profile은 저장 확인 후 인증된 online만 가진다. unsaved가 우선이며 busy에서는 이전 profile을 숨긴다.
- 파괴는 future부터 하여 작업 회수 후 Worker를 해제한다. 종료는 기다릴 수 있으며 취소 기능은 없다.
- 패널 닫기는 표시 변경뿐이다. main은 패널이 숨겨져 있어도 poll한다. 미저장 키와 결과를 버리지 않는다.
- 실행 중 계정/서버 전환은 없다. 추가하려면 결과 문맥 및 미완료 키/작업 정책을 함께 설계해야 한다.
- 입력 의도는 한 번만 처리하고 panel Escape/Space가 Application으로 누수되지 않게 소비한다.
- renderer는 읽기 전용 View와 hover로 그린다. 같은 그림을 다시 그려도 저장/HTTP가 실행되지 않는다.
- 첫 연결은 명시적 입력이며 게임 시작 시 네트워크가 없다. 로컬 게임은 계정 연결과 독립이다.
- 미저장 키는 메모리만 보존한다. 종료/충돌 복구 저널·계정 교체·공개 TLS/서버 배포는 이 패널의 범위 밖이다.
