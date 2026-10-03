# 공급 경로와 정리 경로

Voice의 PCM 대여와 Player의 주소 안정성은 유지한다. 제어 API는 메인 스레드 순차 호출이다.
SDL 장치 잠금은 콜백과 공유 상태 변경을 조정한다. 잠금 없는 구조나 하드 실시간 보장은 아니다.

replace/unload는 바깥 스코프의 빈 optional<Pcm16> retired를 사용한다.
잠금 안에서 Voice::stop → retired.swap(clip_) → 새후보emplace(교체시에만).
잠금이 풀린 다음 retired 소멸자가 이전 PCM을 반환한다. Pcm16 noexcept 이동은 샘플을 복사하지 않는다.
형식 거절은 소유권 변경 전에 일어나 기존 음원과 후보를 보존한다.

콜백은 len 바이트를 요청받고 Voice가 완전한 프레임을 복사하며 나머지는0으로 둔다.
기간 frames/rate는 버퍼에 담긴 음원 길이이며 end-to-end 지연이나 호출 간격 상한이 아니다.
콜백에 할당·해제·파일읽기·디코딩·출력을 추가하지 않는다. 암시적 연산 경로도 점검한다.

진단 대역은 실제 SDL 장치를 계속 pause하여 수동 콜백과 SDL 작업 스레드의 중첩을 막는다.
일반 C++ new/delete 카운터와 잠금 깊이는 해당 스레드에서만 추적한다. 출력 판정은콜백밖.
실제SDL스케줄링은별도playback_contract가연결하고,실시간최악지연이나Cmalloc까지증명하지않는다.
