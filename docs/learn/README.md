# 프로젝트 동행 학습 화면

[공개 강의 사이트](https://rein-arxiv.github.io/Tetris-Multiplayer-RL/) — `main`에 반영된 검수 완료 강의를 GitHub Pages에서 읽을 수 있다.

[index.html](index.html)을 브라우저로 열면 된다. 외부 라이브러리나 빌드 과정 없이
HTML·CSS·JavaScript와 로컬에서 생성한 참고 자료로 동작한다. 필요한 강의 본문과 코드는 화면 안에 포함한다.
기존 Part 0~18과 관련 운영 문서는 HTML 참고 패널에서 읽는다. 소스 파일도 같은 화면에서 열 수 있다.

로컬 학습 사이트는 학습 지도, 강의 본문, 객관식 문제와 즉시 해설, 짧은 복습,
원문 절별 대응 현황과 GPU 학습 경로를 제공한다. 작성된 차시와 집필 예정 차시는
학습 지도에서 확인한다. 편성은 구현 주제와 필요한 CS 설명에 따라 확장한다.
각 차시는 짧은 복습 → 목표 → 필요한 CS → 구현·실행 → 현재 코드 연결 →
확인 문제 순서로 확장한다. 하나를 선택하고 확인을 누르면 그 자리에서 정답·해설을 제공한다.
다른 선택으로 바꾸면 해설을 숨기고 다시 확인할 수 있다. 미선택 상태에서는 답을 알려 주지 않는다.
짧은 복습과 확인 문제는 기본적으로 접혀 있다. 제목을 누르면 열고 닫을 수 있고,
목차에서 해당 항목을 선택하면 펼쳐서 이동한다. 접어도 선택한 답과 확인 결과는 유지된다.
강의에는 코드·상태표를 읽기 전 예측 질문과 곁들여 읽는 메모를 둔다.
필수 설명은 본문에 두고, 추가 용어·반례·복습 링크는 접힌 메모로 제공한다.
넓은 화면에서는 본문 옆, 좁은 화면과 소스 뷰어를 연 상태에서는 아래에 표시한다.

## 여는 방법

`index.html`을 직접 열 수 있다. 브라우저가 로컬 파일의 저장·클립보드를 제한하면
저장이 불가능하다는 안내 또는 복사 실패 안내가 표시된다.
같은 주소로 다시 열어 학습 기록을 이어가려면 저장소 루트에서 다음 명령을 실행한다.

```bash
python3 scripts/serve_learning.py
```

브라우저에서 `http://127.0.0.1:18765/learn/`을 연다.
이 명령은 문서와 허용 목록의 소스 파일만 현재 기기의 루프백 주소로 제공하며 게임 서버와 별개다.
소스를 선택하거나 다시 읽기를 누르면 현재 작업 폴더의 파일을 읽는다. 로그인 정보·DB·임의 경로는 제공하지 않는다.
종료하려면 실행 중인 터미널에서 Ctrl+C를 누른다.

## 기록과 유지보수

- **읽기 설정 · 이어 읽기**에서 글자 크기, 밝은/어두운 테마, 긴 코드 줄바꿈을 선택한다.
  설정과 현재·이전에 읽던 차시/절을 이 브라우저에 저장한다. **이전 읽던 위치**는
  방금 떠난 차시나 절로 연결되며 현재 위치로 되돌아가는 링크는 숨긴다. 정확한 스크롤 위치 복원은 아니다.
- 목차에서 번호나 제목으로 작성된 차시를 찾는다. 모바일 목차는 기본적으로 접혀 있고,
  화면 아래 **학습 목차 / 본문 맨 위**로 긴 강의에서도 이동할 수 있다.
- 목차 이동 시 키보드 초점도 대상에 따라간다. 코드·표는 Tab으로 접근해 가로로 스크롤한다.
  소스 뷰어에 초점이 있을 때 Esc로 닫으면 원래 열었던 버튼으로 돌아온다.

- 차시 끝에서 학습 완료를 체크하면 목차 제목 옆에 단색 ✓가 나타난다. 체크를 해제하면 사라진다.
- 답안과 수동 완료 표시는 브라우저의 localStorage에 저장한다. 서버 저장, 계정 연동,
  서버 채점이나 대화로 전송하는 기능은 없다. 객관식 정오 확인은 브라우저에서 수행한다.
- 객관식 전환 전에 쓴 서술 답안은 별도 접힘 영역에서 읽을 수 있고 새 선택으로 덮어쓰지 않는다.
- 기록은 브라우저·주소별로 분리된다. 다른 포트나 로컬 파일 주소로 열면 기록이
  달라질 수 있다. 브라우저 데이터를 삭제하면 없어질 수 있다.
- 중요한 답안은 화면의 복사 기능으로 따로 보관한다.
- 1차시는 `index.html`, 이후 집필 원본은 `lessons/*.json`이며, 검토된 본문만
  `build_learning_lessons.py`가 `lessons.js`로 묶는다. 학습 화면은 `course.js`가
  구성하고, 탐색·저장·복사는 `app.js`, 스타일은 `style.css`가 담당한다.
- `checkpoints/`는 강의 기준 코드다. 현재 게임 소스와 자동 동기화하지 않는다.
  강의 버전과 변경 이유를 함께 기록하고 독립 빌드한다.
- HTML 강의와 기준 스니펫이 학습 본문의 기준이다. 현재 코드의 콘텐츠·수치 변경에 따라 자동 교체하지 않는다.
- 소스의 책임·의존 방향·입출력 계약이 바뀌면 관련 강의를 검토한다. 세부 차이는 그대로 허용한다.
- 정적 서버 또는 파일로 열면 소스는 보관된 스냅샷이며 화면에 명시한다. 실시간 작업 폴더 코드와 구분한다.
- 이 페이지는 게임을 실행하거나 C++ 코드를 컴파일하지 않는다. 실습은 별도 연습
  디렉터리에서 진행한다. 브라우저로 실행하는 테트리스 클라이언트와도 별개다.

## 참고 자료 갱신

```bash
uv run --with markdown-it-py==3.0.0 python scripts/build_learning_library.py
uv run --with markdown-it-py==3.0.0 python scripts/build_learning_library.py --check
uv run --with markdown-it-py==3.0.0 python scripts/build_learning_coverage.py
uv run --with markdown-it-py==3.0.0 python scripts/build_learning_lessons.py
```

원문을 변경했다면 coverage를, 강의나 기준 코드를 변경했다면 lessons를 재생성한다.
각 스크립트는 `--check`도 지원한다. `coverage-evidence.json`의 내용 해시는 원문
대응을 검토한 시점을 기록한다. 원문 변경 뒤에는 대응 상태를 다시 검토한다.
`coverage.json`에서 후보 차시 배정과 본문 대응 완료는 다른 상태다.
대응표는 원문·강의 탐색과 원문 변경 추적에 사용한다. 모든 항목을 완료로 채우는 작업은
강의 집필 완료의 조건이 아니다. 기존 기록을 재사용하고 변경·누락이 확인된 구간만
대조한다. 원문 해시만으로 소스 변경까지 감지할 수는 없으므로 구현 계약은 별도로 확인한다.

원문과 명시적으로 참조된 소스를 묶어 `library.js`와 해시 목록을 생성한다.
집필용 `_style_guide.md`는 학습 자료에 넣지 않는다. `--check`는 스냅샷의 변경 여부를
검사하며 강의 스니펫과 현재 코드가 동일해야 한다는 뜻은 아니다. 현재 파일 조회는
스냅샷 갱신 없이도 가능하지만, 새 파일을 뷰어 허용 목록에 추가할 때는 재생성한다.

[집필 기준](AUTHORING.md)과 [OpenCode·DeepSeek 연결](opencode-deepseek.md)을 참고한다.
원문 통합과 전체 강의 재집필은 서로 다른 진행 상태다.
[전체 재구성 계획](RECONSTRUCTION_PLAN.md), [검토·검증 기록](REVIEW_LOG.md)도 함께 관리한다.

## 기준 코드 검증

Linux에서 CMake·C++ 컴파일러·SDL2 개발 패키지·pkg-config가 준비되어 있다면
저장소 루트에서 `python3 scripts/check_learning_checkpoints.py`로 실행 예제를
빌드하고 SDL dummy/실패 주입/입력 사건 검사를 수행할 수 있다.
특정 단계만 확인하려면 `07-keys 08-text`처럼 체크포인트 이름을 인자로 준다.
빌드 결과는 `out/learning-checkpoints/`에 생성한다.
이 검사는 실제 GUI나 다른 운영체제 실행을 대신하지 않는다.

## GitHub Pages와 다른 기기

[배포 안내](DEPLOYMENT.md)에 정적 배포물 생성·GitHub Pages 워크플로·소스 스냅샷의
갱신 방법을 정리했다. 개발용 학습 서버를 외부로 개방할 필요는 없다.
현재 답안 저장은 브라우저별이다. 사용자 요청으로 기록 파일 내보내기/가져오기 기능은 삭제했다.

객관식 저장·확인·다시 풀기·기존 답안 보존 검사는 `node tests/learning/quiz.cjs`로 실행한다.

GL 실습의 별도 검사: `python3 scripts/check_learning_gl.py`. Linux 실패 주입 검사는
실제 GPU 표시를 증명하지 않는다. 집필 환경의 SDL 속성 설정 조회는 3.3 Core이고 (직접 GL 조회는 4.5 Core),
단일 버퍼여서 학습 예제의 더블 버퍼 조건에서 거부되었고, 실제 GUI는 미검증이다.

12차시 로더 검사: `python3 scripts/check_learning_loader.py`. 실제 offscreen GL 조회와
학습/실제 소스 로더·WGL 반환값 모델을 구분한다. Windows/macOS 네이티브는 미검증이다.

13차시 CPU 정점 검사: `python3 scripts/check_learning_mesh.py`. SDL/SCRIPTED Release 빌드와
좌표·바이트 범위·용량 규칙, 잘못된 배치/좌표의 거부를 확인한다. GPU 업로드 검사는 아니다.

14차시 버퍼 검사: `python3 scripts/check_learning_vbo.py`. SDL/SCRIPTED 빌드와 실패 정리,
실제 GL 저장소 복사·조회·재정의 및 NDEBUG 삭제 누락 검출을 확인한다.

15차시 VAO 검사: `python3 scripts/check_learning_vao.py`. VAO별 속성·버퍼 연결과 일반
ARRAY_BUFFER 바인딩의 차이, 형식·실패 정리·잘못된 stride 검출을 확인한다.

16차시 셰이더 검사: `python3 scripts/check_learning_shader.py`. 소스 복사·컴파일 상태와
로그·실패/예외 정리, 현재 렌더러 객체 생성 실패 경로를 확인한다.

원고 코드 블록의 형식 오류 검사는 `python3 tests/learning/manuscript_schema.py`로 실행한다.

37차시 종료 상태·판정 순서·GL 화면 검사: `python3 scripts/check_learning_end.py`.
검수 환경과 한계는 REVIEW_LOG.md에 기록한다.

38차시 점수·레벨·정수 포화·숫자 GPU 검사: `python3 scripts/check_learning_score.py`.
실제 게임의 득점 회귀는 CTest `sim_score`에서 검사한다.

39차시 착지 예측·상태 보존·GPU 겹침 검사: `python3 scripts/check_learning_ghost.py`.
현재 게임의 고스트 갱신 회귀는 CTest `sim_ghost`에서 검사한다.

40차시 유지 입력·반복 시계·중력 결합·GPU 검사: `python3 scripts/check_learning_soft_drop.py`.
현재 게임의 소프트 드롭 계약은 CTest `sim_soft_drop`에서 검사한다.

41차시 하드 드롭·입력 소비·고정 경계·GPU 검사: `python3 scripts/check_learning_hard_drop.py`.
현재 게임 회귀: CTest `sim_hard_drop`.

42차시 공격·대기·가비지 행 변환·GPU 검사: `python3 scripts/check_learning_combat.py`.
현재 게임 회귀: CTest `sim_combat`.

43차시 회전 이력·연속 삭제·T-spin GPU 검사: `python3 scripts/check_learning_history.py`. 현재 게임 CTest `sim_t_spin`.

44차시 프레임 실행·보고 스냅샷·GPU 검사: `python3 scripts/check_learning_frame_loop.py`.

### 차시 렌더링 검사

본문 데이터는 정적 파일에 포함하고, DOM은 현재 읽는 차시만 생성합니다. 다른 차시로
이동하면 본문을 교체하고 답안·확인·완료 상태를 브라우저 저장값에서 복원합니다.
검색과 재개 링크는 표시 중인 본문의 유무에 의존하지 않습니다.

`node scripts/check_learning_navigation.cjs`는 jsdom 26.1.0으로 화면 없이 이 계약을 검사합니다.
jsdom을 별도 임시 디렉터리에 설치했다면 그 node_modules 경로를 NODE_PATH로 지정합니다.
브라우저 배포물에는 이 검사 의존성을 포함하지 않습니다.

52차시 정규 상태 바이트·해시·기존 통신 호환성 검사: `python3 scripts/check_learning_hash.py`.

53차시 골든·기록 검증·회귀 진단 검사: `python3 scripts/check_learning_golden.py`.

54차시 어댑터·값 뷰·Game 소유권 검사: `python3 scripts/check_learning_adapter.py`.

55차시 화면 전환·재시작 검사: `python3 scripts/check_learning_screen.py`.

56차시 CPU/GPU 배칭·픽셀 비교 검사: `python3 scripts/check_learning_batch.py`.

57차시 상태 경계・픽셀・이미지 수명 검사: `python3 scripts/check_learning_flush.py`.

58차시 텍스처 자료・상태 복원・아이콘 픽셀 검사: `python3 scripts/check_learning_texture.py`.

59차시 파일 디코딩・메모리 소유・행 변환 검사: `python3 scripts/check_learning_decode.py`.

60차시 이미지 핸들・소유권・재사용 거절 검사: `python3 scripts/check_learning_handles.py`.

61차시 UV・tint・회전과 픽셀 합성 검사: `python3 scripts/check_learning_transform.py`.

62차시 둥근 모서리·거리·알파 마스크 검사: `python3 scripts/check_learning_rounded.py`.

63차시 UTF-8 스칼라·복구·메모리 경계 검사: `python3 scripts/check_learning_utf8.py`.

64차시 폰트 메트릭·글리프·수명·실제 픽셀 검사: `python3 scripts/check_learning_glyph.py`.

한국어 조사·괄호 주변의 강조와 코드 보존 검사: `python3 scripts/check_learning_markdown.py`.

65차시 아틀라스·선반 배치·R8 업로드·여백·UV 수명 검사: `python3 scripts/check_learning_atlas.py`.

66차시 커닝·여러 줄·측정·CPU/GPU 배치 검사: `python3 scripts/check_learning_text_layout.py`.

67차시 캐시·배율·수명·실제 픽셀 검사: `python3 scripts/check_learning_font_cache.py`.

68차시 입력·UI·루트 클릭 회귀 검사: `python3 scripts/check_learning_immediate_ui.py`.

69차시 위젯 상태·입력 중재·루트 경계 검사: `python3 scripts/check_learning_widgets.py`.

70차시 프레임 배치·리사이즈 취소·픽셀/클릭 검사: `python3 scripts/check_learning_pointer_mapping.py`.

71차시 캐릭터 ID·역할별 이미지·정수 배치 검사: `python3 scripts/check_learning_character_art.py`.

72차시 대기 효과 시계·난수/규칙 분리·알파 검사: `python3 scripts/check_learning_idle_animation.py`.

73차시 설정 파싱·재시작·원본 보존·교체 단계 오류 검사: `python3 scripts/check_learning_settings.py`.

74차시 PCM 단위·경계·실제 SDL 로더 검사: `python3 scripts/check_learning_pcm.py`.

75차시 MP3 실제 샘플·예산·실패 정리 검사: `python3 scripts/check_learning_mp3.py`.

76차시 재생 수명·장치 실패·언로드 검사: `python3 scripts/check_learning_playback.py`.

77차시 콜백·잠금 밖 PCM 해제 검사: `python3 scripts/check_learning_callback.py`.

80차시 보이스 풀·공유 수명 검사: `python3 scripts/check_learning_voice_pool.py`. 실제 백엔드 비교: `python3 scripts/check_learning_voice_pool_root.py`.

79차시 규칙 사건·소리 연결 검사: `python3 scripts/check_learning_sound_events.py`. 실제 래퍼 수정 전/후 비교: `python3 scripts/check_learning_sound_events_root.py`.

78차시 믹싱·게인·클리핑 검사: `python3 scripts/check_learning_mixing.py`.

81차시 Windows 오디오·빌드 선택 검사: `python3 scripts/check_learning_xaudio.py`. 실제 Windows 백엔드 전체의 API 대역 비교: `python3 scripts/check_learning_xaudio_root.py`.

82차시 선택적 오디오 실패 검사: `python3 scripts/check_learning_audio_failure.py`. 실제 Game 실패 격리·Windows 반복 진단 비교: `python3 scripts/check_learning_audio_failure_root.py`.

83차시 소켓 수명·두 프로세스 통신·현재 서버 회귀 검사: `python3 scripts/check_learning_sockets.py`.

84차시 스트림 분할·반쪽 종료·실제 부분 I/O 검사: `python3 scripts/check_learning_tcp_stream.py`.

85차시 프레이밍·세션 오류 종료·C++/Python 직접 비교: `python3 scripts/check_learning_framing.py`.

86차시 직렬화·필드 범위·실제 INPUT 메시지 검증: `python3 scripts/check_learning_serialization.py`.

87차시 부분 송신·마감시간·취소 검증: `python3 scripts/check_learning_partial_send.py`.

88차시 연결 수명·half-close·재시작 검증: `python3 scripts/check_learning_connection.py`.

89차시 시드 협상·역할·메시지 검증: `python3 scripts/check_learning_seed.py`.

90차시 입력 교환·순차 소비: `python3 scripts/check_learning_lockstep.py`.

91차시 입력 지연·소비 일정: `python3 scripts/check_learning_delay.py`.

92차시 해시 교환·비교 창: `python3 scripts/check_learning_hash_audit.py`.
93차시 스레드·유한 큐: `python3 scripts/check_learning_threads.py`.
94차시 하트비트: `python3 scripts/check_learning_heartbeat.py`.

95차시 백프레셔: `python3 scripts/check_learning_backpressure.py`.

96차시 라운드 입력: `python3 scripts/check_learning_round_inputs.py`.

97차시 해시 관측: `python3 scripts/check_learning_hash_observation.py`.

98차시 종료 협상: `python3 scripts/check_learning_end_negotiation.py`.

99차시 릴레이 선택: `python3 scripts/check_learning_relay_choice.py`.

100차시 첫 요청·인계: `python3 scripts/check_learning_first_admission.py`.

101차시 워커 수명: `python3 scripts/check_learning_worker_lifetime.py`.

102차시 매칭 큐: `python3 scripts/check_learning_pair_queue.py`.
누적 파일 보존을 확인하고 새 두 타깃만 SCRIPTED/SDL에서 빌드·검사한다.

103차시 방 코드: `python3 scripts/check_learning_room_code.py`.
등록소의 새 두 타깃과 실제 서버의 난수 실패·종료 경합·할당 실패·게스트 정리를 검사한다.

104차시 수락 로비: `python3 scripts/check_learning_acceptance_lobby.py`.
두 참가자 수락·마감·분할 수신·이전 파서와 게임 바이트 인계를 검사한다.

105차시 포워더: `python3 scripts/check_learning_forwarder.py`.
106차시 동시 퇴장: `python3 scripts/check_learning_room_exit.py`.
107차시 연결 예산: `python3 scripts/check_learning_connection_budget.py`.
방향별 짧은 송신·입력 억제·실제 상대 수신·연결 쌍 종료를 검사한다.

108차시 메타 경계: `python3 scripts/check_learning_meta_boundary.py`.
109차시 메타 HTTP: `python3 scripts/check_learning_meta_service.py`.
110차시 테이블·키: `python3 scripts/check_learning_tables_keys.py`.
111차시 조회·인덱스: `python3 scripts/check_learning_indexes.py`.
112차시 이관: `python3 scripts/check_learning_migrations.py`.
113차시 트랜잭션: `python3 scripts/check_learning_transactions.py`.
114차시 멱등 정산: `python3 scripts/check_learning_idempotency.py`.
115차시 RP·XP·BP: `python3 scripts/check_learning_progression.py`.
수치 경계·레벨 임계값·원자적 정산·구 DB의 무소급 이관과 실제 코드의 정수 범위를 검사한다.

116차시 익명 계정: `python3 scripts/check_learning_guest_account.py`.
공개 ID·원문·해시 구분, 등록 전체 롤백과 본인 프로필 조회를 검사한다.

117차시 아이콘 소유권: `python3 scripts/check_learning_icon_ownership.py`.
서버 가격·독립 연결 동시 구매·소유권 격리·조회 오류와 부분 차감 방지를 검사한다.

118차시 JSON 경계: `python3 scripts/check_learning_json_boundary.py`.
문서·타입 분리, 바이트/NUL·중복키·UTF-8·정수범위·객체별 키 범위와
거절된 HTTP 요청의 계정·BP·소유권 보존을 검사한다.

119차시 HTTP 실패: `python3 scripts/check_learning_http_failure.py`.
같은 요청의 재시도·중단, 가짜 시계의 대기/예산, loopback 실패/시간 제한,
SQLite 최초 영수증, 실제 클라이언트의 오류 본문·입력 URL 비노출을 검사한다.

120차시 계정 부트스트랩: `python3 scripts/check_learning_account_bootstrap.py`.
저장·인증 분리, 동일 키 재저장, origin 고정, 손상·잠금·오프라인 보존,
파일 게시 뒤 실패, 실제 프로필 응답의 타입·범위를 검사한다.

121차시 저장 불확실성: `python3 scripts/check_learning_save_uncertainty.py`.
전송 전 요청 저장·응답 유실 뒤 재시작·동일 요청/최초 영수증·보상 한 번 반영·
로컬 영수증 저장 실패와 서버 확인 구분·실제 RP 상태 반영을 검사한다.

122차시 계정 패널: `python3 scripts/check_learning_account_screen.py`.
프레임의 비동기 결과 수집·중복 요청·동일 키 재시도·폴더 잠금·수명·실제 계정 재시작을 검사한다.

123차시 랭킹: `python3 scripts/check_learning_ranking.py`.
저장 결과·동점/64비트 ID 정렬·제한된 응답·공통 작업 수명·실제 메뉴/숫자 표시를 검사한다.
실제 서버 오류 회귀: `python3 scripts/check_learning_ranking_errors.py`.

원문에서 명시한 web HTML 소스도 참고 패널에서 읽고 복사할 수 있다.
HTML 문법 강조는 코드 텍스트에만 적용하며 해당 HTML을 페이지 요소로 삽입하지 않는다.

124차시 측정: `python3 scripts/check_learning_measurement.py`.
누적 TCP/프레이밍/작업자의 실제 에코·원시 표본·CPU 구간·실패 집계·닫힌 부하 조건을 검사한다.
성능 숫자를 고정 합격 기준으로 쓰지 않으며 운영 서버의 최대 인원으로 해석하지 않는다.

125차시 I/O 모델: `python3 scripts/check_learning_io_models.py`.
실제 loopback으로 힌트·수신 완료·프레임 완성과 취소/회수 수명을 검사한다.
현재 Windows 소멸자 회귀: `python3 scripts/check_learning_iocp_lifetime.py`.
이 회귀는 실제 메서드 추출과 완료 전달 대역이며 네이티브 Windows 실행과 구분한다.

126차시 Reactor 계약: `python3 scripts/check_learning_reactor_contract.py`.
실제 여러 소켓의 준비성·등록 ID 재사용 방지·배치 중 제거·콜백 수명·깨우기를 검사한다.
현재 Linux 깨우기 회귀: `python3 scripts/check_learning_reactor_wake.py`.

127차시 Linux epoll: `python3 scripts/check_learning_epoll.py`.
공통 Reactor 계약과 실제 커널 LT/ET/ONESHOT·HUP·실패 후 관심 보존을 검사한다.
현재 Linux 깨우기 읽기 회귀: `python3 scripts/check_learning_epoll_drain.py`.

128차시 IOCP: `python3 scripts/check_learning_iocp.py`.
실제 Windows 수신 구현을 API 대역으로 검사하고, STUDY_MINGW_CXX가 있으면 Windows 실행 파일도 교차 빌드한다.
실제 Windows 실행과 대역·교차 빌드는 구분한다. 현재 예약 필드 판정 회귀: `python3 scripts/check_learning_iocp_status.py`.

129차시 타이머: `python3 scripts/check_learning_timers.py`.
고정 시각·기준 모델과 실제 poll/epoll 타이머 루프를 검사한다.
현재/학습 헤더의 할당 실패·넓은 정수 기준 검사: `python3 scripts/check_learning_timer_root.py`.

130차시 오프로드: `python3 scripts/check_learning_offload.py`.
작업/후속의 실행 주체·대기/실행/결과 전체 상한·실제 poll/epoll과 늦은 결과를 검사한다.
고장 주입: `python3 scripts/check_learning_offload_root.py`.
실제 저장 결과 상태 전이 대역: `python3 scripts/check_learning_offload_result.py`.

131차시 상태 머신: `python3 scripts/check_learning_state_machine.py`.
연결별 진행/잔여 입력/인증 후 재개와 실제 룸 중첩 종료를 검사한다.
실제 메서드 고장 대역: `python3 scripts/check_learning_room_lifetime.py`, `python3 scripts/check_learning_auth_state.py`.

132차시 백프레셔: `python3 scripts/check_learning_backpressure.py`.
실제 소켓의 읽기 중지/역방향/재개/정체와 공유 예약/반납을 검사한다.
현재 서버의 실패 주입: `python3 scripts/check_learning_backpressure_root.py`.

133차시 샤딩: `python3 scripts/check_learning_sharding.py`.
제한 우편함·소유권·실패 철회·만기와 실제 두 작업 스레드 전달을 검사한다.
현재 서버 고장 주입: `python3 scripts/check_learning_sharding_root.py`.

134차시 위협 모델: `python3 scripts/check_learning_threat_model.py`.
서버가 연결에 고정한 주체·배치 원자성·거절 뒤 정상 입력·실제 소켓 프로브를 검사한다.

135차시 HTTPS와 게임 연결: `python3 scripts/check_learning_secure_connections.py`.
공개/개발 전송 정책과 실제 HTTPS 인증서·이름·실패 응답을 로컬 시험 서버로 검사한다.
