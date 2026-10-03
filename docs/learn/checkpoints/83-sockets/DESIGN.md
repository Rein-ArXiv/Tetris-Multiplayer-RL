# 83 구현 경계

누적 게임·렌더러·오디오를 변경하지 않고 StudySocket과 socket_probe를 추가한다.
Runtime은 성공한 Winsock 준비 참조 하나를 소유하고 Socket보다 오래 산다.
Socket은 move-only이며 native()는 대여, release()는 소유권 이전이다.
POSIX descriptor0도 유효하다. Windows 핸들은 uintptr_t로 보존한다.

socket → bind(loopback) → listen → getsockname으로 리스너를 만든다.
accept는 별도 소켓을 반환한다. 서버는 한 연결 수락 후 리스너를 닫는다.
backlog는 연결 대기열 요청값이며 총 접속자 수 제한이 아니다.
실패 오류를 OS 정리 호출 전에 복사하고 부분 자원은 RAII로 해제한다.
Windows 배타 바인드, Linux MSG_NOSIGNAL, macOS SO_NOSIGPIPE를 사용한다.

1바이트 recv의 길이0은 EOF, 받은 바이트의 값0은 정상 데이터다.
send 성공은 로컬 수락이다. 클라이언트는 같은 값의 응답을 확인한다.
게임 네트워크 계층에 합치기 전 바이트 표현·대기 정책을 독립 진단에서 확립한다.
메시지 프레이밍·동시성·인증·타임아웃·재접속은 이 기준 코드에 포함하지 않는다.
