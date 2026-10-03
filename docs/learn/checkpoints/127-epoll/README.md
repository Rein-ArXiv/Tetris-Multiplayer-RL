# 127 — Linux epoll 백엔드

126-reactor-contract 누적 코드를 보존하고 Linux epoll/eventfd 구현을 추가한다.
공통 reactor_probe/reactor_contract는 --epoll 팩토리 선택만 추가한다.

```sh
cmake -S docs/learn/checkpoints/127-epoll -B out/study-127 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-127 --target reactor_probe reactor_contract epoll_contract -j2
out/study-127/reactor_probe
out/study-127/reactor_probe --epoll
ctest --test-dir out/study-127 -R "^(reactor_contract|reactor_probe|epoll_shared_contract|epoll_frame_probe|epoll_contract)$" --output-on-failure
```

Linux용 epoll_contract는 실제 파이프 LT/ET/ONESHOT, 마스크0의HUP/남은바이트,
커널관심DEL후정지,ADD/MOD/DEL실패주입후상태보존,half-close후수신/역방향전송,
100wake병합을 확인한다. 실패주입은 GNU호환 linker --wrap=epoll_ctl을 사용한다.
주입하지않은호출은실제커널로전달하며, ENOSPC주입은실제오류발생빈도를재현하지않는다.

EpollReactor는 단일소유자·등록ID·콜백수명 계약을 유지한다. 커널data.u64=0은wake전용.
관심0은사용자ID보존/커널DEL,재개ADD,활성변경MOD. 성공뒤표갱신. ADD실패ID재사용안함.
UniqueFd는내부epoll/eventfd소유,연결소켓은빌림. remove후socket닫기.
LT운영백엔드,ET/ONESHOT은비교실험에서만사용한다. 요청대기0~1000ms.
파괴전wake호출자중단/join,Runtime은루프보다오래유지. 이미받은배치는제거로자동삭제안됨.

검사: python3 scripts/check_learning_epoll.py
실제Linux깨우기읽기회귀: python3 scripts/check_learning_epoll_drain.py
네이티브epoll은Linux전용. 다른OS의공통poll경로에는epoll소스를연결하지않는다.
게임은별도SDL구성tetris타깃. 독립복사는STUDY_VENDOR_DIR를지정한다.
