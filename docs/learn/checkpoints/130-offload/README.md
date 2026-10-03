# 130 — 제한된 오프로드와 루프 결과 회수

129-timers 누적 파일을 보존하고 net/offload.h·tests/offload_contract.cpp·tools/offload_probe.cpp를 추가한다.

```sh
cmake -S docs/learn/checkpoints/130-offload -B out/study-130 -DSTUDY_PLATFORM=SCRIPTED -DSTUDY_AUDIO=NONE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-130 --target offload_contract offload_probe -j2
out/study-130/offload_probe
ctest --test-dir out/study-130 -R "^(offload_contract|offload_probe|epoll_offload_probe)$" --output-on-failure
```

Windows 다중 구성은 --config Release/-C Release와 Release/offload_probe.exe 경로를 사용한다.
기본 poll/WSAPoll, Linux --epoll. 누적 게임은 SDL 구성 tetris 타깃.
Offload의 Job은 워커, Cont/failure는 drain 뒤 루프에서 실행한다.
capacity는 대기+실행+미회수 결과의 합. list 노드 이동으로 완료 공개 시 추가 큐 할당을 피한다.
empty success는 즉시 슬롯을 반환한다. 작업 예외는 결과로, wake 예외는 카운터로 회수한다.
shutdown은 같은 소유자가 순차 호출하고 워커에서 호출하지 않는다. join 뒤 마지막 drain/적용을 마친다.
프로브의 future 대기는 느린 서비스를 통제하는 실험 장치이며, 루프 핸들러가 기다리는 구조가 아니다.
큐 상한은 바이트나 전체 실행 시간 상한이 아니다. 미완료 작업이 참조한 서비스와 Reactor를 유지한다.

검사: python3 scripts/check_learning_offload.py
고장 주입: python3 scripts/check_learning_offload_root.py
STUDY_MINGW_CXX가 있으면 Windows 대상도 교차 빌드한다. 네이티브 실행과 구분한다.
독립 복사 시 STUDY_VENDOR_DIR 지정. 공개 배포와 별개인 로컬 기준 코드다.
