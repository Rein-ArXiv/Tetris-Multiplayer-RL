# 126 — Reactor 계약과 콜백 수명

125-io-models 누적 파일을 보존하고 등록표·poll/WSAPoll 백엔드·콜백 루프를 추가한다.

```sh
cmake -S docs/learn/checkpoints/126-reactor-contract -B out/study-126 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-126 --target reactor_contract reactor_probe -j2
out/study-126/reactor_probe
ctest --test-dir out/study-126 -R "^(reactor_contract|reactor_probe)$" --output-on-failure
```

registrations.h: 소켓을 빌리는16칸 등록표. 재사용하지 않는64비트ID, 중복/무효/알수없는관심 거절.
ID공간 고갈시0으로 영구 거절. interest0은 등록 유지·OS관찰 제외. 이미 받은 사건은 남는다.
study_reactor.h/poll_reactor.cpp: 다중 poll/WSAPoll, 요청 대기0~1000ms, 사건값/깨우기 구분.
깨우기는 내부 논블로킹TCP채널. 업무는 별도 동기화된 큐/플래그에 먼저 공개한다.
callback_loop.h: Node가Socket/Handler소유. remove후목록해제,현재콜백은지역shared_ptr로반환까지유지.
콜백중attach/change/close허용,observe/dispatch재진입거절,루프파괴/원본배치수정금지.
핸들러예외는전파하고Guard는플래그만복구. 상태전이/I/O롤백은하지않는다.
한스레드소유,wake만일반다른스레드허용.파괴전모든wake호출자회수.Runtime은루프보다오래유지.
콜백이참조캡처한외부값의수명은호출자가지킨다.등록ID범위는Reactor인스턴스내다.

reactor_probe: 실제11바이트프레임수신→자기등록제거→콜백반환→연결사건없는wake.
reactor_contract: fd재등록·용량/마스크/고갈·pause·A가B와자기제거·예외·재진입·깨우기.
검사: python3 scripts/check_learning_reactor_contract.py
실제epoll깨우기회귀: python3 scripts/check_learning_reactor_wake.py

게임은 별도 SDL 구성 tetris타깃. C++17/CMake와 누적 의존성 필요.
독립 복사는 STUDY_VENDOR_DIR 지정. 다중구성은 --config Release/-C Release/Release 경로.
Windows/macOS 네이티브 실행과 Linux 실행 증거를 구분한다.
