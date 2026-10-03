# 101 워커 수명과 작업 완료

100-first-admission에 제한된 작업 그룹을 추가한다.
[HTML 강의](../../index.html#lesson-101)에서 예약·예외·캡처 소멸·대기 순서를 따라간다.

```sh
cmake -S docs/learn/checkpoints/101-worker-lifetime -B out/study101 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study101 -j3
ctest --test-dir out/study101 --output-on-failure
./out/study101/worker_contract
./out/study101/worker_probe
python3 scripts/check_learning_worker_lifetime.py
```

worker_probe는 실제 TCP 연결3개를 작업에 옮겨 각 첫 요청과 RoundPlay의 tick0을 처리한다.
초기 게임 설정은 드라이버가 공급한다. 실제 방·매칭·인증은 구현 범위가 아니다.
stop_accepting은 접수만 닫으며 wait는 task/capture 정리를 기다린다.
Windows 다중 구성에서는 --config Release와 CTest -C Release를 사용한다.
