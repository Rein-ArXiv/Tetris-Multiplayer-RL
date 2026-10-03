# 139 — CSPRNG와 해시: 예측·노출·검색의 차이

누적 게임·계정 서비스를 유지하고 자격 생성·인코딩·목적 해시·조회·비밀 비교의
계약을 분리한 독립 실습을 추가한다. `meta/credential_lab.h`와
`tests/credential_crypto_contract.cpp`가 새 파일이다.

```sh
cmake -S docs/learn/checkpoints/139-credential-crypto -B out/study-139 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-139 --target credential_crypto_contract account_contract -j2
ctest --test-dir out/study-139 -R '^credential_crypto_contract$' --output-on-failure
python3 scripts/check_learning_credential_crypto.py --boost /path/to/boost/include
```

C++17·OpenSSL Crypto가 필요하다. 누적 WSS 도구의 구성에는 Boost 헤더도 필요하며
별도 경로는 `-DSTUDY_BOOST_INCLUDE=/path/to/boost/include`로 지정한다.
Windows 다중 구성에서는 빌드에 `--config Release`, CTest에 `-C Release`를 더한다.
SDL 구성의 `tetris`는 누적 그래픽 게임이다. 새 검사는 창이나 외부 서버가 필요 없다.

생성 실패 주입은 로컬 실험 전용이다. 실제 경로는 기본 RAND_bytes를 사용하며
실패 출력이나 시간값으로 대체하지 않는다. 실행은 자격 자체를 출력하지 않는다.
고정 fixture는 공개 검사 값이며 실제 계정에 사용하지 않는다. 실제 난수 호출 검사는
호출 경로·형식만 확인하며 난수의 예측 불가능성을 표본으로 증명하지 않는다.

CredentialIndex는 단일 스레드 메모리 실습이다. 요약과 ID를 멤버로 유지하지만,
호출자 원문·임시 계산 버퍼의 안전 삭제, 지속성·동시 접근·용량 정책은 제공하지 않는다.
접두사 study-account-v1은 누적 account_hash와 호환되며, 실제 서버의 tetris 목적
접두사와는 다르다. recovery 목적 계산은 도메인 분리 실험으로만 사용한다.

검사 도구는 SCRIPTED/SDL 검사·누적 계정 호환성·SDL 게임 빌드와 학습/현재 코드의
ASan/UBSan을 실행한다. `--snippets-only`는 발췌·심볼·누적 파일 보존만 확인한다.
기능 검사는 시간 부채널 부재나 모든 실행 플랫폼의 보안을 증명하는 검사가 아니다.
