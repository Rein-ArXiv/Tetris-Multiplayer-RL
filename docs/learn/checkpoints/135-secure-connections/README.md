# 135 — HTTPS API와 게임 연결의 분리

134 누적 파일을 CMake/README 외 보존한다. ConnectionPlan은 이미 분해된 두 주소의
역할과 전송 정책을 검사한다. URI 파서·DNS 검증·게임 접속 성공을 대신하지 않는다.
HTTPS 프로브는 API만 연결하고 실제 인증서 체인과 SAN 이름을 확인한다.

```sh
cmake -S docs/learn/checkpoints/135-secure-connections -B out/study-135 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-135 --target connection_policy_contract https_probe -j2
ctest --test-dir out/study-135 -R connection_policy --output-on-failure
python3 scripts/check_learning_secure_connections.py
```

OpenSSL 개발 라이브러리와 저장소 third_party 헤더가 필요하다. 검사 스크립트가 로컬
시험 CA/인증서를 임시 폴더에 만들고 HTTPS 서버와 프로브를 연결한 뒤 회수한다.
프로브 단독 사용: `https_probe localhost PORT /path/to/test-ca.pem`.
응답 계약은 `/healthz`의200과 `study-meta-ready` 본문이다.
CA 인자 `-`는 기본 저장소 사용이며 검증 생략이 아니다. 종료4는 TLS/전송 실패,
5는 HTTP/본문 거절이다. 시간 제한은 연결·읽기·쓰기 각 단계의1초 설정이다.

- net/connection_policy.h: 공개 HTTPS+WSS, 개발용 숫자 loopback 예외.
- net/tls_identity.h: 검증된 체인·실제 peer·DNS/IP SAN 비교.
- tools/https_probe.cpp: 정책 확인→TLS API 요청→HTTP/본문 확인.
- tests/connection_policy_contract.cpp: 역할·모드·개별 평문 경계와 입력 바이트 검사.

Windows 다중 구성 빌드는 `--config Release`, CTest는 `-C Release`, 실행 경로는
`Release/`를 사용한다. OpenSSL 라이브러리도 타깃 OS/아키텍처와 일치해야 한다.
누적 SDL 게임은 `STUDY_PLATFORM=SDL` 구성에서 `tetris`로 빌드한다.
