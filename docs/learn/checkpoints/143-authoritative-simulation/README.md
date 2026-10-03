# 서버가 재현하는 경기

기존 InputAuthority와 Round/Duel을 연결한다. 서버 seed·고정 계정 주체·같은 틱의 입력 쌍으로 규칙을 진행하며, 완결된 경기만 MatchRecord를 만든다.

## 빌드와 실행

```sh
cmake -S docs/learn/checkpoints/143-authoritative-simulation -B out/study-143 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-143 --target authoritative_contract authoritative_probe -j2
out/study-143/authoritative_contract
out/study-143/authoritative_probe
out/study-143/authoritative_probe --epoll
```

마지막 명령은 Linux용이다. Boost가 기본 경로 밖에 있으면 STUDY_BOOST_INCLUDE를 지정한다.

## 책임

- net/authoritative_match.h: 같은 규칙 실행·첫 종료·유한 틱 예산·완결 기록.
- net/match_input_stream.h: 연결에서 고정한 주체와 바이트 스트림.
- tests/authoritative_contract.cpp: 직접 규칙 실행과 묶음/도착 순서·종료·ID 폭·제출 경계 비교.
- tools/authoritative_probe.cpp: 실제 TCP와 poll/epoll·분할 수신·누락 대기·서버 판정.

프로브 actor는 인증 완료 fixture이며 저장 포트는 검사 대역이다. 실제 계정 인증이나 DB 지급을 실행한 것으로 해석하지 않는다. 공개 서비스에는 실제 시간 대비 입력 예산과 연결 수명 정책이 더 필요하다.

현재 서버 RankedGame은 전체 입력 기록을 보관하고 종료 후 잘못된 입력으로 무효화할 수 있다. 학습 구현은 유한 창과 완결 시 입력 종료를 사용한다. 공유 핵심은 입력 공통 틱·규칙 순서·실제 종료에서 파생하는 결과다.

```sh
python3 scripts/check_learning_authoritative_simulation.py --boost /path/to/boost/include
```
