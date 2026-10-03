# 44차시 — 규칙 진행과 화면 그리기

43-history의 규칙은 유지하고 main의 시계·입력·틱 반복을 loop/frame_runner.h로 분리한다.

```sh
cmake -S docs/learn/checkpoints/44-frame-loop -B out/study-44 -DSTUDY_PLATFORM=SCRIPTED
cmake --build out/study-44
./out/study-44/frame_demo
ctest --test-dir out/study-44 --output-on-failure
```

데모 시간8ms/9ms/50ms→0/1/3틱. 첫 프레임만Up·Space→다음 틱의T-spin Single800점.
마지막틱보고는이후-1로돌아가도고정시점의FrameReport값은남는다.

SDL2·pkg-config·desktop OpenGL3.3 Core 환경에서 STUDY_PLATFORM=SDL로별도빌드한뒤
`tetris T spin`실행. 기존키·시나리오·보드/점수/미리보기/고스트표시유지.

- FrameRunner owns Round/FixedClock/PendingControls, explicitduration/input, noIO/GL/platform.
- 한프레임후보에서시계/입력/규칙을함께게시. invalid면원본보존,스레드원자성아님.
- FrameReport:최대6개의유효틱관찰,각고정값복사,OR변경힌트,프레임삭제합계.
- Round참조는최종상태,LockReport는과거사건값. stopped는과거보고재발행안함.
- main은보고를출력한뒤최종Round의메시를갱신·그리기. 제출실패가규칙을되돌리지는않음.
-36누적계약.0틱입력보관/실패후보존/여러고정/종료/분할시간/읽기전용투영.
- frame_probe는실행기를통해0틱입력보관→1틱고정.18화면의보드/활성/고스트VBO·RGB.
  점수·미리보기·종료마커는이실험범위에포함하지않는다.
- 실제main의온라인/모달/오디오/비동기업무를이예제로교체하지않는다.

검증: `python3 scripts/check_learning_frame_loop.py`. 집필환경·한계는REVIEW_LOG.md참조.
