# 제어 계약과 구현 경계

Session의 같은 제어 호출에 SDL Player 또는 XAudioPlayer를 빌드 때 연결한다.
PCM과 사운드 사건은 공통이고, SDL 사용자 믹서와 XAudio OS 그래프의 DSP는 별개다.
state/cursor_frames는 SDL 진단 API이며 이번 공통 제어 계약에 포함하지 않는다.

XAudioPlayer는 9 PCM 자리와 9 Source 자리, Mastering/엔진을 소유한다.
모든 제어와 소멸은 초기화한 메인 스레드에서 직렬 실행한다. COM S_OK/S_FALSE는
이번 호출의 성공 한 번을 같은 스레드의 CoUninitialize와 대응시킨다.
RPC_E_CHANGED_MODE는 호스트 모델을 유지하고 엔진 생성 결과를 따로 검사한다.

open은 형식을 먼저 검사한 뒤 COM→엔진→Mastering을 얻는다.
close는 모든 Source→PCM→Mastering→엔진→이번 COM 참조 순서로 정리하며 멱등이다.
rate/channels를 open에서 고정해 끝난 Source를 재사용할 수 있다.
WAVEFORMATEX의 blockAlign은 프레임 바이트, avgBytesPerSec는 초당 바이트다.

play는 빈/큐가 끝난 보이스를 우선 선택한다. 모두 바쁘면 성공한 시작 순서상
가장 오래된 보이스를 DestroyVoice로 선점한다. 생성·제출·시작을 각각 검사하고,
실패 시 보이스를 비우며 성공한 경우에만 owner와 순서를 기록한다.
포화 후 생성 실패는 과거 재생을 복원하지 않는다. PCM 소유는 유지된다.

XAUDIO2_BUFFER 설명서는 지역 변수지만 pAudioData는 소유 PCM을 가리킨다.
replace/unload는 같은 owner의 모든 Source를 파괴한 뒤 PCM을 변경한다.
DestroyVoice가 OS 읽기 종료 경계이며 대기할 수 있다. 사용자 콜백을 등록하지
않아도 워커의 PCM 읽기와 소유자 해제 간 동기화 책임은 존재한다.

STUDY_PLATFORM과 STUDY_AUDIO를 분리해 SDL 창 + XAudio2 소리를 연결한다.
NOMINMAX는 Windows 헤더의 함수형 min/max 매크로 충돌을 막는다.
대역은 수명/실패 순서를 검사하고 실제 SDK ABI나 장치 동작은 검증하지 않는다.

SDL_MAIN_HANDLED를 SDL 소비자에 전달해 main 재정의를 막고 플랫폼/Player 및
직접 SDL을 초기화하는 진단 main에서 SDL_SetMainReady를 호출한다.
