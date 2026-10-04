# 필요한 역할만 구성하는 빌드 진입점

기존 누적 소스와 루트·bindings·inference CMake는 보존한다. 새 roles/CMakeLists.txt가
규칙·계정 서비스·학습 바인딩·정책 추론 중 한 역할을 선택한다. 소스 목록을 새로운 규칙
구현으로 복제하지 않고 같은 누적 코드를 다시 사용한다.

체크포인트 디렉터리에서 실행한다.

```sh
cmake -S roles -B build-rules -DSTUDY_ROLE=RULES -DCMAKE_BUILD_TYPE=Release
cmake --build build-rules --config Release --parallel 2
ctest --test-dir build-rules -C Release --output-on-failure
cmake -S roles -B build-service -DSTUDY_ROLE=SERVICE -DCMAKE_BUILD_TYPE=Release
cmake --build build-service --config Release --parallel 2
```

RULES의 binding_oracle과 policy_match_contract는 실제 누적 Round/Match를 사용한다.
SERVICE의 study_account_db는 `포트 DB경로` 인자를 받는다. 포트0은 사용 가능한 루프백
포트를 고르고 PORT를 출력한다. `/healthz`, `/study/v1/guest`, `/study/v1/me` 등 기존
학습 서비스 경로를 사용한다. account_contract는 존재하지 않는 새 DB 경로를 인자로
요구하므로 매 검사마다 새 임시 폴더를 사용한다.

서비스는 OpenSSL Crypto와 저장소 third_party의 HTTP/JSON/SQLite 파일이 필요하다.
체크포인트를 옮겼다면 -DSTUDY_VENDOR_DIR로 그 디렉터리를 지정한다. RULES에는 이
패키지 검색과 C 컴파일러 활성화가 필요하지 않다.

```sh
cmake -S roles -B build-training -DSTUDY_ROLE=TRAINING -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=/path/to/python -Dpybind11_DIR=/path/to/pybind11/share/cmake/pybind11
cmake --build build-training --config Release --parallel 2
ctest --test-dir build-training -C Release --output-on-failure
cmake -S roles -B build-policy -DSTUDY_ROLE=POLICY -DCMAKE_BUILD_TYPE=Release -DORT_ROOT=/path/to/onnxruntime-sdk
cmake --build build-policy --config Release --target policy_match_probe --parallel 2
```

TRAINING은 bindings를, POLICY는 inference를 하위 프로젝트로 연결한다. Python 확장은
`build-training/bindings/python/Release`, 추론 실행기는 `build-policy/inference` 아래에
생긴다. 다중 구성 생성기의 실행 파일은 구성 하위 폴더, Windows는 .exe를 확인한다.
실제 Python 인터프리터/모듈 경로와 SDK의 대상 OS·아키텍처를 맞춘다.

이 진입점은 헤드리스 누적 구성요소를 선택한다. 그래픽 데모는 기존 루트 CMake에 남아
있으며, 현재 제품의 tetris/relay/meta 조합은 저장소 루트의 CMakePresets.json에서
선택한다. 서로 다른 제품 이름과 서비스 프로토콜을 같은 것으로 대체하지 않는다.
