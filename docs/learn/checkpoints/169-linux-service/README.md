# Linux 서비스의 실행 경계와 준비 상태

누적 SERVICE의 `study_account_db`를 그대로 사용하고 `operations/local_service.py`를
추가한다. 빌드·설치 경로, 프로세스 작업 디렉터리, 가변 DB 경로를 따로 다룬다.
이 체크포인트의 실행 보조는 Linux의 신뢰한 학습 서비스와 루프백 실습용이다.
공개 서버 기동이나 방화벽·인증 설정을 대신하지 않는다.

체크포인트 디렉터리에서:

```sh
cmake -S roles -B build-service -DSTUDY_ROLE=SERVICE -DCMAKE_BUILD_TYPE=Release
cmake --build build-service --target study_account_db --parallel 2
mkdir -p study-state
python3 operations/local_service.py build-service/study_account_db "$PWD/study-state/accounts.sqlite"
```

보조는 자식을 포트 0으로 실행하고, 자식의 `PORT` 공지와 `/healthz`를 확인한다.
출력된 주소는 실행 중인 컨텍스트의 주소다. 이 CLI는 기동 검사를 끝내면 자식을
종료하므로 명령 종료 후 접속하는 상시 서버 주소가 아니다. DB는 남는다.

직접 실행 상태를 유지해 살펴보려면 같은 C++ 프로그램에 선택한 포트와 DB 경로를 준다.
포트 0을 주면 운영체제가 선택한 실제 번호를 출력한다. 이미 사용 중인 포트를
빼앗거나 다른 프로세스를 종료하지 않는다.

```sh
./build-service/study_account_db 0 "$PWD/study-state/accounts.sqlite"
# 다른 터미널: 출력의 PORT 번호로 GET /healthz를 요청한다.
# 종료할 때는 이 프로그램을 실행한 터미널에서 Ctrl+C를 누른다.
```

실행 보조를 가져와 `with local_service(binary, database) as url:` 안에서 HTTP 작업을
수행할 수 있다. 컨텍스트를 나올 때 성공·예외 모두 자신이 만든 자식만 종료하고 회수한다.
`/healthz`의 성공은 그 경로의 HTTP 응답을 확인한 것이다. DB 쓰기, 모델 추론, 하위
서비스 준비는 각각 필요한 요청으로 검사한다. 현재 보조의 소켓 timeout은 각 I/O의
대기 한도이며 느린 원격 응답 전체에 적용되는 강제 종료 시계가 아니다.

규칙·바인딩·정책의 누적 코드는 유지된다. 제품의 Linux 배포는 별도의 meta/relay/WSS
프로세스, 인증 정보, TLS 인증서, 데이터 디렉터리와 실행 계정을 설정해야 한다.
학습용 포트 0 공지 계약과 제품의 고정 서비스 포트 설정을 구분한다.
