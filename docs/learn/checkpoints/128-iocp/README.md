# 128 — 실제 Windows IOCP 수신과 완료 회수

127-epoll 전체 코드를 보존하고 양수 길이 WSARecv 수신기를 추가한다.
Windows의 실제 프레임 프로브:

```sh
cmake -S docs/learn/checkpoints/128-iocp -B out/study-128 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE
cmake --build out/study-128 --config Release --target iocp_probe completion_rules
out/study-128/Release/iocp_probe.exe
ctest --test-dir out/study-128 -C Release -R "^(iocp_probe|completion_rules)$" --output-on-failure
```

단일 구성은 -DCMAKE_BUILD_TYPE=Release와 Release 하위 폴더 없는 실행 경로를 쓴다.
Linux에서는 completion_rules와 누적 poll/epoll 경로를 빌드한다.

IocpReceiver는 새 connected overlapped TCP Socket과 기본 IOCP 통지 설정을 전제한다.
Runtime은더오래유지.한미회수요청.고정주소OVERLAPPED/16바이트저장소.
post id!=0/capacity1~16,즉시성공도패킷회수전pending. ready도take전재제출거절.
GQCS FALSE+nonnull은실패완료,FALSE+null은timeout/포트오류,wake는별도키/null.
결과값은바이트를소유.취소는상태회수를대신하지않는다.
소멸시취소+완료회수,복구불가포트오류시미완료State전체를프로세스종료까지보존/진단.
한스레드소유,wake만다른일반스레드허용,파괴전wake호출자종료/join.

검사: python3 scripts/check_learning_iocp.py
교차컴파일러는 STUDY_MINGW_CXX 또는 PATH의 x86_64-w64-mingw32-g++를 사용한다.
Linux API대역은동일한Windows .cpp를컴파일하지만Windows ABI/커널구현이아니다.
의도적포트오류보존시나리오만누수검사끄기,정상수명검사는ASan누수검사유지.
현재서버예약필드판정회귀: python3 scripts/check_learning_iocp_status.py
네이티브Windows실행과교차빌드증거는구분한다. 공개배포상태와연동하지않는다.
게임은누적SDL구성tetris타깃,독립복사는STUDY_VENDOR_DIR지정.
