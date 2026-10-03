# 95 송신 접수 예산과 백프레셔

94-heartbeat에 대기+활성 송신의 wire 비용 추적을 추가한다.
[HTML 강의](../../index.html#lesson-95)에서 고수위/저수위와 비용 반환을 읽는다.

```sh
cmake -S docs/learn/checkpoints/95-backpressure -B out/study95 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study95 -j3
ctest --test-dir out/study95 --output-on-failure
python3 scripts/check_learning_backpressure.py
```

FlowQueue<8,140,105,70>은 pop에서 비용을 반환하지 않는다. 일반 프레임의 모든 바이트가
전송 계층에 수락되면 complete한다. full은 생산자가 같은 레코드를 재시도하도록 전달한다.
flow_probe PORT COUNT는 읽기/EOF 상대를 필요로 하는 한 방향 실험이다. 검수 스크립트가
500개 번호를 확인하는 실제 상대를 제공한다. 생산 접수 지연250ms·전체10초 정책을 사용한다.
수신은 기존8칸 초과 실패 정책, 제어 프레임은 별도 유한 슬롯이다. 전체 RSS 제한이 아니다.
Windows 다중 구성은 --config Release / CTest -C Release를 사용한다.
