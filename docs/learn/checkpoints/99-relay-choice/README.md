# 99 직접 경로와 중계 경로

98-end-negotiation의 규칙·연결 계층에 경로 실험과 비용 모델을 추가한다.
[HTML 강의](../../index.html#lesson-99)를 보며 실제 바이트 전달과 가정 기반 계산을 구별한다.

```sh
cmake -S docs/learn/checkpoints/99-relay-choice -B out/study99 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study99 -j3
ctest --test-dir out/study99 --output-on-failure
./out/study99/topology_probe
./out/study99/relay_cost_contract
python3 scripts/check_learning_relay_choice.py
```

loopback 실험은 직접1개/중계2개의 실제 TCP 연결에서 RoundPlay 입력을 전달한다.
read cap1/2/7/16 모두 정규 상태 바이트가 같고 미래 라운드는 수신 endpoint에서 거절된다.
중계자는 고정 실험 길이의 바이트만 복사한다. 인증·매칭·운영용 대기 제한을 갖춘 서버가 아니다.
비용 모델의 us/B/s 값은 입력한 가정이며 WAN 실측값이나 용량 보증이 아니다.
Windows 다중 구성은 --config Release와 CTest -C Release를 사용한다.
