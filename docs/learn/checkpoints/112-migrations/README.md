# 112 — 스키마 이관

111 누적 저장소에 v2 선택 아이콘과 기본 소유 관계를 추가한다.
구 플레이어 ID·이름·다른 소유·경기 행을 보존하며 반복 시작은 선택을 초기화하지 않는다.

```sh
cmake -S docs/learn/checkpoints/112-migrations -B out/study-112 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-112 --target schema_upgrade study_meta_db migration_contract -j2
out/study-112/schema_upgrade out/study-112/lesson.db
out/study-112/schema_upgrade out/study-112/lesson.db
```

HTTP 연결:
```sh
out/study-112/study_meta_db 18081 out/study-112/lesson.db
```

실패 실험은 저장소 루트에서 `python3 scripts/check_learning_migrations.py`.
검사 스크립트가 임시 DB·프로세스를 소유하고 정리한다. migration_contract의 인자로 주는 파일은 존재하지 않아야 한다.
schema_upgrade는 없는 파일을 새로 만든다. 실제 계정 DB와 혼용하지 않는다.
구 학습 DB는 서비스를 멈추고 일관된 백업 사본에서 이관한다.
`--pause-after-column`은 열 추가 뒤 Enter를 기다리는 로컬 중단 실험용이다. 입력 종료는 롤백 오류 경로, 정상 Enter는 계속 진행이다.
Windows 다중 구성에서는 --config와 실행 파일의 구성 폴더를 맞춘다. 분리한 체크포인트는 STUDY_VENDOR_DIR를 지정한다.
