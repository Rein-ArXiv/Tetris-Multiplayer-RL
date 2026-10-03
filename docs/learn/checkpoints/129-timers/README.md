# 129 — 타이머 힙과 실제 대기 루프

128-iocp 누적 파일을 보존하고 net/deadline_wait.h·net/timer_heap.h,
tests/timer_contract.cpp·tools/timer_probe.cpp를 추가한다.

```sh
cmake -S docs/learn/checkpoints/129-timers -B out/study-129 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-129 --target timer_contract timer_probe -j2
out/study-129/timer_probe
ctest --test-dir out/study-129 -R "^(timer_contract|timer_probe|epoll_timer_probe)$" --output-on-failure
```

Windows 다중 구성은 --config Release/-C Release와 Release/timer_probe.exe 경로를 쓴다.
기본 poll/WSAPoll, Linux --epoll. 게임은 SDL 구성 tetris 타깃.
타이머는 단일 루프 소유이며 한 Reactor 인스턴스의 등록 ID에 만기 하나를 둔다.
만기는 steady_clock 절대 시각, 출력은 append·일회성 배치, cancel은 이미 나온 배치를 철회하지 않는다.
미래 밀리초 올림과 사전 포화, seq 소진 거절, 같은 만기 설정 순서를 보장한다.
지연 무효화 힙에 별도 크기 상한/압축은 없다. 대상 소유권은 CallbackLoop에 있다.

검사: python3 scripts/check_learning_timers.py
실제/학습 헤더의 할당 실패·시간 경계: python3 scripts/check_learning_timer_root.py
STUDY_MINGW_CXX가 있으면 Windows 실행 파일을 교차 빌드한다. 실행 증거와는 구분한다.
독립 복사 시 STUDY_VENDOR_DIR 지정. 공개 배포 상태와 별개인 로컬 기준 코드다.
