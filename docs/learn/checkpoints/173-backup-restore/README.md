# 온라인 백업과 격리 복구

SERVICE 역할의 계정 서버를 실행하고 합성 계정의 키 교체 전후를 백업한다.
각 스냅샷을 새로운 경로로 복원해 신원·인벤토리와 허용/거절 자격을 비교한다.
운영 DB나 개인 키를 입력하지 않는다. 임시 디렉터리와 자식 프로세스만 소유한다.

```sh
cmake -S docs/learn/checkpoints/173-backup-restore/roles -B out/study173 -DSTUDY_ROLE=SERVICE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study173 --config Release --target study_account_db
python3 scripts/check_learning_backup_restore.py --binary out/study173/study_account_db
```

Visual Studio 생성기는 out/study173/Release/study_account_db.exe를 전달한다.
Python sqlite3와 C++ SERVICE 의존성이 필요하다. Windows 디렉터리 접근 권한은 ACL로 관리한다.

- 온라인 API가 완성한 복사본을 구조·외래 키 검사 후 게시한다.
- 새 목적지와 그 부속 파일이 존재하면 거절한다. 부모는 신뢰하는 단독 관리 경로다.
- hard link 게시로 경쟁자의 기존 파일을 덮어쓰지 않는다. 미지원 저장소는 실패한다.
- 기한은 콜백 사이의 협력적 검사이며 임의 I/O를 강제 중단하지 않는다.
- 게시 후 동기화 실패는 완성 파일이 남는 경우도 있다. 예외를 롤백으로 해석하지 않는다.
- 과거 백업은 폐기했던 키를 다시 허용할 수 있다. 파일 검사는 최신 보안 상태의 인증이 아니다.
- README 외 누적 파일을 보존하고 operations의 두 파일을 추가한다. 스냅샷 코드는 작성 시점 기준이며 제품 도구는 별도 레퍼런스로 바뀔 수 있다.
