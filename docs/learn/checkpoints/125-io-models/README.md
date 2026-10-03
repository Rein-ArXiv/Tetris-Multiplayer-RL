# 125 — 준비성 관찰과 제출 수신의 완료

124-thread-measurement 전체를 보존하고 read_hint·PostedReceiver·io_models_probe를 추가한다.

```sh
cmake -S docs/learn/checkpoints/125-io-models -B out/study-125 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-125 --target io_models_probe -j2
out/study-125/io_models_probe
ctest --test-dir out/study-125 -R '^io_models_probe$' --output-on-failure
```

read_hint는 한 connected socket의 poll/WSAPoll 한 번, timeout0~1000ms.
데이터를 소비하지 않고 모드를 바꾸지 않으며 소켓을 소유하지 않는다.
PostedReceiver는 소켓을 소유한 작업자 기반 완료 어댑터다. 네이티브 IOCP/io_uring은 아니다.
post id/최대1~16바이트/1~5000ms, 한 미회수 future. 결과가 준비돼도 take 전 재제출 거절.
내부20ms readiness wait→nonblocking read, 결과는 count/state/id와 바이트 배열을 소유한다.
동작 중 객체의 주소 이동 금지. 취소 요청 후 완료 회수, 소멸 시 취소+wait 후 자원 정리.

실험은 hint 비소비·옛힌트 뒤 WouldBlock·헤더 부분완료·누적프레임·EOF·취소/timeout·
완료 후 취소·미회수 재제출 거절·파괴 대기·잘린 프레임을 실제 loopback으로 검사한다.
고정 지연 성능을 검사하지 않는다. 네이티브 Windows 실행 증거와 API 대역 검사는 별개다.
검사: `python3 scripts/check_learning_io_models.py`.
실제 IOCP 소멸자 회귀: `python3 scripts/check_learning_iocp_lifetime.py`.

게임은 SDL 백엔드 별도 폴더의 tetris 타깃. 계정/랭킹/측정 옵션은 누적 구현을 유지한다.
C++17/CMake 및 누적 의존성 필요. 독립 복사는 STUDY_VENDOR_DIR 지정.
다중구성: --config Release / -C Release / Release 실행 하위 폴더를 맞춘다.
