# 복구 저널 체크포인트

누적 계정 서비스의 원자 교체 요청을 전송 전에 저장하고 프로세스 재시작 뒤 같은 요청으로 재개한다.

## 빌드

```sh
cmake -S docs/learn/checkpoints/142-recovery-journal -B out/study-142 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-142 --target study_account_db account_journal journal_contract -j2
out/study-142/study_account_db 0 out/study-142/service.db
```

서버의 PORT를 확인한 뒤 다른 터미널에서 실행한다. Boost가 기본 경로 밖에 있으면 STUDY_BOOST_INCLUDE를 지정한다.

```sh
out/study-142/account_journal PORT out/study-142/profile connect
out/study-142/account_journal PORT out/study-142/profile backup
out/study-142/account_journal PORT out/study-142/profile rotate crash-recovery
out/study-142/account_journal PORT out/study-142/profile connect
```

작업은 connect, backup, rotate, recover, resume이다. 실패 실험 인자는 crash-pending/crash-recovery/crash-account, fail-account/fail-clear, uncertain-pending/uncertain-account/uncertain-clear이다. 강제 종료는70, 일반blocked는4, 불확실은8, 확정거절은9, 서버성공·로컬미완료는10, 완료는0이다. 출력에 비밀을 포함하지 않는다.

## 책임과 경계

- journal_wire.h: 엄격 파싱·origin·필드 간 의미 검증.
- journal_store.h: 부트스트랩과 같은 잠금·비공개 원자 저장·후보 생성·idle 표시.
- journal_http.h: loopback HTTP와 성공/거절/불확실 분류.
- journal_flow.h: 동일 요청 재개 및 복구→접근→완료 순서.
- LocalAccountStore: 활성 저널 또는 복구 자료를 새 프로필로 오인하지 않는다.

완료 저널은 origin과idle만 남긴다. 현재 게임의 pending 삭제와 표현은 달라도 미완료 의도를 먼저 보관하고 성공 저장 뒤 정리하는 계약은 같다. 애플리케이션 소유 폴더·협조적인 작성자와 OS 동기화 계약을 전제로 한다. 파일 권한은 암호화나 장치 안전 삭제가 아니다.

```sh
python3 scripts/check_learning_recovery_journal.py --boost /path/to/boost/include
```
