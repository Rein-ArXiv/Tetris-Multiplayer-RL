# 문서 허브

> 캐릭터별 모델·속도·일러스트와 서버 검증 BP는 [봇과 Colab 안내](bots-and-colab.md)를 먼저 보세요.

현재 인수인계·SOLID·보안·사용자 오인 수준은 [2026-09-19 품질 검토](architecture-quality-review.md)를 참고한다.

이 저장소의 문서는 **현재 코드**, **운영 절차**, **구현 과정**을 구분한다.
처음에는 아래 순서대로 읽고, 필요한 주제만 상세 문서에서 찾는 것이 가장 빠르다.

## 지금 다시 빌드하고 출시를 준비한다면

1. [처음 켜는 순서](start-here.md) — 설치, 빌드, 서버 시작, 오류 확인.
2. [아이콘·폰트·블록을 고칠 곳](customization.md) — 설정 변경과 규칙 변경 구분.
3. [출시 보안·Windows 서버 이전](release-readiness.md) — 현재 한계와 운영 절차.
4. [WSS·입장권의 설계와 코드](blog/part16-secure-admission.md) — 인증·전송·수명·실패 처리.
5. [가입 없는 계정의 보관·폐기·복구](blog/part17-guest-account-recovery.md) — DB 이관, 복구 파일, 실패 후 재시도.
6. [서버 경기 판정과 실패 상태](blog/part18-authoritative-results.md) — PvP 입력 검증·결과 사유·역할 분리.
7. [이번 폴리싱 검증 기록](polish-validation.md) — 실제 통과한 검사와 미검증 범위.

운영 전제는 **Mac 하드웨어의 Linux 주 서버, Windows 예비 서버**다.
이전 문서의 Linux relay + Android/Termux meta 구성은 가능한 별도 배치 예시다.
웹 폴더는 랭킹 페이지이며, 브라우저 게임은 아직 구현하지 않았다.

구현 원리를 공부하려면 [블로그 목차](blog/README.md)의 Part별 의존 순서를 따른다.
이미 있는 코드를 실행하려고 Part 0부터 다시 구현할 필요는 없다.

## 목적별 경로

| 목적 | 시작 문서 | 이어서 볼 문서 |
|---|---|---|
| 현재 품질과 위험 확인 | [품질 검토](architecture-quality-review.md) | [`blog/part12-hardening-and-release.md`](blog/part12-hardening-and-release.md)의 회귀·통합 검증 |
| 처음 빌드하고 실행 | [실행 안내](start-here.md) | [`blog/README.md`](blog/README.md)의 "빌드 규약 — 먼저 읽을 것" |
| 이론과 코드를 누적 구현 | [`blog/README.md`](blog/README.md) | 클라이언트·온라인 서비스·학습·인게임 AI 중 목적별 경로 |
| 완성 구조 확인 · 고칠 곳 찾기 | [`blog/part13-structure-and-build-reference.md`](blog/part13-structure-and-build-reference.md) | [품질 검토](architecture-quality-review.md)의 알려진 위험과 검증 근거 |
| 완성 코드를 빠르게 이해 | [`README.md`](../README.md)의 아키텍처 절 | 실제 코드 → [`blog/part13-structure-and-build-reference.md`](blog/part13-structure-and-build-reference.md) |
| 공개 서버 운영 | [`public-server-deployment.md`](public-server-deployment.md) | [`blog/part12-hardening-and-release.md`](blog/part12-hardening-and-release.md) |
| RL 모델 학습/export | [`python/train/README_colab.md`](../python/train/README_colab.md) | [`model/bots/README.md`](../model/bots/README.md) |

## 문서의 기준과 우선순위

설명이 충돌하면 다음 순서로 판단한다.

1. 테스트로 검증된 현재 코드와 `CMakeLists.txt`
2. 현재 실행·운영 문서와 해당 Part의 **현재 소스 발췌**
3. 품질 검토와 검증 기록에 날짜로 구분한 확인 범위
4. 해당 Part의 **Part N 체크포인트** — 구현 학습용 중간 상태이며 현재 배포 명령보다 우선하지 않음

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

문서를 고친 뒤 `python3 scripts/check_part_docs.py`로 Part 형식·현재 소스 발췌·로컬
파일 링크를 검사한다. CI에서도 같은 검사를 실행한다. 본문 설명과 단계별 체크포인트의
의미·빌드 가능성은 기계 검사와 별도로 확인한다.
