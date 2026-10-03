# 선택 기능의 상태와 실패 정책

BasicSession이 정책을, Device가 OS 자원과 PCM을 소유한다. 컴파일 시간 템플릿 인자로
장치 구현과 지원 여부를 고른다. 지원하지 않는 가지는 if constexpr로 버려지므로
NoAudioDevice에 실제 장치 메서드가 필요하지 않다. 런타임 가상 함수는 추가하지 않는다.

unprepared에서만 prepare를 시도한다. 성공은 ready, 선택 안 된 빌드는 disabled,
준비 실패는 부분 Device를 닫고 unavailable이다. 이후 prepare는 상태만 반환한다.
ready는 준비 완료를 뜻하며 이후 실제 장치 건강이나 청취 성공을 보증하지 않는다.

유효한 send는 ready에서만 play를 한 번 호출한다. 거절되면 failed를 반환하되
다음 별개 사건은 시도할 수 있다. invalid_kind는 준비 여부보다 먼저 거절한다.
stop은 ready Device의 재생을 중단하고 준비된 PCM·정책 상태는 유지한다.

prepare의 CueFactory는 nullopt 또는 예외를 낼 수 있다. bad_alloc을 구별하고 다른
예외는 unexpected로 보고한다. Device 기본 생성/play/stop/close는 비예외 계약이다.
catch는 C++ 예외 경계이며 다른 스레드의 오류나 메모리 오염을 복구하지 않는다.

seen은 수명 중 이미 관찰한 실패 종류, pending은 아직 보고하지 않은 종류다.
읽기 후 pending만 비우므로 재발해도 중복 보고하지 않는다. 저장소/보고 상한은
Failure::count=7이며 시간창 기반 호출 제한·오류 이력·발생 순서 보존이 아니다.

Batch는 take가 소비 위치를 먼저 진행한다. drain은 성공/건너뜀/실패를 모두 소비하며
되돌리거나 재전송하지 않는다. 반환 카운트는 한 Batch 최대24로 제한된다.
잘못된 FrameReport의 용량 위반은 Batch::from이 거절하고 main은 오류로 종료한다.
선택 기능의 장치 실패와 내부 규칙/메모리 경계 계약 위반을 같은 복구로 취급하지 않는다.

로그는 main의 report_audio_notices가 고정 문자열을 stderr로 쓴다. worker에는 로그를
넣지 않는다. 동일 외부 입력과 dt에서의 규칙 독립성과 실제 CPU/I/O 지연은 구별한다.
