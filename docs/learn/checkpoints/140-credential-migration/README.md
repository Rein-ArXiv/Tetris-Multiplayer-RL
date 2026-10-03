# 140 — 토큰 이관: 평문을 지우는 전체 과정

누적 계정 서비스는 처음부터 해시를 보관한다. 이 도구는 별도의 옛 설치본 실험
파일을 만들어 실제 서버와 같은 이관 경계를 관찰한다. 실험용 players의 ID·BP를
보존하고 원문 열을 목적 해시로 바꾼다. 실제 운영 DB나 누적 게임 DB에 실행하지 않는다.

```sh
cmake -S docs/learn/checkpoints/140-credential-migration -B out/study-140 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-140 --target credential_migration credential_migration_contract -j2
out/study-140/credential_migration init out/study-140/experiment.db
out/study-140/credential_migration crash-commit out/study-140/experiment.db
out/study-140/credential_migration migrate out/study-140/experiment.db
python3 scripts/check_learning_credential_migration.py --boost /path/to/boost/include
```

init은 새 경로만, 다른 모드는 기존 실험 파일만 받는다. crash-row/crash-commit/
crash-vacuum은 std::_Exit로 소멸자 없이 종료한다. migrate를 별도 명령으로 재실행한다.
계약 검사를 직접 실행할 때는 아직 존재하지 않는 실험 디렉터리 경로를 인수로 준다.
자동 검사 스크립트는 임시 디렉터리를 사용한다. --snippets-only는 자료 대응만 검사한다.

C++17·OpenSSL Crypto·누적 SQLite 타깃을 사용한다. 누적 WSS 구성의 Boost 경로는
STUDY_BOOST_INCLUDE로 지정한다. Windows 다중 구성은 --config Release와 Release/
실행 파일 경로를 사용한다. 새 도구에는 GUI·외부 서버가 필요 없다.

논리 이관은 열 변경·모든 행·hash marker를 하나의 BEGIN IMMEDIATE에 묶는다.
SELECT를 닫고 고정 ID 다음 행을 다시 찾으며 ID+1을 계산하지 않는다. 물리 정리는
커밋 후 checkpoint/VACUUM/checkpoint 순서로 수행하고 마지막에 scrub marker를 넣는다.
옛 reader가 정리를 막으면 시작을 거절하고 reader 해제 뒤 재시도한다.

공개 fixture 바이트만 사용하고 원문을 출력하지 않는다. 변환 후 DB/WAL에서 fixture가
없는지와 외부 사본에는 남는지를 함께 검사한다. 프로세스 중단 실험은 실제 전원 장애나
SSD·스냅샷·모든 백업의 안전 삭제를 증명하지 않는다. 일반적인 임의 DB 복구 도구가 아니다.
