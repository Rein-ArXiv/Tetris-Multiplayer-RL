# 시스템 경계의 종합 복습

새 런타임 기능을 추가하지 않고 누적 구현을 역할별로 다시 실행한다.
입력·규칙·표현·통신·판정·저장·복구의 계약을 함께 읽는다.

```sh
python3 scripts/check_learning_system_review.py
```

RULES, ARCHITECTURE, CONTENT, SHUTDOWN, SERVICE를 독립 빌드 디렉터리에 구성한다.
등록된 CTest가 없는 경로는 성공으로 간주하지 않으며 SERVICE는 실제 임시 계정 복구를 실행한다.
GUI 픽셀·실제 장기 모델 성능·공개 서버 용량은 이 검사 묶음의 범위 밖이다.
학습용 SERVICE는 루프백 전용이며 공개 서버 배포물로 사용하지 않는다.
README 외 누적 파일은 그대로 보존한다. 현재 제품의 소스 레퍼런스와 이 기준 구현은 구분한다.
