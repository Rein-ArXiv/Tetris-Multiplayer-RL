# 문서 허브

이 저장소의 문서는 **현재 코드**, **운영 절차**, **구현 과정**을 구분한다.
처음에는 아래 순서대로 읽고, 필요한 주제만 상세 문서에서 찾는 것이 가장 빠르다.

## 권장 읽기 순서

1. [`blog/README.md`](blog/README.md) — 의존성에 따른 구현 경로와 Part별 완료 기준
2. 모든 트랙의 공통 기반인 Part 0~1로 빌드 뼈대와 결정론 규칙 엔진을 구현.
   화면이 필요한 트랙(온라인 서비스·인게임 AI)만 Part 2~4를 더해 실행 가능한
   게임 클라이언트까지 만든다
3. 목적에 따라 온라인 서비스(클라이언트 기반 → Part 5→6→7→10→11→12), 학습 전용
   AI(Part 0→1→8 — 학습 코어는 `SimGame`만 의존하므로 Part 2~4 없이 진행),
   인게임 AI(클라이언트 기반 + Part 8→9) 경로 진행
   (Part 13은 순서대로 읽는 장이 아니라 완성 구조를 찾는 레퍼런스다)
4. [`README.md`](../README.md) — 완성된 프로젝트의 빠른 실행 요약
5. [`blog/part13-structure-and-build-reference.md`](blog/part13-structure-and-build-reference.md)
   — 완성 구조와 최종 모듈 계약을 찾아보는 레퍼런스

이 프로젝트의 주 학습 문서는 `docs/blog/part0~12`이고, `part13`은 그 위의 레퍼런스다.
루트 `README.md`, [`public-server-deployment.md`](public-server-deployment.md),
[`code-review.md`](code-review.md)는 Part를 대체하는 본문이 아니라 각각 완성 상태의
요약, 운영 절차, 품질 점검 기록이다.

## 목적별 경로

| 목적 | 시작 문서 | 이어서 볼 문서 |
|---|---|---|
| 현재 품질과 위험 확인 | [`code-review.md`](code-review.md) | [`blog/part12-hardening-and-release.md`](blog/part12-hardening-and-release.md)의 회귀·통합 검증 |
| 처음 빌드하고 실행 | [`README.md`](../README.md) | [`blog/README.md`](blog/README.md)의 "빌드 규약 — 먼저 읽을 것" |
| 이론과 코드를 누적 구현 | [`blog/README.md`](blog/README.md) | 클라이언트·온라인 서비스·학습·인게임 AI 중 목적별 경로 |
| 완성 구조 확인 · 고칠 곳 찾기 | [`blog/part13-structure-and-build-reference.md`](blog/part13-structure-and-build-reference.md) | [`code-review.md`](code-review.md)의 알려진 위험과 검증 근거 |
| 완성 코드를 빠르게 이해 | [`README.md`](../README.md)의 아키텍처 절 | 실제 코드 → [`blog/part13-structure-and-build-reference.md`](blog/part13-structure-and-build-reference.md) |
| 공개 서버 운영 | [`public-server-deployment.md`](public-server-deployment.md) | [`blog/part12-hardening-and-release.md`](blog/part12-hardening-and-release.md) |
| RL 모델 학습/export | [`python/train/README_colab.md`](../python/train/README_colab.md) | [`model/bots/README.md`](../model/bots/README.md) |

## 문서의 기준과 우선순위

설명이 충돌하면 다음 순서로 판단한다.

1. 테스트로 검증된 현재 코드와 `CMakeLists.txt`
2. 해당 Part가 명시한 **Part N 체크포인트**
3. 루트 `README.md`, `public-server-deployment.md` 같은 완성 상태 요약·운영 문서
4. `code-review.md` 같은 내부 점검 기록

Part 문서는 구현 순서와 이론 설명의 기준이다. 각 Part의 체크포인트 코드는 그
시점까지의 누적 상태이므로 뒤 Part가 확장한 최종 API와 다를 수 있다.
`blog/part13-structure-and-build-reference.md`는 현재 최종 상태의 상세 레퍼런스다.
내부 점검 기록은 이력 보존용이며 공개 사양으로 사용하지 않는다.

## 문서 유지 규칙

- 동작을 바꾸는 PR은 코드와 같은 PR에서 관련 문서를 갱신한다.
- 빌드 옵션과 실행 명령은 `CMakeLists.txt` 및 실제 CLI 파서와 대조한다.
- 네트워크 메시지 변경은 C++/Python framing parity 테스트와 해당 Part 문서(Part 6~7,
  Part 13 레퍼런스)를 함께 갱신한다.
- 결정론에 영향을 주는 변경은 골든 해시를 의도적으로 검토하고 갱신한다.
- 아직 구현하지 않은 기능은 현재 기능처럼 서술하지 않고 명시적으로 범위 밖에 둔다.
