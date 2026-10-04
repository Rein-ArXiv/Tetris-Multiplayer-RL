# 릴리스 검사 기록의 범위

규칙과 실제 HTTP 복구를 실행하고 후보 지문·환경·범위를 포함한 기록을 만든다.
GUI/부하를 요구 목록에 두되 미실행 상태로 보존하여 전체 ready는 false가 된다.

```sh
python3 scripts/check_learning_release_evidence.py
```

검사기는 RULES/SERVICE 역할을 별도 디렉터리에 빌드한다.
보고서는 out/learning-checkpoints/174-release-evidence-check/report.json에 생성한다.
GUI·공개 서버 부하를 자동 실행하지 않는다. 합성 계정과 임시 DB만 사용한다.

release_evidence.py는 선언을 분류할 뿐 실행 사실을 암호학적으로 인증하지 않는다.
소스와 실행 파일 지문에는 공유 라이브러리·컴파일러·전체 배포 자산의 출처가 포함되지 않는다.
배포 후보는 별도 불변 경로로 고정하고 실제 의존성/환경 기록과 함께 관리해야 한다.

README 외 누적 파일을 보존하고 operations에 평가기·수집기·계약 검사를 추가한다.
