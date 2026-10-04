# Windows 이전: 명령행 인코딩과 PORT 줄 끝

누적 계정 서비스의 업무 계약을 유지하면서 Windows의 넓은 명령행을 UTF-8로
변환한다. SQLite에 전달하는 DB 경로와 포트 공지의 LF/CRLF 경계를 분명하게 만든다.

변경 파일은 meta/account_service.cpp, roles/CMakeLists.txt,
operations/local_service.py다. platform/utf8_arguments.h와 .cpp,
tests/utf8_arguments_test.cpp를 추가했다. 다른 누적 파일은 그대로 보존한다.

## 만들기

Linux:

```sh
cmake -S docs/learn/checkpoints/170-windows-port/roles -B out/study170 -DSTUDY_ROLE=SERVICE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study170 --target study_account_db
python3 scripts/check_learning_windows_port.py --binary out/study170/study_account_db
```

Windows PowerShell, Visual Studio 생성기:

```powershell
cmake -S docs/learn/checkpoints/170-windows-port/roles -B out/study170 -DSTUDY_ROLE=SERVICE
cmake --build out/study170 --config Release --target study_account_db utf8_arguments_test
ctest --test-dir out/study170 -C Release --output-on-failure
python scripts/check_learning_windows_port.py --binary out/study170/Release/study_account_db.exe
```

대상 OpenSSL 개발 파일과 실행 DLL이 필요하다. vcpkg를 사용하면 구성 단계에
해당 toolchain을 지정한다. 단일 구성 생성기는 실행 파일의 실제 위치를 사용한다.
이 실습은 GUI나 Python 학습 프레임워크를 요구하지 않는다.

## 따라가기

- wmain의 UTF-16 인자를 엄격한 UTF-8로 변환한다. 빈 인자와 공백도 보존한다.
- 문자열 벡터를 완성한 뒤 char* 뷰를 만들고 run이 끝날 때까지 소유자를 유지한다.
- Python은 셸 없이 인자 리스트를 전달하고 LF/CRLF 공지만 받아들인다.
- 임시 한글·공백·이모지 DB 경로에서 계정 생성 → 종료 → 재시작 조회를 확인한다.
- 소켓의 핸들 폭·실패 센티널·해제 API, Winsock 초기화 수명은 net/socket 계층에서 읽는다.

실행 보조의 CLI는 준비 확인 직후 자식을 종료한다. 서버를 계속 켜 두려면
study_account_db에 포트와 DB 경로를 직접 전달한다. terminate는 소유한 프로세스를
회수하는 정책이며 진행 중 요청을 모두 마치는 응용 수준 정상 종료가 아니다.
Windows API 검사는 Windows 실행 환경에서 수행한다. 다른 OS의 컴파일이나
입력 바이트 fixture를 Windows 실제 실행과 같은 증거로 해석하지 않는다.
