# 봇전의 공용 BP 검증

직전 체크포인트에 공식 봇 입력 재현, 서버 챌린지, 공용 지갑 지급을 연결한다.
`simulation/bot_replay.h`는 누적 Round/Duel을 사용한다. 학습 상대는 일정한 간격마다
하드 드롭을 요청하며 현재 게임의 Controller/heuristic/ONNX 정책과는 다르다.
서버가 선택한 정책과 시드로 첫 종료까지 사람 입력을 재현한다는 경계를 비교한다.

- `meta/bot_challenges.h`: 계정에 묶인 발급, 보관 상한, 만료, 정수 진행 허용량,
  try-lock 검증기, 검증 후 교체 여부 재조회, 영수증 우선 재시도.
- `meta/bot_reward_policy.h`: 잔액 대신 서버 UTC일의 영수증 합계로 남은 지급량 계산.
- `meta/bot_settlement.h`: 티켓 영수증과 기존 wallets 지급을 한 트랜잭션으로 저장.
- `meta/migrations.h`: bot_rewards를 추가하는 스키마 이행. 과거 데이터 보존.

## 실행

```sh
cmake -S docs/learn/checkpoints/147-bot-rewards -B out/study-147 \
  -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-147 --target bot_reward_contract bot_reward_probe -j2
out/study-147/bot_reward_contract out/study-147/contracts-new.db
out/study-147/bot_reward_probe out/study-147/probe.db
out/study-147/bot_reward_probe out/study-147/probe.db
python3 scripts/check_learning_bot_rewards.py --boost /path/to/boost/include
```

계약 검사는 새 DB 경로를 받는다. 프로브는 같은 경로로 다시 실행하여 재지급이 없는지
확인한다. Boost가 기본 경로 밖에 있으면 구성에 STUDY_BOOST_INCLUDE를 지정한다.
검사 도구는 임시 DB를 사용하며 사용자 DB를 삭제하지 않는다.

## 연결과 수명

SqliteResults는 BotChallenges보다 오래 살아야 한다. 서비스 파괴 전에 호출 스레드를
모두 종료한다. 인증된 actor·공식 카탈로그·CSPRNG 티켓·단조 시각·UTC일은 서버 어댑터의
책임이다. 프로브의 고정 입력은 실습 fixture이며 공개 HTTP 인증을 대체하지 않는다.
`claim`의 Verify 템플릿 포트는 신뢰된 로컬 검증기/테스트 대역이다. 요청자가 전달하는
winner 플래그나 콜백을 넣는 API가 아니다. 기본 claim은 실제 규칙 재현을 호출한다.

발급 기록은 프로세스 메모리, 지급 영수증은 DB에 남는다. 같은 DB의 서로 다른 연결은
SQLite 쓰기 트랜잭션으로 조정한다. 여러 서비스의 진행 표를 공유하는 배포는 별도 설계가
필요하다. 새 챌린지를 발급하는 포트는 저장/할당 오류를 예외로 전달하므로 어댑터는 이를
발급 실패로 응답해야 한다. claim은 로컬 검증/저장 예외를 unconfirmed로 돌려 재시도한다.

지급 정책의 0은 정상적인 기록이다. 하루 상한을 사용한 뒤 같은 영수증을 다음 날
다시 청구해도 지급량은 바뀌지 않는다. 보상 계산은 사람 조작 여부를 판별하지 않는다.
실습과 실제 서버의 wire·상대 알고리즘·시각 경계·정책값 차이는 강의에서 구별한다.
