# 판정 사유와 결과 통지

서버 판정·저장 확인·송신 배수·클라이언트 수신을 서로 다른 상태로 다룬다. NoticeReason은 ChannelView를 사용자 설명으로 분류하고, ResultNotice는 경기 키와 사유를 직렬화한다. FinalOutput은 누적 PendingSend를 사용해 기존 바이트 뒤에 통지를 넣고 유한 기한으로 배수한다.

```sh
cmake -S docs/learn/checkpoints/146-result-notices -B out/study-146 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-146 --target result_notice_contract notice_probe -j2
out/study-146/result_notice_contract
out/study-146/notice_probe
out/study-146/notice_probe --epoll
```

마지막 명령은 Linux용이다. Boost가 기본 경로 밖에 있으면 STUDY_BOOST_INCLUDE를 지정한다. 프로브는 실제 TCP·작게 나눈 write/read·FIFO·완전한 통지 뒤 EOF를 확인한다. 서버 저장 상태는 이 프로브의 fixture이고, 실제 저장 계약은 누적 relay_owner_probe와 현재 relay/meta 통합 검사에서 별도로 다룬다.

- notice_reason.h: 사실에 맞는 상태 분류와 사용자 문구.
- result_notice.h: 안정된 학습 wire·미래 사유 Unknown·기대 경기 키의 수신 뷰. 현재 MATCH_RESULT wire와 다르다.
- final_output.h: 같은 FIFO·부분 송신 접미사·쓰기 관심·유한 배수·예산 반환.
- result_notice_contract.cpp: 상태/잘못된 조합·모든 사유 바이트·길이/키·FIFO/기한/예산.
- notice_probe.cpp: 실제 소켓·poll/epoll·통지·EOF.

FinalOutput의 drained는 커널이 바이트를 받았다는 뜻이다. 원격 화면 표시나 DB 저장을 증명하지 않는다. 타이머는 쓰기 이벤트가 없어도 expire를 호출해야 하며 terminal 상태에서 연결 소유자가 실제 소켓을 닫는다. 연결 하나의 모든 송신은 같은 큐를 거친다. 공유 예산 카운터는 큐보다 오래 살아야 한다.

```sh
python3 scripts/check_learning_result_notices.py --boost /path/to/boost/include
```

Linux의 현재 서버 부분 송신 회귀는 tests/learning/result_send_fault.c를 테스트 릴레이에만 주입한다. 운영 실행에 이 라이브러리를 설정하지 않는다. 현재 코드의 결과 큐·송신 실패 재진입·상대 이탈 후 유한 배수 경계는 python/tests/test_match_summary_crosscheck.py의 오류 주입 검사와 연결된다.
