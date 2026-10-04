# 의존성을 가져오고 설치 경로에서 실행하기

누적 규칙·서비스·바인딩·정책 구현을 유지한다. inference 빌드가
`dependencies/OnnxRuntime.cmake`의 `Tetris::OnnxRuntime` 타깃을 사용한다.
`ORT_ROOT`는 `include/`와 `lib/<platform>/`가 있는 SDK 디렉터리다.
공식 압축의 평평한 lib 폴더를 쓴다면 선택한 대상 폴더 아래로 배치한다.
헤더와 라이브러리는 같은 SDK 릴리스에서 확보한다.

체크포인트 디렉터리에서:

```sh
cmake -S roles -B build-policy -DSTUDY_ROLE=POLICY -DCMAKE_BUILD_TYPE=Release -DORT_ROOT=/path/to/sdk
cmake --build build-policy --config Release --parallel 2
ctest --test-dir build-policy -C Release --output-on-failure
cmake --install build-policy --config Release --prefix /path/to/study-install
```

`study-install/bin/dependency_probe`를 다른 작업 디렉터리에서 실행한다.
Linux는 실행 파일 기준 `../lib`에서 ORT를 찾고 버전을 출력한다.
설치 디렉터리 전체를 옮겨도 같은 관계를 유지한다. build-policy 아래의 실행 파일은
빌드 SDK 경로를 이용할 수 있으므로 배포 경로 검사와 구별한다.

`onnx_probe model.onnx seed decisions`와 `policy_match_probe`도 설치된다.
모델은 별도 입력이고 이 설치 규칙이 자동 선택하지 않는다. 기준 구현의 실제 정책
검사는 `tests/policy_match_inference.py`를 사용하며 TRAINING 역할로 만든
study_py 경로와 설치한 policy_match_probe 경로를 넘긴다.

Windows의 .lib는 링크 입력이고 .dll이 실행 시 필요하다. 이 실습은 설치된 bin에
SDK DLL을 둔다. macOS는 설치 실행 파일의 @loader_path/../lib 경로와 SDK의
install name을 함께 확인한다. 파일 경로 선택 검사는 다른 OS 바이너리의 실행
검증을 대신하지 않는다. 로컬 설치 실습은 시스템 런타임·그래픽 드라이버·운영 데이터·
재배포 조건까지 갖춘 제품 릴리스가 아니다.

RULES/SERVICE/TRAINING 역할은 그대로다. 계정 DB를 설치 트리에 넣지 않으며
필요한 서비스 인자와 새 DB 경로는 meta/account_service.cpp의 진입 계약을 따른다.
