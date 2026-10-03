# 입력의 형식·순서·보관량·진행 시간

PacedMatch가 서버 활성화 시각과 PacePolicy를 소유하고 AuthoritativeMatch 앞에서 입력의 끝 위치를 검사한다. 규칙 코어는 고정 논리 틱을 그대로 사용한다.

```sh
cmake -S docs/learn/checkpoints/144-input-pacing -B out/study-144 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-144 --target pacing_contract paced_probe -j2
out/study-144/pacing_contract
out/study-144/paced_probe
out/study-144/paced_probe --epoll
```

마지막 명령은 Linux용이다. Boost가 기본 경로 밖에 있으면 STUDY_BOOST_INCLUDE를 지정한다.

- net/tick_allowance.h: 정책 검증·정수 나노초의 허용량, 시간과 I/O 없음.
- net/paced_match.h: 신뢰된 서버 시각·경기별 고정 기준·전체 예산·허용량·시계 역행 검사 후 규칙 진행.
- net/paced_input_stream.h: 분할 프레임과 고정된 인증 주체, 수신 소유자가 시각 제공.
- tests/pacing_contract.cpp: 경계·마스크·입력 창·부분 변경 방지·시간 주입.
- tools/paced_probe.cpp: 실제 TCP poll/epoll 수신, 주입 시각으로 진행·조기 제출 거절.

프로브는 인증된 연결의 대역과 저장 포트 대역을 쓴다. 논리 시각은 서버 검사 코드에서 주입하며 클라이언트 프레임에 넣지 않는다. 소켓 대기 기한에는 실제 steady_clock을 사용한다. 허용량은 입력 위치를 제한하며 패킷 수·바이트·연결 수 제한은 별도다.

누적 AuthoritativeMatch와 해당 실습은 시간 제한을 분리한 기준 예제로 남는다. 새 보호 경로는 PacedMatch만 소유하고 내부 match를 수정 가능한 형태로 노출하지 않는다. 현재 서버는 첫 INPUT을 시간 기준으로 하고 전체 연속 기록을 보관한다. 학습 구현은 활성화 기준과 유한 창을 사용한다.

```sh
python3 scripts/check_learning_input_pacing.py --boost /path/to/boost/include
```
