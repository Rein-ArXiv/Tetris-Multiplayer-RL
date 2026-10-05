# 연속 집필 진행 기록

- 2026-10-05 이미지·글자·UI 단원 최종 대조 완료: 57의 빈 viewport 전역 배처 상태,73의 표시 영역 조회·창 설정 환경 검증을 보완. 설명용 선택 함수의 컴파일/경계/ASan·UBSan·LSan,57/73 집중 DOM,879문제·Part·Markdown·정적 배포 검사 통과. 다음 소리 단원은74의 오래된 예산 설명과76의 공유음악 수명 메모를 보완한다.

- 2026-10-05 시간과 재현성 단원 최종 대조 완료: 44–55의 기존 설명/기준 구현은 유지. Part1의 해시 필드 목록·정수 규칙 시간·공개 API·지원 빌드 전제와 Part4의 논리틱 표현을 바로잡았다. 다음은 이미지·글자·UI이며57/73에 실제 누락 보완 예정. 50fbaeb의 Pages37282630160 및 실제 배포 자산 검증 완료.

- 2026-10-05 보드와 규칙 단원 최종 대조 완료: 25–43 강의·기준 코드에서 새 결함은 찾지 못했다. Part1의 도입/요약과 고스트 사례에서 현재 구조와 맞지 않는 일반화를 교정하고 HTML 원문을 재생성했다. 다음은 시간과 재현성 단원이다. GPU 커밋 7b49b65의 Pages37281580500 및 실제 배포 자산 해시 확인 완료. CI37281580489는 아직 진행 중.

- 2026-10-05 최신 상태: 기본 편성의 집필은 완료됐고 단원별 최종 대조 중이다. “GPU로 첫 도형 그리기” 대조를 마쳤으며 11의 OS 오류 보고와12의 X-매크로 설명을 보완했다. Part2/3의 렌더링 일반화와 API 계약을 교정했다. 다음 단원은 보드와 규칙이다. sol/terra/luna의 분리 검토와 DeepSeek 문장 초안을 책임자가 검수했다. 관련 로컬 검사 및 정적 release `2e16e9849ddefadc` 통과. 상세 근거는 REVIEW_LOG의 2026-10-05 항목.
- 직전 main cbf577d의 Pages 배포와 공개 자산은 검증됐으나 CI37243123290 Windows WSS 통합 검사에서 실패했다. 첫 수신 조각만 읽던 테스트 도구를 프레임 완성까지 누적하도록 수정했고 기존 인증26검사가 통과했다. 이번 단원 커밋·푸시 뒤 원격 Windows 결과를 확인해야 한다. 전체 CI 성공을 선기록하지 않는다.


- 2026-10-04 기초 단원 내용 대조 마감: “프로그램과 창”의 준비·빌드·수명·입력·백엔드 계약을 대조했다. 1/5/7/10 설명과 Part0/2의 오개념을 보완했으며 새 차시는 필요하지 않았다. 기초 Linux 체크포인트, 전체 DOM 및 추가 메모의 집중 DOM, 객관식·Part·Markdown·정적 배포 검사 통과. 최종 정적 release는 `70722b75b1682ab7`. 상세 근거는 REVIEW_LOG의 단원 마감 항목. 아래 “module01 진행 중”은 중간 기록이다. 단원 커밋·푸시 뒤 원격 배포를 확인하고 다음은 GPU 첫 도형 단원의 내용 대조다. 전체 원문 covered 상태를 자동으로 올리지 않는다.


- 2026-10-04 최신 사용자 재개: 남은 범위 확인 후 계속 진행. 아래 예약 중지 기록은 이력이다. 편성된 강의는 모두 reviewed이나 원문 전체 내용 대조는 미완료. module-01부터 준비물·기초 구현을 대조하고 실제 누락/문서 오류만 보완한다. 각 큰 단원의 대조·교정 마감 시 커밋·푸시/Pages 확인. 새 차시 수를 먼저 정하지 않는다.
- 직전 마감 최종 증거: main9ace2e3, CI37239274263 전체 성공, Pages37239274217 성공. Windows38CTest·기본1995pytest/37subtests·리액터단일107검사 및 다중루프 단계 성공. 공개 release1e623d7feffc0e34 실제15자산 해시 일치. out/learning-jobs/module13-deployment-verification.json에 최종 증거가 있으며 이전의 CI 대기 메모보다 우선한다. 기존 :memory:.ses는 제외한다.

- 2026-10-04 사용자 최신 중지 조건: 현재 큰 단원 module13 「배포와 유지보수」(167~177)를 완료·검수·커밋·푸시하고 Pages 배포를 확인한 뒤 goal을 paused로 변경한다. 177까지 로컬 집필·검수를 마쳤으며, 단원 완료 뒤 추가 범위 감사/신규 작업을 자동 시작하지 않는다. 조건 완료 전에는 중지하지 않는다.

- 2026-10-04 최신(progress):169 Linux서버 실행·준비·DB재시작 강의검수완료. 로컬839문제/집중DOM/Part/스니펫/실제SERVICE/릴리스5검사·정적releasec35518537f0bb39c통과. 다음170Windows이전. module13경계에서강의커밋·푸시,goal계속. HTTP/WindowsCI hotfix024b1f7는별도로main푸시했고Pages37227049106/실제자산9b53b953160ad711검증완료. 제품CI37227049100은진행중으로결과추적필요. :memory:.ses제외,모든169외부초안작업완료,기존로컬서버유지.

- 2026-10-04 최신: main30d2eb3과 Pages workflow 공개 확인 완료. Windows CI의429 미소비본문/연결 재사용 오류를 별도 수정했다. 수정 전 Linux400재현, 수정 후 C++경계·실제서버106검사·공개168기준 정적검사 통과. CI hotfix push 뒤 원격Windows 결과 확인 필요.169는 실제CP검사를 마쳤고 사이트검수 진행 중, 단원13 계속.

## 작업 계약

2026-09-21 사용자가 차시 제작 → 검수 → 다음 차시 제작을 턴 단위로 계속 요청했다.
현재 작업에 전체 강의 완성을 위한 목표 모드가 활성화되어 있다. 별도의 반복 알림은 만들지 않는다.
매 턴 이 파일과 AUTHORING.md, RECONSTRUCTION_PLAN.md, course-plan.json,
REVIEW_LOG.md를 확인하고 이미 검토한 차시를 중복 생성하지 않는다.

- 한 차시를 reviewed로 바꾸기 전에 설명·CS·문제/풀이·기준 코드·현재 코드 연결을 검토한다.
- 실제 가능한 빌드/실행/화면 검사를 수행하고, 못 한 OS/GUI 검증을 구분한다.
- 발견한 실제 소스/Part 오류를 수정하면 관련 HTML 묶음과 대응 해시도 갱신한다.
- 각 턴의 완료 지점, 실행 중인 외부 작업, 다음 시작점, 막힌 사항을 아래에 갱신한다.
- 외부 모델은 승인한 선택 자료만 OpenCode/DeepSeek로 전달한다. 키/DB/설정은 제외한다.
- 2026-09-23 사용자 승인: 현재 챕터(시간과 재현성, 44~55차시)를 완성하면 1~55차시를 GitHub Pages로 공개한다. 필요한 커밋·푸시·Pages 설정·배포를 진행하고, 56차시 이후는 로컬에서 이어 간다. DNS·외부 계정 생성은 요청 범위가 아니다.

- 2026-10-03: 매 큰 단원 완료 시 커밋. course-plan의 modules 기준이다. 당시134~148 단원을 다음 경계로 지정했으며, 이후 경계는 아래 현재 상태를 따른다. 개별 차시 커밋/자동 푸시/공개 범위 확대는 하지 않는다. DeepSeek 선택 초안과 직접 검수 분담 유지.

## 현재 상태

- 2026-10-04 재개첫대조완료: Part0준비/최소빌드와관련기초강의를대조해1의도구준비·5의의존경계메모·10의SDL탐색설명보완. Part0의의존성/플랫폼/엔진/옵션단정과Part2의마우스래치·큐용량·규칙좌표오개념교정. 직접근거covered14,전체대조미완료. DeepSeek검토/JS초안종료. 새콘솔빌드·헤더경계반례·학습30입력/수명·879문제·전체177DOM·Part/Markdown검사통과. 첫정적검사는통과했으나최종coverage갱신후lessons.js재생성누락을검사가검출했다. 생성물을순서대로갱신한뒤정적검사를다시확인한다. module01대조가진행중이므로아직커밋/푸시없음. 다음은Part2자원소유/콜백/입력/종료·백엔드계약을기초차시와대조하며,원문소개/요약이라는이유만으로covered로바꾸지않는다. 새강의추가가필요한지는실제누락을확인한뒤결정. 기존 :memory:.ses제외.

- 2026-10-04 추가마감: e0b2255/Pages37237579116·실자산1e623d7feffc0e34확인. CI37237579127은Windows기본통합중meta의PORT공지5초기한1오류(1990passed/37subtests). 테스트helper총준비기한30초로교정,가상기한4/페어링4/실제HTTP3검사통과. 제품제한이나skip/retry변경없음. 수정커밋의원격결과확인후paused,새집필없음.

- 2026-10-04 단원 공개4431898/Pages37235561503·실자산0dc17f28a8faad2f 확인. 제품CI37235561549는Windows기본통합까지통과했으나리액터페어링로그관찰1실패. 실제paired로그존재,동일시계값의새로그를놓칠수있는엄격대소비교를목록커서로교정하고4회귀/실제백프레셔검사통과. 최종수정푸시후원격검사확인만남았으며그뒤goal paused. 새집필없음.

- 2026-10-04 단원 마감: module13의177까지 reviewed. 종합 체크포인트의 실제 RULES/ARCHITECTURE/CONTENT/SHUTDOWN 8CTest·SERVICE HTTP복구·증거계약 통과. 제품 전체 빌드·38CTest·관련177pytest/37subtests, 단원167~177 스니펫·전체177차시DOM·879객관식·Part/Markdown·정적release0dc17f28a8faad2f 통과. 미래 문제를177로 고정했던 테스트를 현재 목록에서 계산하도록 교정했다. 모든 집필/DeepSeek 작업 종료. 다음은 main 단원 커밋·푸시와 해당 리비전의 CI(Windows 포함)/Pages 실자산 확인만이며, 그 뒤 사용자 요청대로 goal paused. 원격 결과는 out/learning-jobs/module13-deployment-verification.json에 별도 보관한다. 전체 원문 coverage는 여전히 부분적이므로 전체 goal 완료로 표시하지 않는다. :memory:.ses 제외, 기존 읽기 서버 유지.

- 2026-10-04 최신(progress):176 콘텐츠확장 완료. 다음177종합복습후단원마감. 실제CONTENT/표현불변/정책입력/참조ZIP검사·제품패키징/프로필37검사37subtests(skip없음)·보상권한2HTTP통과. 설정외부링크·재독불일치·부분ZIP/게시경쟁·dot경로교정. 인라인7·874객관식·집중DOM·Part·정적release5127b82a1022b661통과. 모든176작업종료,177새파일/외부작업없음.177완료·단원검사·main커밋·푸시·원격WindowsCI/Pages확인후goal paused;전체coverage추가감사는시작하지않음. :memory:.ses제외.

- 2026-10-04 최신(progress):175 변경경계/SOLID 완료. 다음176 콘텐츠추가,177복습뒤단원마감. 누적화면/봇Character namespace충돌을실제재현후분리;ARCHITECTURE·표현·설정3CTest와widget구문통과. Part13 relay서버재현누락교정. 인라인7·869객관식·집중DOM·Part/Markdown·정적release5a5dc72d27d988b9통과. 모든175작업종료,176새파일/외부작업없음.177완료·단원커밋·푸시·원격WindowsCI/Pages확인후goal paused. :memory:.ses제외.

- 2026-10-04 최신(progress):174 릴리스 검사 완료. 다음175 아키텍처/변경위치/SOLID,176콘텐츠추가,177복습. 실제 RULES/SERVICE·계정복구·선언반례 통과; 인라인9·864객관식·집중DOM·Part·정적release45820fec0a4bce3f 통과. DeepSeek 평가기·문제 초안 검수완료,모든174작업종료.175새파일/외부작업없음.177단원완료후커밋·푸시·원격WindowsCI/Pages확인후paused조건유지. :memory:.ses제외.

- 2026-10-04 최신(progress):173 백업/복구 완료. 다음174 릴리스 검사. 실제 CP/제품 키회전 전후 복원과 백업24검사·859객관식·집중DOM·Part·인라인9·정적releasee0ad285306815371 통과. 백업 도구의 링크/sidecar/외래키/기한/게시/보존 오류와 Part의 옛WAL 혼합 복구 순서 교정. 모든173 작업 종료,174 새파일/외부작업 없음.177까지 끝내고 단원 커밋·푸시·CI/Pages 확인 뒤 goal paused; 중간 차시에서 중지하지 않음. :memory:.ses 제외.

- 2026-10-04 최신(progress):172 종료/SIGPIPE/소켓소유/워커정리 제작·검수완료. 다음173 백업과복구. 실제논블로킹설정실패가종료를막는문제를재현하고제품relay/reactor/Session교정. 로컬854문제·집중DOM·Part·인라인7·실제신호/루프백/워커/제품3CTest·활성경기종료·정적releasebfe4afcd171fdea6 통과. 모든172작업종료,173새파일/외부작업없음. 현재제품수정본빌드는out/ci-http-fix(CMAKE_HOME_DIRECTORY=ROOT);out/ci-repair/build는과거snapshot이므로수정본증거로사용하지말것. 원격Windows기동2오류·170Unicode의실제Windows검증은단원푸시뒤확인필요.177까지완료/검수/커밋/푸시/Pages확인후goal paused. :memory:.ses제외·기존읽기서버유지.

- 2026-10-04 최신(progress):171 비공개 파일 게시/내구성 강의 제작·검수 완료. 다음172 종료/SIGPIPE/소켓소유/워커정리. 로컬849문제·집중DOM·Part·인라인5·학습/제품 실제writer 실패주입·정적release5f24fff9297e85a8 통과. 모든171 외부/검사 종료,172파일/외부작업 없음. CI port0/기동진단 보강은 로컬109검사 통과했으나 원격Windows기동2오류의 원인확정/해결증거는 아직 없음. 170Unicode와함께단원푸시확인필요. module13의177까지완료·검수·커밋·푸시·Pages확인후goal paused로중지. :memory:.ses제외,기존읽기서버유지.

- 2026-10-04 진행: CI37227049100 최종 Windows 서버 실패. 기존429/연결재사용 회귀는 통과, 다른 OS/클라이언트/학습 성공. Windows 통합검사2건은 meta 기동 fixture에서 종료 원인 없이 실패하여 원인 미확정. 제품 meta 포트0 공지와 실패 stderr 진단을 추가하고 부모 포트 예약 경쟁을 제거했다. 실제 로컬HTTP/보안109검사 통과(ci-meta-fixture-tests.log). 해당 변경과170 Unicode 코드는 아직 로컬이며 다음 단원 푸시로 Windows 결과 확인.171 집필 진행 중;177까지 완료 후 중지 조건 유지.

- 2026-10-04:170 Windows이전 제작·검수완료(progress). 다음171 비공개파일/원자교체. main024b1f7의CI37227049100은Windows서버thread-model smoke중(나머지성공),170Unicode변환코드는아직로컬이며단원푸시뒤Windows네이티브회귀확인필요. 로컬844문제·집중DOM·실제SERVICE/UnicodeDB재시작·meta50HTTP·정적release320944393f8fc299통과. 단원13의177까지끝내고커밋·푸시·Pages확인후goal paused로중지하라는최신지시유지. 외부초안/로컬검사모두종료,기존서버유지, :memory:.ses제외.

- main/Pages 통합 완료: **30d2eb38e77ad0ff1f07f448d8df7cf09aca8432**를main/origin에커밋·푸시했다. **Learning site37225195663 build/deploy성공**, Pages Sourceworkflow전환확인. 실제HTTPS에서release8956e73123f7a264의manifest와15자산해시가로컬과일치하며lesson-168포함확인(out/learning-jobs/main-pages-live-verification.json). 현재검수강의는공개됨. 다음큰단원main푸시도사이트검사후자동배포. 제품CI최신은**37225195676 (30d2eb3)**이고 Windows순차빌드수정의직전검사37224758450도진행중이라둘의실패로그를먼저확인한다. 전체제품CI통과는아직주장하지않음. 이메모는배포후운영기록으로다음작업커밋에포함한다.

- 2026-10-04 최신 요청: main과 Pages 통합. 개발은 이미main이며166까지 원격 반영돼 있음을 확인했다. gh-pages는 오래된 정적 배포 브랜치다. 검수된167~168·관련 제품/Part 개선을main에 함께 커밋하고, Pages Source를GitHub Actions로 전환해 main의 학습사이트 검사 성공 후 최신 reviewed강의를 공개한다. 이 요청이 과거공개1~55동결보다 우선한다. gh-pages 이력과알수없는 :memory:.ses는보존. 작업결과는 Learning site배포 및 실제HTTPS manifest/hash로 확인한다.

- 2026-10-04 후속 요청: 반복 CI 실패 알림 우선 교정 중. b0e7196·40151af·54d4867의 코드 수정으로 원격 run37223680416에서 Linux/macOS 서버·세 OS 클라이언트·CPU 학습·Learning site가 통과했다. Windows 서버도 모든 실행 파일의 컴파일/링크는 통과했지만, 병렬 vcpkg z-applocal DLL 배치가 파일 잠금(code32)으로 실패했다. 6b30039에서 Windows CI 빌드만 순차 실행으로 수정·푸시했다. **현재 확인할 최신 CI는 run37224758450 (HEAD6b30039), Learning site37224758496**. Windows 서버 CTest/실제 통신 단계는 아직 완료 증거가 없으므로 CI 전체 해결을 선언하지 않는다. 원격 결과를 먼저 확인하고 실패가 남으면 로그에 따라 수정한다. 기존 :memory:.ses 제외,167~168 강의 변경 로컬 유지, 공개 Pages배포 없음.

- 2026-10-04: 최신 사용자 요청으로 **goal까지 중지하지 않고 큰 단원마다 검수·커밋·푸시 후 계속**한다. 과거 예약 중지/자동 푸시 없음 규칙은 이력이다. 공개 범위는 위의 최신 main/Pages 통합 요청을 따른다.
- 167차시 빌드 타깃과168 의존성/설치 이동 제작·검수 완료(reviewed). 다음169 Linux 서버 실행 순서와 포트 경계. module13 전체가 다음 커밋 경계다.167의 누적 실제 역할 빌드·계정HTTP·바인딩·ONNX·스니펫·DOM·객관식/정적 배포 검사 통과. 상세 증거는 REVIEW_LOG의167항목.
- 직전 단원4457e34를 origin/main에 푸시했다. 원격 학습 사이트 검사는 성공;제품CI는 선택Torch테스트 수집과 Windows max매크로 충돌로 실패했다. 두 원인을 로컬 교정하고 학습전용CPU CI를 추가했다. 실제 학습1919검사·37subtests/skip없음, 서버용Torch없는환경 수집 통과. 수정후 원격CI는 단원푸시 뒤 확인한다.
- 168에서 실제 ONNX SDK·IMPORTED 타깃·Linux 설치 이동·621프레임 모델 대조, 필수 Runtime 누락 실패를 확인했다. root SDK 경로를 공유하고 미지원 CPU/필수 복사 실패를 교정했다. 최종 정적release8af8eccfb056bef1의 재현 ZIP·집중DOM/834객관식·최종HTTP 소스 해시까지 검증 완료. 모든168 외부/검증 작업 종료.169 원고/체크포인트는 아직 생성 전이며 out/learning-jobs/169-authoring-notes.txt에 준비한 명세·선택 DeepSeek 초안·로컬 검증을 기록했다. REVIEW_LOG 마지막 CI 항목부터 이어간다. Windows수정은 소스검토·Linux컴파일만 완료, Windows 성공을 주장하지 않는다. 외부ORT1.18.1의128byte LSan한계 유지.
- 기존 미추적 :memory:.ses는 제외한다. 외부초안/검증은 out/learning-jobs에 보존. 기존 로컬읽기서버 유지;수동GUI검사 없음.

## 직전 단원 마감 — 166차시

- 2026-10-04: **module-12 학습하는 봇(149~166) 제작·마감 검수 완료**. 단원 변경을 커밋하고 사용자 요청대로 일시 중지한다. 다음 시작점은167이며 재개 요청 전 작업하지 않는다. 공개1~55 동결, 푸시·배포 없음.
- 최종166과 단원 마감 근거는 REVIEW_LOG.md의166차시 항목에 정리했다. root 전체35CTest, CP166 누적21CTest 및 실제 ONNX621프레임, 메타 서버13HTTP검사, fresh native 기반 Python1898검사·37subtests, 전체 차시 DOM·824객관식·단원 전체 스니펫·Part 발췌·대응표 검사 통과.
- 정적 released08e0892cffd8487의16파일·허용 목록·상대 자산·해시·재현 ZIP 검증 통과. 로컬18767의 최종 생성 자료와 현재 코드 해시가 일치한다. 단원 검증/외부 모델 작업은 모두 종료했으며 기존 로컬 읽기 서버31110만 유지한다.
- 순수 새 C++ 코드의 ASan/UBSan/LSan 통과와 외부 ORT1.18.1의128byte LSan 미해결 사항을 구분한다. 대상 Windows/macOS 실제 실행·GUI·장기 학습 성능을 이번 검사로 검증했다고 주장하지 않는다.
- 출처 불명 기존 미추적 :memory:.ses는 커밋에서 제외한다. 후속167의 파일·초안·외부 호출은 만들지 않았다. 다시 진행할 때 AUTHORING의 집필/DeepSeek 범위, REVIEW_LOG의 미검증 경계, course-plan과 coverage의 partial 상태를 유지한다.

## 직전 완료 기록 — 165차시

- 2026-10-04: **165차시 캐릭터 제작·검수 완료(progress)**. 직전164는 실행기/강의/검사 실제 변경의 progress. 로컬1~165·819문제. 다음166 실패/fallback·보상 경계 및 module12(149~166) 마감 감사·커밋·goal paused. 다음 단원은 재개 요청 전 시작하지 않는다. 공개1~55 동결·푸시 없음.
- 실제 불일치 발견: 게임은 줄 끝 # 주석을 허용하지만 package_opponents는 숫자 일부로 읽어 거절. Colab 등록 셀은 이름 안 #를 허용해 런타임 행이 잘림. 수정 전 실제 패키징 거절/등록 검사 통과를 재현. 새 Python codec을 등록/패키징이 공유하고 C++ 명시 프로필 파서를 순수 함수로 추출. BOM·주석·ASCII 공백·숫자 전체·UTF-8 이름 바이트 길이·제어문자 계약을 대조. Python splitlines가 Unicode 줄 구분 문자까지 잘라 C++ getline과 달라지는 지점도 LF 기준으로 교정.
- DeepSeek에 순수 profile codec 명세만 보냄. 최초 경로 오류로 파일 전송 전 종료, 절대 경로로 수정 후21994 exit0. 165-profile-material.txt/events.jsonl/draft.json 보존. 응답 앞 설명을 제외해 JSON 추출. 환경에 tools deny/프롬프트 도구 금지를 지정했지만 이벤트에는 /tmp/opencode의 생성 코드·테스트 write2회/shell2회가 실행됨. 요청 명세로 만든 임시 파일만 읽고 실행했음을 이벤트에서 확인; 저장소/비밀/설정 읽기나 전송 없음. 도구 차단이 강제됐다고 주장하지 않는다. 반환 Python을 직접 검토하고 실제 별도 검사를 수행했다.
- root bot/opponent_profile.h에 무시할 행/오류/유효값 분리. discover_opponents는 기존 부분 수용·명시 항목 우선·legacy/자동발견 정책을 유지하고 행 번호/고정 오류로 진단. 필드 안 제어문자·명시 프로필 부호 있는 숫자는 거절. Windows 편집기의 BOM을 ID에서 제거. 모델/경로 안전성이나 UTF-8 전체 유효성 인증 기능으로 확대하지 않음. src/main.cpp의 오래된 모델 수/로스터 주석 교정, UI 동작은 그대로.
- root bot_controller/bot_replay2CTest 통과(165-root.log,56391 exit0). 실제 C++ parser dump와 Python 행 계약/주석/BOM/한국어 바이트/범위/제어문자/Unicode separator 대조. 패키징 허용 경로·누락·기존 ZIP 보존 포함24검사/37subtests 통과. Colab 기존 계약까지42검사/37subtests 통과(165-python-final.log). CMake 새 타깃은 재구성 전 No rule을 확인 후 기존 옵션 보존 재구성·빌드·24검사 통과(165-parser-target.log,35154 exit0).
- CP165는164 누적 파일을 README·bindings/CMakeLists·inference/CMakeLists 외 보존. Character{id,Appearance,Behavior}, Catalog의 전체 후보 검사·ID 중복 거절·값 복사 선택 추가. 같은 모델 공유·표시 독립성·선택 후 재로딩·실패 시 기존 목록 보존 검사. current root의 부분 수용/legacy 탐색과 strict CP의 차이를 본문에 명시한다.
- 초기 검사 스크립트의 CP 경로가164를 가리켜 기존19CTest 후 새 실행파일 없음으로 실패(165-cp-initial.log,59335 exit1). 경로를165로 교정하고 이 작업이 만든 잘못된 build 폴더만 제거해 fresh 구성. 누적20CTest 전부 통과·실제 모델443프레임 Python/native 전이 대조(165-cp.log,41018 exit0). 이름/그림만 다른 두 캐릭터의 실제 입력/상태 동일, Pacing 변경 fixture의 기록 차이를 확인. 콘솔은 이미지 경로만 출력하고 실제 GUI 이미지를 그렸다고 주장하지 않는다.
- 새 Catalog/Driver 연결과 실제 C++ parser의 ASan/UBSan/LSan 통과(55377 compile exit0, 격리 밖character와24검사 실행 exit0). 외부 ORT를 포함하지 않는 메모리 검사이며162의 SDK1.18.1 128byte 누수 한계 유지.
- 원고13절·5문제·인라인8: 안정적 ID/표시 이름·키/중복·구성·구문/의미·쓰기/읽기의 특수문자·바이트/글자·숫자 변환·후보/실패 보존·값 스냅샷과 파일 내용·이미지 캐시·실제 모델 공유·서버 카탈로그 권위. Part15§3을 실제 파서/선택/표시와 연결하고 해당 절만 partial 대응/해시 갱신. bots-and-colab의 고정 속도/오래된 비교를 설정 출처/시간 의미로 바꾸고 등록/패키징 문법을 동기화.
- 819객관식·lazy·Mermaid11·Markdown27·Part/coverage·인라인/현재 심볼/누적보존·diff 통과. 집중165 DOM 및 전체 메뉴/공통 저장/뷰어/file URL 계약 통과(165-navigation.log,36022 exit0). 수동 GUI 없음·공통UI 수정 없음.166 마감에서 전체 DOM 수행 예정.
- 정적 release66f96b2b7fefde62·16파일·허용목록/상대자산/해시/재현ZIP 통과(165-site.log,89988 exit0). 현재 자료28문서·246소스. 로컬18767 생성자산3개와현재소스4개 bytes/해시 일치(165-http.log). 모든165 외부/검증 작업 종료, 기존로컬서버31110만유지. :memory:.ses 기존 출처 불명 파일은 단원커밋에서 제외할 것.
- **다음166**: src/main.cpp BotSingle picker와 meta/bot_challenges.cpp claim picker의 `ok || fallback_placement`가 정책 실패를 조용히 성공처럼 만든다. bot/reward_replay.h verify_victory도 Controller no_decision/invalid_target 등을 보상 적격성과 연결하지 않는다. 실패 provenance/연습 전환·명시 UI/서버 실패 시 BP 거절·정상 결정론 fixture 유지·티켓/모델 버전 경계를 실제 코드와 검사로 해결할 것. 클라이언트 모델 실패와 서버 카탈로그 mismatch/모델 교체 가능성도 범위를 명시한다. 단원 마감에는 전체 DOM·누적 핵심 회귀·Part 대응·변경 출처/비밀파일 감사 후 단원 커밋, goal paused. 새166 파일/외부 작업 없음.

- 최종 release9646f856a2be0d74·16파일·허용목록/상대자산/해시/재현ZIP 통과(164-site-final.log,48988 exit0). 현재 자료28문서·244소스. 최종 Part/coverage/인라인/diff 통과(2363). 로컬18767 생성자산3개·현재소스4개 HTTP bytes/해시 일치(164-http-final.log). 모든164 외부/검증 작업 종료, 기존 로컬서버31110만 유지. :memory:.ses는 출처 불명 기존 미추적 파일로 이번 작업 포함하지 않음.

- 2026-10-04: **164차시 속도 조절 제작·검수 완료(progress)**. 로컬1~164·814문제. 다음165 캐릭터. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 유지. 개별 커밋·푸시·공개1~55 확대 없음.
- 전달 리뷰를 현재 파일과 재대조했다. library/Mermaid 요청 시 로딩·SDL_MAIN_HANDLED/SetMainReady·SDL2 Config 우선·noscript·모바일 공백·동적 Part/주제 수·그래픽 지도·절 제목 변경 유지. lazy-assets 회귀 통과. 첫 로딩1/3과 macOS 전반 검증 단정은 채택하지 않으며 반복 선수 차시 번호를 복원하지 않는다.
- DeepSeek deepseek/deepseek-flash에 순수 틱 게이트 명세만 전달(/tmp tools deny,21315 exit0). 164-gate-material.txt/events.jsonl/draft.json 보존. custom clamp를 std::clamp로 정리하고 primary가 Controller/실제 모델 프레임 연결·원고·교재·검사를 통합. 선택 자료 외 비밀/설정/전체저장소 전송·Codex 하위 에이전트 없음.
- **현재 코드**: Pacing에 기본값/허용 범위를 모으고 TickGate가 think·interval·minimum을 관리. age를 max(think,minimum)에서 포화시켜 무한 증가 제거. Controller/캐릭터 로더가 같은 출처를 사용한다. 캐릭터 행은 범위 오류를 거절, Controller.reset은 clamp, 실습 Driver는 직접 설정 오류도 거절한다. 이 서로 다른 경계를 원고에서 교정했다.
- 수정 전163 작업본 Controller와 수정 후 입력·Status·DiagnosticStateHashV2를 실제 게임13438틱 대조하여 일치(164-root-parity.log,30524 exit0). root bot_pacing/input_route/bot_controller/bot_replay4CTest 통과(164-root.log,66467 exit0). 기본 정책의 난이도를 문구 수정에 맞춰 바꾸지 않았다.
- CP164는163 누적 파일을 README·bindings/CMakeLists·Session C++ pose 조회·inference/CMakeLists 외 보존. TickGate/Driver가 같은 Session과 ONNX를 연결한다. 모든 실제 틱을 소유하고 NONE도 step으로 진행, 목표 유지·현재 pose에서 다음 조작 선택·자연 잠금 초기화·실패 재시도 간격·조작 효과 검사. 외부 변경은 reset 요구. 후보 복사/예외 없는 이동으로 Session/Driver 내부 상태만 원자 반영하며 콜백 외부 부작용은 보존 범위 밖이다.
- 누적 Release native19CTest·demo 통과(164-cp.log,4362 exit0). 예외 테스트의 catch가 자신의 require 실패까지 잡던 fixture를 구체적 오류 확인으로 교정, 새 계약 재실행 및 실제 CPU ONNX 통합 통과(164-paced-final.log,91597 exit0). 실제 TrainingRun/export 뒤881프레임·51결정에서 Python 모델 선택·native 전체 상태 bytes·반복 C++ 기록 대조. 장기 모델 성능이나 대상 OS 실행으로 확대하지 않음.
- 새 게이트와 CP Driver ASan/UBSan/LSan 통과(9829 compile exit0 및 격리 밖 실행 exit0). 외부 ORT를 포함하지 않은 검사다. 기존 ORT1.18.1 Env/Session의128byte 누수 한계는 여전히 미해결이며 전체 SDK 누수 통과로 기록하지 않는다.
- 원고14절·5문제·인라인9. 벽시계/틱/렌더·첫 호출0의 max(S,M−1)·interval−1 대기·포화·빈 입력 중력·목표 보존·실제 효과·단일 소유·실패 보존·분포 변화 설명. 최소 시간은 자발 드롭만 제한. 한 이동이면 예제틱2/7, 두 조작이면2/5/8을 구분했다. CP strict route와 paced current-pose 방식·현재 Controller 호출 책임의 차이를 명시.
- Part9§13 Controller/Opponent와 시간 조건 재구성. Part15§3/4/5 현재 발췌·시간 설정 출처·스키마 상수·로컬 학습 불가능처럼 읽힐 문장 교정. bots-and-colab의 집필 환경 보고 제거. 검토한4절만 partial 대응/해시 갱신, 전체 캐릭터/보상/배포 완료로 확대하지 않음.
- 814객관식·lazy자산·Mermaid11·Markdown27·Part 발췌/coverage·인라인/현재 심볼/누적보존·diff 통과(53710/9527). 집중164 DOM 및 공통 탐색·저장·뷰어·file URL 계약 통과(164-navigation.log,20075 exit0). 수동 브라우저 없음, 공통UI 변경 없음.166 단원 마감에 전체 DOM 예정. 원고의 설정 오류 처리 문장 최종 교정 후 정적 자산 재생성.
- **다음165 감사 지점**: bot/opponents의 id/name/model/icon/portrait/difficulty/time 분리·중복/잘못된 행·legacy 자동발견·리소스 상대 경로·클라이언트 표시와 서버 신뢰 경계. 현재 로더는 범위 밖 캐릭터 행을 거절하고 legacy 잘못된 수치는 기본값 유지. root 표시/UI와 서버 보상용 프로필은 같은 것으로 가정하지 말고 실제 참조를 추적한다.164 실행기의 단일보드 소유와 두보드 서비스 연결을 구분,166의 명시 실패/fallback/보상 정책까지 마무리한 뒤 커밋·중지. 새165 작업 없음.

- 2026-10-04: **163차시 입력 전개 제작·검수 완료(progress)**. 직전162 턴은 실제 wrapper/실습/강의 교정과 실행 증거를 만든 progress. 로컬1~163·809문제, 다음164 속도 조절. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 계약 유지. 개별 커밋·푸시·공개1~55 확대 없음.
- DeepSeek deepseek/deepseek-flash로 Python 입력 검증 helper 명세만 전달(/tmp,tools deny,50918 exit0). 163-expansion-material.txt/events.jsonl/draft.json 보존. Integral/bool 거절·검사 후 int 정규화·명령 범위를 검토해 netbot/expansion_contract에 적용. primary가 실제 Controller/경로 실행기/문서/검사 통합. 키/계정/DB/설정/전체저장소 전송과 Codex 하위 에이전트 없음.
- **현재 코드 교정**: expand_placement가 현재 원점·목표 열·회전을 뺄셈/할당 전에 제한. 음수 원점과 정책 목표 도메인 구별. Python bool/소수/문자열/이질 회전 규약 거절·NumPy Integral 지원. Controller가 picker 출력 초기화/범위 검사, 회전·이동을 값 복사본에 사전 적용해 효과 확인, 드롭 전 실제 원점/회전과 보관 목표 비교. blocked/target_lost면 큐를 버림. status에 waiting/input/no_decision/invalid_target/blocked/target_lost/finished 구별. 원본 변경 없음·실제 Tick 중복 없음. 아직 상위 UI/보상은 상태를 모두 소비하지 않으며166에서 연결할 범위로 남김.
- **오류 재현과 회귀**: 동일 벽 fixture에 기존 HEAD Controller는10틱 안에 도달 불가 목표를 향한 다른 위치의 드롭2회, 수정 후0회(163-before-after.log,48472). 현재 headless root4CTest(input_route/placement_contract/bot_controller/bot_replay) 전부 통과(163-root.log,70635). 잘못된 목표/blocked/외부이동 후target_lost·picker 실패·원본 해시 보존 검사. 실제 승리 리플레이 회귀가 그대로 통과하여 fixture를 성공하도록 바꾸지 않음.
- C++ action_codec_dump와 Python 전체 입력 표 직접 대조·손계산 사례·타입/도메인 회귀1649통과(163-python-final.log,29120). 가짜 native 대신 실제 C++ dump 사용, skip 없음. Python 참조 테스트의 오래된 “실제 C++ 대조 없음/byte-for-byte 동일” docstring 교정.
- CP163은162의 README·bindings/CMakeLists 외 누적 파일 유지. 새 bot/route_player와route_demo/route_contract 추가. plan의 연속 틱 예측 상태를 canonical bytes로 저장하고 start→issue→실제tick→observe. 중복 발행/확인 금지·실제 전이 확인 후 cursor 전진·최종 확인 뒤 complete·외부 변화 stale·취소/재계획을 분리. 모델은162 Session 경로를 유지하며 route_demo는 Round에서 첫 합법 목표를 골라 실행 순서를 관찰한다. 현재 Controller의 간격 중 자연 중력 허용과 CP의 엄격한 연속 틱 가정을 구별한다.
- 독립 Release 확장·누적 CTest17·새 route_demo 통과(163-cp.log,93043). ASan/UBSan/LSan의 CP 실제8845경로·40201입력확인 및 벽·숨은 중력 단계·미적용/다른틱·재시작 통과(163-route-sanitized.log,58095). 현재 helper/Controller와SimGame도ASan/UBSan/LSan통과(163-current-sanitized.log). 초기 sanitizer compile이 존재하지 않는 sim_block.cpp 등 파일명 추측으로 실패하여 실제 CMake source 목록(sim_game.cpp/position.cpp)으로 교정(49883→6724); 제품 코드 실패로 기록하지 않음.
- 원고14절·5문제·인라인8. 끝점/도달경로·명령/효과·원점/지역좌표·나머지·범위와할당·값 복사·상태 기계·발행/확인·숨은 규칙상태·복사 비용을 설명. Part8§12.4~6/Part9§12 재구성·Controller 발췌 동기화·SimGame 헤더의 경로처럼 읽힐 설명 교정. “회전 먼저면 안전”, “회전이 싸서 선택”, “UndoRotation은 private”, “직접 교차 패리티가 없다” 등의 단정 삭제/교정. framing/속도/전체보상까지 대응 완료로 확대하지 않은partial 해시 갱신.
- Part 발췌·인라인/현재심볼/누적보존·coverage·diff 통과. 809객관식·lazy자산·Mermaid11·Markdown27 통과(30019). 집중163 DOM과 공통탐색·답안/완료/저장·소스/정적/fileURL계약 통과(163-navigation.log,3929). 공통UI 변화·수동브라우저 없음. 공통 전체 DOM은166 마감에서 수행. 최종 생성/HTTP는 아래 기록.
- 최종 release6aeaa6c11ae80bfd·16파일·허용목록/상대자산/해시/재현ZIP 통과(163-site-final.log,49435 exit0). 현재 자료28문서·243소스. Part/coverage/diff 최종 통과(27799). 로컬18767 생성자산3개·현재소스5개의HTTP해시일치(163-http-final.log). 모든163 외부/검증 작업 종료; 기존로컬서버31110만유지.
- **다음164**: thinkTicks/inputIntervalTicks/minPieceTicks의 첫 틱/경계/오프바이원·cooldown·age saturation·실제 중력 스폰/오래된 큐/속도와 추론 횟수를 감사. CP의 연속 틱 계획에 실제 대기를 넣는 경우 예측을 다시 설계하고, 기존 Round/Session 및 ONNX 정책과 프레임 실행 연결 범위를 명시할 것. 실패 상태를166 UI·연습/보상 정책으로 소비하는 계약을 잊지 않는다. 새164 파일/작업 없음. 165캐릭터/166fallback 뒤 커밋·중지, 외부 ORT1.18.1 LSan128byte 미해결 기록 유지.

- 2026-10-04: **162차시 C++ 추론 제작·검수 완료(progress)**. 로컬1~162·804문제. 다음163 입력 전개. module12(149~166) 완료→범위 감사→단원 커밋→goal paused 요청 유지. 개별 커밋·푸시·공개1~55 확대 없음.
- 리뷰 재대조: library/Mermaid 지연 로딩, SDL_MAIN_HANDLED·SetMainReady 및 Config 우선, noscript, 가변 개수·그래픽 지도·절 라벨·모바일 공백/CSS는 기존 교정 유지. lazy-assets 자동 검사 재통과. 첫 로딩1/3 단정·macOS 완전 검증 주장은 채택하지 않음. 반복 차시 선수 언급을 되돌려 넣지 않았다.
- DeepSeek deepseek/deepseek-flash에 순수 C++ 유한/합법 최댓값 helper와 작은 검사 명세만 전달(/tmp,tools deny,57362 exit0). 162-choice-material.txt/events.jsonl/draft.json 보존. primary가 소유권/API·실제 실행·실패 경계·강의·Part를 통합. 키/DB/설정/전체 저장소 전송·Codex 하위 에이전트 없음.
- **현재 코드 교정**: BotOnnx 생성자의 ORT 준비를 Load 예외 경계로 이동. 로드 성공 시 err_out 정리. Infer의 텐서 준비·Run·검사 예외를 하나의 외부 경계에서 처리하고 col/rot은 성공 후 반영. 양 출력의 exact shape/dtype·value finite·모든 logit finite 확인. NaN/±inf/빈 합법 집합에서 내부 fallback 성공으로 위장하지 않고 false. 호출부 게임/서버 fallback 결합은166의 정책 범위로 남겨 두며 보상 계약이 이번에 완성됐다고 주장하지 않음.
- 수정 전 HEAD wrapper를 같은 실제 SDK에 빌드하여4비유한 fixture의 추론 성공과 이전 오류 메시지 잔존 재현. 수정 후 모두 실패/빈 오류로 교정(162-before-after.log,87294 exit0). root headless build 및 실제 ORT contract·순수 helper·스텁 빌드 통과. 현재 실제 DDQN export를 C++ BotOnnx로 실행하고 원래 PyTorch와 실제 native 게임24결정 선택 일치(162-root-actual.log,37284 exit0). 기존159 새 확장 선행 import 및경로 확인, 장기 성능 근거로 확대하지 않음.
- CP162는161 누적 파일을 README 외 보존. bot/onnx_policy·policy_choice·inference CMake·probe·계약검사 추가. Env/Session 역순 파괴·PImpl·입력 배열 소유와 래퍼 빌림·동기 Run·출력 소유/복사·오류 상태/선택·컴파일/링크/로더 단계를15절·5문제·인라인8로 구성. 의미 metadata 인증이나 비신뢰 모델 sandbox 기능을 주장하지 않음.
- CP 독립 Release 확장·누적 CTest16·새 선택 helper 검사·실제 C++ ORT1.18.1 대조 통과(162-cp.log,95757 exit0). 실제 TrainingRun을 저장/export하여3시드18관측의 PyTorch float32 출력·합법 argmax·순차 상태 비교, 최대 절대 오차1.4901161193847656e-08. 잘못된 shape·NaN/±inf/value NaN·반복 출력 독립·상태 bytes 보존·로드 실패/재시도·오류 정리 검사. 신규 C++ 경고 없음.
- **메모리 검사 한계**: 격리 실행의 ptrace 때문에 첫 LSan 실패(162-sanitized.log,78375). 격리 밖 동일 검사에서 외부 ORT1.18.1 라이브러리 내부 calloc 두 곳의128byte 누수 보고(162-sanitized-final.log,18451 exit1). 새 wrapper 없이 Env/Session 생성·파괴만 하는 최소 프로그램에서도 동일128byte 재현(162-vendor-leak.log). SDK 교체나 광범위 누수 억제를 적용하지 않았다. detect_leaks=0으로 주소/UB 검사와 실제18관측/실패 대조는 통과(162-address-undefined.log,99538 exit0); 순수 helper는 기본ASan/UBSan/LSan도 통과. 외부 Runtime은 비계측이며 전체 누수 검사를 통과했다고 기록하지 않음.
- Part9§10을 실제 코드/소유권/실패 계약으로 재구성. MemoryInfo가 입력 배열을 할당·소유한다는 오해, shape를 데이터와 같은 방식으로 계속 빌린다는 서술, 출력 count만 검사/내부 fallback/무조건 예외 없음 설명 교정. §9의 GAME=OFF이면 BOT 아무 효과 없음·SDK 누락과 런타임 실패 동일 취급도 교정. §9/10 partial 대응·해시 갱신. 후속 fallback/보상 정책은 별도.
- Part·coverage drift·Markdown27·804객관식·lazy·Mermaid11·집중162 DOM/공통탐색/답안/저장/뷰어·인라인/누적보존·diff 통과. 수동 브라우저 없음. 공통UI는 바꾸지 않음;166 경계에서 전체 DOM 수행. 최종 생성물과 HTTP는 아래 최종 기록을 따른다.
- 최종release aade915a8ac9a1cc·16파일·상대 경로/허용목록/해시/재현 ZIP 통과(162-site-final.log,30529 exit0). 마지막 명시 include/fixture 사용 안내 후 root 계약 재통과(42354 exit0), Part/인라인/coverage/diff 재통과(40052 exit0). 로컬18767 자산3개·현재C++ 소스4개 해시 일치(162-http-final.log). 현재 자료242소스/28문서. 162 외부 작업·검증 작업 모두 종료, 기존 로컬 서버31110만 유지. :memory:.ses는 이번162 작업 전부터 있던 미추적 파일이므로 단원 커밋 감사까지 포함하지 않는다.
- 다음163: bot/controller.h·placement.expand_placement와 누적 simulation/action_plan.h의 경로 계산/실행을 비교. 현재 RngState는 GetRandomBlock 호출 때 바뀌고 spawn마다 preview를 채우므로 현재 새 피스 탐지 근거가 있으나 영구 spawn ID와 구분할 것. 입력 도메인 검증/정수 overflow·차단된 회전/이동·중력 잠금 중 이전 큐·예측 상태와 실제 틱·정책 실패 구분을 감사.163 새 파일/작업 없음.164 속도·165 캐릭터·166 명시 fallback/보상 경계 뒤 중지.

- 2026-10-04: **161차시 ONNX 내보내기 제작·검수 완료(progress)**. 로컬1~161·799문제. 직전160 턴과 중단된161 전반은 실제 코드/문서/실습을 바꾼 progress. 다음162 C++ 추론 수명. module12(149~166) 완료 뒤 범위 감사·단원 커밋·goal paused 유지. 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP161-onnx-export는160의 README·bindings/CMakeLists 외 누적 파일 보존(cache 제외). export_policy·onnx_pipeline·onnx_contract·실제 ONNX 검사 추가. 원고15절·5문제·인라인9. 학습 상태/추론 그래프·텐서 이름/축/타입/의미·initializer·IR/opset·추적·eval/no_grad·constant folding·고정배치·배열 소유·수치 허용오차와 argmax·external data·파일 커밋을 구현 흐름으로 설명. 반복 부정/중복 문장을 줄이고 실제 오해인 내구성/검사 범위는 사이드 노트로 옮겼다.
- OpenCode deepseek/deepseek-flash /tmp tools deny로 ONNX metadata 검사 명세만 전달(64798 exit0). 161-contract-{material.txt,events.jsonl,draft.json}. 초안 첫 C식 파일 주석을 Python에서 제거하고 spec 이름 중복·shape 필드 누락/알 수 없는 rank 검사 추가. primary가 변환/실제 runtime 비교·원자 교체·native 사례·실습/본문/검사를 통합. 키/DB/설정/전체 저장소 전송 없음, Codex 하위 에이전트 없음.
- 별도 /tmp/study161-python 환경 준비(43094 완료). Python3.12.13·torch2.8.0+cpu·NumPy2.5.3·Gymnasium1.3.0·pybind11 3.0.1·ONNX1.23.1·ONNX Runtime1.30.0. 기존149 환경/시스템 Python 유지. export extra 및 Colab requirements에 onnxruntime 추가, uv.lock은 flatbuffers/onnxruntime 신규 기록만 추가(5453 완료). colab_runtime 준비 기록에 onnxruntime 버전 추가. 원격 Colab 실행으로 주장하지 않는다.
- **현재 exporter 교정**: source/destination 동일 경로 거절, checkpoint 로드 전후 해시·native 입출력 폭 대조. 목적지의 같은 부모 임시 폴더에 TorchScript 경로로 export. 이름/순서/float32/고정 shape·initializer 입력 중복·opset·ONNX 구조를 검사. 중첩 TensorProto external data 및 동반 파일 거절. 후보의 CPU Runtime 수치/합법 argmax 대조 뒤 fsync/replace. 변환·checker·수치·replace 실패 시 이전 파일 보존·임시 정리. 단일 작성자·단일 파일 범위이며 부모 디렉터리 내구성/출처 인증으로 확대하지 않음.
- **검증 사례의 구분**: 현재 기본 CLI는 synthetic tensor probes, export(cases=...)는 실제 관측/마스크 입력. CP는 별도 RoundEnv에서 실제 사례 수집. float32/shape/finite 검사와 allclose 허용오차 뒤 같은 mask의 argmax를 별도 대조. 작은 값의 순서 반전이 allclose를 통과하지만 행동 검사는 실패하는 fixture 포함. 메타데이터에는 규약/체크포인트 해시, 결과에는 실제 사례 수·최대 절대 오차·tolerance/provider 기록. 전체 입력 동등성이나 플레이 성능을 주장하지 않음.
- 수정 전 실제3회귀 실패(161-before.log,45607). 초기3검사 교정 통과(77495). 추가 테스트에서 protobuf repeated field를 자기 자신으로 extend하는 fixture 오류 때문에52465가7개 뒤 진행하지 못함. 제품 코드 오류가 아니며 독립 extra input append로 교정. 중단 뒤 PID189389 부재·52465/84665 및 exec914 handle missing을 확인하여 기존 프로세스를 중복 실행하지 않고 재개. 초기 CP 작업은 최종 증거로 사용하지 않음.
- 최종 root 관련53통과(161-root-final.log,76823). optional export 의존성은 importorskip으로 명시하고 실제 설치된 환경에서 새10검사 모두 통과/skip 없음(161-root-optional.log,27020). symbolic/type/name/count/unknown rank/initializer/external-data 경계, 원본 덮어쓰기·목적지 보존·근사 수치와 행동 차이 포함. PyTorch가 알리는 legacy exporter deprecation은 검증한 경로의 한계로 기록하며 수치 오류로 해석하지 않는다.
- **실제 현재 native 대조**:159의 fresh build-we8ucudt를 같은 Python ABI로 선행 import하고 실제 게임 관측8개에서 export/CPU 실행/합법 선택 일치(161-native.log,16083). 최대 절대 오차2.2351741790771484e-08. source는 실제160 짧은 DDQN checkpoint이며 장기 성능 주장은 없음. 현재 CLI도 실제 실행해 기본 합성2사례 검증 후 cli.onnx 생성(161-cli.log,66109).
- CP 독립 native 빌드·누적 CTest16 전부 통과(161-cp-reviewed.log,79737). 새 ONNX 검사를 metadata/replace 실패까지 보완한 뒤 해당 계약 재실행 통과(161-cp-final.log,59843). 실제 업데이트를 거친 TrainingRun·native관측6개·RNG 보존·기존 ONNX 보존·symbolic/name/dtype/rank/external data·가까운 값의 행동 차이 검사. C++ 런타임 변경 없음으로 sanitizer 반복 없음.
- Part9§6 재구성·§5 추적 입력 설명 및 Part13§2 Python 의존성/발췌·README_colab 동기화. 기존 constant folding이 모든 Conv/Linear weight 계산을 미리 없앤다는 설명을 교정. make_onnx_fixtures의 board/piece/action 폭을 common 상수로 변경. Part 검사에서 발견한 Part13 pyproject 발췌 drift도 교정. 세 절 partial 대응·해시 추가/갱신, 대상 OS C++ 실행/총 의존성 대응으로 확대하지 않음.
- 자동 탐색기에 --lesson=lesson-ID 옵션 추가. 새 본문 순회만 제한하며 전체 메뉴/링크와 공통 탐색·답안·완료·뷰어·file URL/저장불가 계약은 유지. 전체160 DOM은 직전 턴에서 통과했고 이번에는161 집중검사 통과(161-navigation.log,72119). 공통 UI 변경/166 단원 마감에는 전체 순회할 것을 AUTHORING에 기록. 수동 브라우저·스크린샷 없음.
- 799객관식·lazy자산·Mermaid11·Markdown27·인라인9/현재심볼/누적보존·Part 발췌/coverage drift/diff 통과. 정적 release c20cdd3722307167·16파일·상대 자산/해시/재현 ZIP 통과(161-site.log,15676). library28문서·240소스. 학습 화면 문구에 검수 환경 보고를 넣지 않았다.
- 초기 HTTP가 connection refused로 로컬 서버 종료를 확인. 기존 읽기 전용 serve_learning의 DOCS만 docs/learn으로 지정한 /tmp/learning161_server.py로127.0.0.1:18767 복구. server session31110 유지. 생성 자산3개와 허용된 현재 Python 소스4개의 HTTP 해시는161-http-final.log에서 일치했다. pyproject.toml은 뷰어 허용 목록 밖이므로404이며 Part13 발췌 검사로 확인했다. Part 최종 검사 통과(161-part-final.log). 서버 외 검증 작업 종료.
- **다음162 감사 지점**: BotOnnx Impl의 Env/Session/MemoryInfo·입출력 버퍼 소유/수명과 동기 Run을 설명할 것. 현재 Infer는 혼합NaN/+inf를 일관되게 거절하지 않고 내부 fallback을 성공으로 반환할 수 있음. Load 성공 시 과거 err_out이 남는 경로, 생성자/Load 예외 범위, 출력 exact shape와 metadata 의미 규약 검사, 실패 후 loaded 상태를 대조. 현재 third_party/onnxruntime include/lib/linux-x64 존재(fetch 기본1.18.1; 실제 파일 버전은 확인 필요). CMake의 headless bot_onnx_contract_test 타깃으로 실제 C++ 실행 가능성을 확인할 것. ONNX fixture IR/opset은 선택 Runtime과 맞춰야 함. 새162 파일/작업 없음.

- 2026-10-04: **160차시 모델 zoo 제작·검수 완료(progress)**. 로컬1~160·794문제. 직전159 턴은 실제 실행기·노트북 교정과 집필·검증을 수행한 progress. 다음161 ONNX 내보내기. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 계약 유지. 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP160-model-zoo는159의 README·bindings/CMakeLists 외 누적 파일 보존(cache 제외). model_zoo/evaluation_summary/실제 native 비교 검사를 추가. 기존 TrainingRun/PPO/C++ 규칙 불변. 원고15절·5문제·인라인5. 정책 logit/Q 의미·replay/Double DQN 선택/평가·종료/제한·CEM/CBMPI/잠재 탐색·평가 protocol·학습/평가 seed·선택 편향·RNG 소유권·행별 지표·요약/짝 비교·난이도와 실행 조건을 설명.
- OpenCode deepseek/deepseek-flash /tmp tools deny로 선택한 통계 helper 명세만 전달(14447 exit0). 160-summary-{material.txt,events.jsonl,draft.json}. 초안의 summarize/paired_difference를 검토하고 불필요한 재요약을 입력 검증으로 줄이며 차이의 유한성 검사를 보완. primary가 실제 평가/학습 교정·체크포인트·본문·검사를 통합. 키/DB/설정/전체 저장소 전송 없음. Codex 하위 에이전트 없음.
- **현재 DQN 교정**: terminal 행은 다음 max/argmax 계산 자체를 제외해0×-inf NaN 방지. replay done은 terminated, reset은 term|trunc로 분리. live 빈 mask 거절·finite loss/gradient·학습 인자 검증·공통 보드 상수 기반 shaping·줄 카운터·finally 환경 정리·명시한 warm-start 파일 누락 실패. Double DQN은 online 선택/target 평가가 다른 손계산 표적 검사 포함.
- **현재 MuZero 교정**: max_pieces/truncation에서 실제 끝 관측의 scaled value를 원래 보상 단위로 되돌려 return tail에 사용. terminal만0. 경로 입력 배열 소유·환경/mode 정리. search의 reward/value 공통 scale 요구, 작은 temperature 방문 분포를 log 공간에서 정규화. native checkpoint의 원자 교체·유한 dense float32·클래스/용량/버전/의미/scaling 검사, null format version과 bool 의미값의 legacy 우회 거절. warm start는 weights-only로 명시하고 scale 불일치 거절. CEM/CBMPI도 missing resume의 조용한 새 학습을 거절; 전체 알고리즘 정확성/장기 성능 검증으로 확대하지 않음.
- **현재 공통 평가**: evaluate_episodes가 별도 env·모드 복원·시드별 lines/score/reward/pieces·terminated/truncated/budget을 기록. 줄 수는 누적 info.lines 사용. train.model_zoo는 동일 protocol·모델 해시·선택 소스/확장 해시·runtime·원자료/요약·첫 후보 대비 짝 차이 기록, 기존 report 덮어쓰기 거절. validation/test 구분은 기록이며 전역 시드 장부나 자동 중복 차단으로 주장하지 않음. 원자료의 빈 목록/중복seed/NaN/서로 다른 seed 순서 거절.
- 회귀 재현: 최초160-before 전체 실행은 잘못된 scale 거절 검사에서 수정 전 실제 MCTS 학습으로 진입해 명시 종료(85022 exit143). 한정된 HEAD 재현160-before-bounded는7실패1통과(17683 exit1). 수정 직후9통과, 확장20통과, 관련 최종91통과(160-root-reviewed.log,85115 exit0). 중간89도 통과했으나 최종 집계는 null/bool 버전 검사까지 포함. 실제 fresh root native 선행 import 및 TETRIS_PY_MODULE_DIR 지정, skip으로 성공 수를 부풀리지 않음.
- **실제 native 훈련/평가**:159에서 새로 만든 build-we8ucudt 확장을 사용(C++ 변경 없음). DQN/DDQN 각3결정·MuZero2결정/작은 MCTS·한 업데이트·두 증류 step·canonical 로드·동일seed비교 통과(160-native.log,84707 exit0). native MuZero의 canonical 평가 입력 거절. 문서의 colab_runtime launch→train.model_zoo CLI도 새 프로세스로 실행해 report 생성(160-cli.log,39078 exit0). 이 작은 실행을 학습 성능·원격 Colab·GPU·실제 대전 난이도 근거로 사용하지 않음.
- CP 독립 Release build·누적 CTest15 전부 통과(160-cp.log,39904 exit0). 새 계약은 실제 TrainingRun 초기/업데이트 후보·동일 파일 복사본·동일seed결과·보고서와 RNG 보존·별도 CLI·재실행 덮어쓰기 거절·지표/잘못된 입력을 검사. 새 C++ 런타임 변경 없음으로 sanitizer 반복 없음.
- Part9§7 재구성·§4의 build 전체 삭제/clone은 CBMPI만 의존한다는 안내 교정, README_colab 모델 비교/weights-only 경계 추가. CEM을 derivative-free라고 부르던 오류, shape 호환=무엇이든 같은 성능이라는 과장 교정. 해당2절 partial 해시 갱신; ONNX/캐릭터 대전 전체 완료로 확대하지 않음. Part발췌·coverage drift·diff 검사 통과.
- 794객관식·lazy자산·Mermaid11·Markdown27·인라인5/현재심볼/누적보존·전체160 DOM 단일mount/앵커/검색/답안·완료·소스/file URL/저장불가 검사 통과(160-navigation.log,60741 exit0). DOM 실행 중 생성 사이트는 고정. 이후 MuZero의 null/bool metadata 검사라는 소스-only 변경을 자료/강의 번들에 재생성하고 최종 정적 배포/HTTP로 확인한다. 수동 브라우저/스크린샷 없음.
- 전달받은 리뷰의 library/Mermaid 지연로딩·SDL 진입점/Config 우선·noscript·그래픽 지도·절 라벨·가변 수치/CSS/모바일 공백 교정 유지. 현재 lazy 검사와 UI 문자열 검색으로 재대조했다. 로딩이 반드시1/3로 감소한다는 계산이나 macOS 완전 정상이라는 추론은 채택하지 않음. 실제 플랫폼 실행과 전송/파싱 시간은 별도 증거가 필요하다. 공개1~55는 갱신하지 않음.
- 최종 정적 release ce17c774e387cd29·16파일·상대 자산/해시/재현 ZIP 통과(160-site-final.log). library28문서·238소스. 로컬18767의 최종 자산과 다섯 현재 소스 내용/해시 일치(160-http-final.log). Part 기록/coverage drift/diff 통과.
- 모든160 외부/검증 작업 종료: DeepSeek14447; 초기 회귀85022 명시 종료→bounded17683 완료; root55421/77545/73578/85115 통과; native84707/CLI39078; CP39904; 생성19235/77358/79456; site44302·static63982·DOM60741·최종HTTP91527 전부 terminal 확인. 로컬18767만 유지. 다음161로 진행하며166 단원 마감 뒤에만 커밋·goal paused.
- **다음161 감사 지점**: export_onnx.py는 최종 목적지에 직접 export한 뒤 checker 실행하므로 실패 시 기존 산출물 보존을 못함. checker 성공과 C++ 입출력 형식/실제 수치 일치 구별, 비기본 모델의 n_piece_types/n_placements와 native 계약 대조, 실제 관측의 PyTorch↔ONNX Runtime 출력/합법 선택 비교, 동적축·opset·trace/shape·가중치와 그래프 의미 설명. python/tests/make_onnx_fixtures.py의 고정 보드/피스/행동 폭을 계약 값과 대조할 것. ONNX/onnxruntime는 현재 CPU venv에 아직 없음. 새161 파일/작업 없음.

- 2026-10-04: **159차시 Colab 실행 제작·검수 완료(progress)**. 직전158 턴은 실제 저장/복원 교정·집필·검증이 있는 progress. 로컬1~159·789문제, 다음160 모델 zoo와 실험 비교. 현재 module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused. 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP159-colab-workflow는158의 README·bindings/CMakeLists 외 누적 소스를 보존(cache 제외). experiment·process_runner·실제 child/산출물 계약을 추가. C++ 규칙과 PPO/TrainingRun은 유지. 원고14절·5문제·인라인6. 노트북/커널/파일 수명·ABI·현재 Python·import 캐시·새 프로세스·소스/모듈 식별·CPU/GPU 경계·인자 배열·출력 파이프/종료·실험 기록·Drive/배포 차이를 구현 흐름으로 설명.
- OpenCode deepseek/deepseek-flash /tmp tools deny로 선택한 run_logged 명세만 전달(99232 exit0). 159-runner-{material.txt,events.jsonl,draft.json}. primary가 초안의 마지막 wait 중단 시 자식이 남는 범위를 보완하고 terminate 경합·환경 출력 설명을 교정했다. helper/훈련 명령·실행기·노트북·실습/검사/본문은 직접 통합. 키/DB/설정/전체 저장소 전송 없음, Codex 하위 에이전트 없음.
- **현재 노트북 교정**: 같은 sys.executable의 pip/pybind/CMake Python을 사용하고 실패 코드를 검사. 기존 checkout 자동 pull/reset과 전체 build/로드된 확장 삭제를 제거. 선택한 full commit은 변경사항이 없는 경우에만 명시적으로 checkout. prepare_native는 매번 독립 폴더에 tetris_py/sim_hash_dump 빌드, 현재 ABI의 단일 확장·빌드 전후 소스 해시·모듈 파일 해시·Python/패키지 버전·commit/dirty 여부 기록. helper 변경 뒤 커널의 캐시 재사용 거절.
- **현재 실행 흐름**: fresh child가 지정한 tetris_py를 선행 import하고 실제 경로를 대조한 뒤 trainer/export 모듈 실행. native smoke는 clone/배치·환경 step·모델 gradient/갱신·checkpoint 왕복을 실제로 수행. 검증은 최적화 옵션에서 사라지는 assert 대신 명시적 예외. GPU available은 가용성 정보이며 GPU 연산 성공으로 쓰지 않는다. root SimGame의0줄 성공을 bool 실패로 판정한 새 probe 초안 오류를 실제 실행이 잡아 >=0 계약으로 교정했다.
- **현재 실험 명령/기록**: training_commands가 preset과 이름 검증 후 현재 Python 인자 배열 반환. HEAD의 smkoe가 long/큰 예산으로 떨어짐을 재현(159-before.log) 후 거절. run별 독립 폴더·manifest/train.log/result 및 모델 해시, 실패 전파·기대 checkpoint 확인. long은 같은 알고리즘/준비 상태의 성공 smoke 기록 필요. 재실행/오타/다른 알고리즘/바뀐 준비/결과 없는 성공을 구별. export는 현재 ALGO/RUN_NAME/준비와 완료 manifest를 대조. Drive에는 run 기록도 보관하며 학습 디렉터리와 게임 배포 묶음은 분리.
- workflow/static19검사 통과(159-workflow-reviewed.log). 실제 child 비정상 종료/기존 로그 보존, 최종 wait 중단의 직접 자식 정리, 잘못된 preset/이름, stale preparation, 실패/중복/다른 알고리즘 long, 정상 종료지만 checkpoint 누락 검사. 일부 admission 테스트의 산출물은 fixture이며 실제 학습 증거는 아래 별도 실행으로 한정한다.
- **실제 현재 native/훈련**: prepare_native로 새 root Release 확장/driver 빌드·CPU native/model smoke·짧은 실제 PPO·checkpoint/manifest/result 생성 통과. 최종159-native-reviewed.log, out/colab-native/build-we8ucudt/contract-runs/cpu-contract. 초기57060은 probe의0줄 반환 오판으로 실패→44321 통과; 이후 probe assert 제거 후48571 최종 재빌드/실행 통과. 실제 통합 예산은 명시적인3결정이며 notebook 전체 smoke/long preset이나 모든 알고리즘을 실행했다고 주장하지 않는다. ONNX/onnxscript 없음·CPUtorch 조합을159-ready.json에 기록.
- CP 독립 Release 확장·누적 CTest14 전부 통과(159-cp.log). 새 experiment 계약은 실제 native CPU child의 예산/업데이트/저장/manifest/hash/log/중복 거절과 잘못된 preset/미확인 long 거절을 검사. parent의 상태와 child 메모리 분리, 진행 저장은 누적 TrainingRun 경계 사용. C++ 런타임 변경 없음으로 sanitizer 반복 없음.
- Part9§8 전면 재구성, README_colab·bots-and-colab의 빌드 삭제/고정 행동 수/Drive 기록 범위·warm start·실행 과장 교정. 노트북 code는 셸 magic 없는 Python으로 구문 검증하고 저장 outputs 비움. 관련 partial 대응 해시 갱신; GPU/Colab/ONNX/모든 알고리즘 대응 완료로 확대하지 않음. 현재 노트북/소스 변경은 로컬 작업이며 공개 브랜치에 아직 반영하지 않았다.
- Part발췌·Markdown27·789객관식·lazy자산·Mermaid11·인라인6/현재심볼/누적보존·전체159 DOM 단일mount/앵커/검색/답안·완료/소스/file URL/저장 불가 경로 통과. DOM 종료까지 생성 사이트를 고정한 뒤 root probe의 명시적 검사 변경을 소스 스냅샷에 재생성. 강의/UI 구조는 같으며 마지막 소스-only 변화는 정적 배포/HTTP 해시로 확인. 수동 브라우저/스크린샷 없음.
- 원격 Colab·Drive 마운트·CUDA/ONNX 실행·장기 훈련·Windows/macOS native·원격 저장 내구성은 미검증. notebook 자체를 원격 실행한 것처럼 기록하지 않는다. helper의 선택 소스 해시와 runtime 식별자는 내부 실행 일관성 검사이며 인증/전체 환경 증명의 수단으로 확대하지 않는다.
- **다음160 감사 지점**: DQN train_step은 next_mask가 빈 terminal에서 target max=-inf 뒤0을 곱해NaN 가능. done=term|trunc replay와 실제 끝 bootstrap을 분리할 것. MuZero self_play는 외부 max_pieces/시간 제한에서 tail을0으로 만드는 리턴 및 native checkpoint 직접 torch.save/resume 파일 없음 경로를 감사. 모델 zoo의 Q값/정책 logit/증류 모델 의미·평가 조건/시드·학습량과 난이도·artifact 해시/선택을 구별. DQN/CEM/CBMPI/MuZero의 기존40/20×10 설명·하드코딩도 실제 계약과 대조. 새160 원고/체크포인트/외부 작업 없음.
- 최종 정적 release bd7375f06e137752,16파일·상대 경로/자산해시/재현 ZIP 통과(159-site-final.log). library28문서·237소스. 로컬18767 강의/자료/코스 및 세 실행 helper의 현재 소스 해시 일치(159-http.log). diff/coverage drift 없음.
- 모든159 외부/검증 작업 종료: DeepSeek99232,현재native57060실패→44321→48571최종통과,CP46683,workflow37088,Part85758,초기library13418/lessons6623,site44552/정적99169/DOM17774,최종library83675/lessons99299/site6877 모두 terminal 확인. 로컬18767만 유지. 다음160 집필로 진행하며 단원166 뒤에만 범위 감사·커밋·goal paused를 적용한다.

- 2026-10-04: **158차시 학습 체크포인트 제작·검수 완료(progress)**. 직전157 턴은 집필·코드 교정·자동 검사 및 기록 마감이 있어 progress. 로컬1~158·784문제, 다음159 Colab 실행. 현재 module12(149~166) 완료 → 범위 감사·단원 커밋 → goal paused 계약 유지. 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP158-training-checkpoint는157의 README·bindings/CMakeLists·python/policy_network 외 누적 소스 보존(cache 제외). 모델에 용량 설정 속성만 추가하며 그래프/C++ 규칙 불변. atomic_save·ReplayRoundEnv·TrainingRun·새 프로세스 demo/계약 검사 추가. 원고14절·5문제·인라인9. 직렬화/주소·추론/warm start/재개·Adam 모멘트·저장 경계·난수 위치·입력 로그 재생·파일 원자성/내구성·후보 복원·실행 조건을 구체적으로 설명.
- DeepSeek deepseek/deepseek-flash /tmp tools deny로 선택한 atomic_torch_save 명세만 전달. 처음 material 상대 경로가 /tmp를 기준으로 해석돼 첨부 누락(exit1); 파일 절대 경로로 생성 후18603 exit0. 158-atomic-{material.txt,events.jsonl,draft.json}. primary가 전원 장애에 반드시 손실된다는 단정을 내구성 미보장으로 교정하고 fdopen 실패의 FD 정리를 보완. 키/DB/설정/전체 저장소 전송 없음, Codex 하위 에이전트 없음.
- **현재 모델 저장 교정**: constructor config 및 format_version/io_contract를 기록하고 예약 meta 덮어쓰기를 거절. 유한한 기본 metadata·canonical 클래스·dense float32 가중치 검사. CPU에서 읽고 클래스/정수 버전/의미/구성/키·shape·dtype·유한성 검사 뒤 새 모델을 요청 장치의 eval 상태로 반환. format_version 없는 과거 기본 구성 파일은 호환하되 불완전한 새 필드의 legacy fallback은 거절. 정상 그래프/ARCH_VERSION은 유지. 동일 shape의 의미 버전 갱신은 사람이 관리하는 계약임을 명시.
- **파일 실패 경계 교정**: 같은 디렉터리 임시 파일에 동기 직렬화→flush/fsync/close→replace. 직렬화·fsync·교체 실패 시 기존 목적지 보존/임시 정리 검사. 단일 작성자·저장 동안 모델 불변 계약. 부모 디렉터리 fsync, 전원 장애 내구성, 다중 파일 원자성이나 출처 인증까지 보장하지 않음. 기존 payload와 비기본 구성 왕복·잘못된 버전/의미/shape/dtype/NaN/metadata 검사.
- 현재 PPO/정책경사의 --resume은 weights-only warm start로 도움말/로그 명시. 지정한 경로가 없으면 새 학습을 조용히 시작하지 않고 실패한다. PPO는 checkpoint 읽기 뒤 환경 생성으로 순서를 옮겨 로드 실패 시 환경을 남기지 않는다. 다른 훈련기 전체의 resume 경로 변경이나 완전 훈련 상태 복구 구현으로 확대하지 않는다. common/__init__의 고정 행동 수 설명과 버전 검사의 과장도 보정.
- **실습의 실제 재개**: CPU 단일 동기 TrainingRun이 완료 업데이트 경계에서 gradient를 비우고 저장. 중간 실패는 ready=false로 진행/저장 차단. 모델·Adam·config·mode·카운터·Torch RNG·Collector 통계·현재 에피소드 seed/성공 행동·미래 seed Generator를 저장한다. ReplayRoundEnv가 같은 규칙으로 재생하고 native bytes 대조, 실제 복원 obs/info 사용. 후보 생성은 fork_rng로 감싸 실패 시 호출자 RNG 보존; 성공 뒤 저장 RNG 커밋. native schema/규칙 revision/주요 runtime 식별자 검사. CUDA·비동기/대전·중간 미니배치·scheduler/AMP 상태는 이 형식 범위 밖이다.
- 초기 현재 오류8회귀 전부 실패(158-before.log)→기본/새24통과(158-root.log). 최종 실제 fresh tetris_py를 선행 import하고 환경 선택 변수도 지정하여 checkpoint/정책/PPO/Gym/보상/대전127통과(158-root-native-final.log). 앞선 실행은 변수 누락으로90pass37skip였으며 전체 통과로 집계하지 않는다. 실제 native 기존157 모델 파일에서 PPO/A2C warm start→업데이트→새 형식 저장/로드도 통과(158-native.log). 짧은 학습의 미완료 에피소드 로그 nan은 성능값 부재이며159 smoke 안내에서 구분할 검토점.
- CP 독립 Release 확장 빌드·기존12 CTest 통과. 새 checkpoint 검사는 초기 변조 fixture가 native bytes를 list처럼 수정해 TypeError; bytearray로 교정 후4개 검사 메서드 모두 통과(158-cp-final.log). 실제 별도 프로세스의 다음 rollout/지표·모델/Adam·환경/통계/카운터/RNG가 연속 실행과 정확히 일치하고 다음 구간의 에피소드 reset도 포함. 초기 스냅샷·비기본 용량·남은1표본·실패 후보/실패 run 경계 포함. 새 C++ 런타임 변경 없음으로 sanitizer 반복 없음.
- Part8§7/8 재구성·Part9§5/8 관련 문구 수정,156차시 현재 로더 설명1.0.1로 갱신(CP156 안정 코드 유지). model 파일과 전체 재개 스냅샷을 구별. 해당4절 partial 대응 해시 갱신, ONNX/Colab 전체 대응 완료로 확대하지 않음. Part/누적보존/인라인 검사 통과.157 진행 기록의 수치 배열 뒤 괄호가 잘못된 Markdown 링크로 해석되던 표기도 교정.
- 정적 검수: Part발췌·Markdown27·784객관식·lazy자산·Mermaid11·인라인9/현재심볼/누적보존 통과. 최종 release af6710b233cec7d4,16파일, 상대 경로/자산해시/재현 ZIP 검사 통과(158-site.log). 로컬18767 자산 및checkpoint/atomic helper/검사/PPO 소스 해시 일치(158-http.log). 첫 sandbox localhost 요청은 제한돼 승인된 로컬 read로 재실행 후 성공. 수동 브라우저/스크린샷 없음. Windows/macOS native·CUDA·Colab·장기훈련·전원 장애 내구성 미검증.
- 다음159: 두 Colab notebook의 !pip/python 실행환경 불일치·명령 실패 후 셀 진행·전체 build 삭제·실행 중 로드된 native 모듈 교체/캐시·무조건 git pull·고정 /40 출력·smoke/장기학습/Drive 파일 저장 범위를 감사. README_colab/setup markdown의 raylib·기본모델·import성공=export보장 문구와 재개 설명을 갱신할 것. 추후160의 DQN/MuZero term|trunc 합산 감사 예약 유지. 새159 파일/코드 수정/외부 작업은 아직 없다.
- 최종 전체158차시 DOM 단일 mount/앵커/검색/답안·완료 보존/소스 참조·file URL·저장 불가 경로 통과(158-navigation.log). 모든158 외부/검증 작업 종료: DeepSeek18603,초기회귀82890,기본27485/61794,CP7361(새 fixture 실패)→49848통과,root72007(부분 skip)→11452전체통과,native11467,Part7410(157 기록 링크 오류)→57570통과,생성22519/7536,site62504,정적38464,DOM8788 exit0. 로컬18767만 유지. 전체 goal은 active이며 단원166 마감 후에만 사용자 요청대로 paused로 전환한다.

- 2026-10-04: **157차시 PPO 제작·검수 완료(progress)**. 로컬1~157·779문제. 다음158 저장·복구. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 요청 유지. 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP157-ppo는156의 README·bindings/CMakeLists·python/gym_env 외 누적 소스 보존(cache 제외). lines_cleared 사건 정보와 returns/ppo_loss/Collector/update/evaluate/demo/계약 검사 추가. 원고17절·5문제·인라인12. 확률비·고정 행동 마스크/old logp·계산 그래프와 데이터 소유권·TD/GAE·종료와 외부 제한·clip의 부호별 의미·모집단 표준편차·수집 예산·평가 지표를 구현 흐름으로 연결.
- DeepSeek deepseek/deepseek-flash에 선택한 순수 GAE/정규화 명세를 /tmp·tools deny로 전달(60245 exit0). 자료/응답:157-gae-{material.txt,events.jsonl,draft.json}. primary가 numbers.Real 검사·출력 overflow·중복 설명을 보완하고 실제 수집기·loss·본문·검사를 통합했다. 키/DB/설정/전체 저장소 전송 없음, Codex 하위 에이전트 없음.
- **현재 PPO 교정**: 실제 step의 next_observation 가치를 reset 전에 저장. terminal bootstrap 마스크와 episode trace 마스크를 분리해 외부 제한에서 가치 꼬리를 보존하고 새 에피소드 보상이 섞이지 않도록 했다. info.lines 누적 native 줄 수를 제공하여 reward와 구분. 잔여 예산에 맞춘 수집·singleton 정규화·모델 소유 피스 폭·유한 loss/gradient·인수 검증 추가. 평가는 별도 환경과 finally 정리/모드 복원, 줄·점수·보상·종료/제한/평가 예산 종료를 구분. eval_best는 single 줄/versus 보상을 기준으로 명시한다. save_every=0은 주기 저장만 끈다.
- **현재 A2C/REINFORCE 교정**: 전이 배열을 복사해 소유하고 실제 끝 관측 가치·에피소드 경계로 return 계산. A2C는 lambda=1 GAE 반환을 사용하며 rollout 간 에피소드 진행 기록 유지. REINFORCE는 종료 시 Monte Carlo, 외부 잘림 시 value 꼬리의 변형임을 명시. 수집/업데이트 양쪽에 같은 양의 유한 temperature 사용. 잔여 결정 예산·줄 지표 교정. 구버전 A2C 통제 환경 수익 [97.29,98.1,99]가 독립 에피소드의 `[13.5,13.5,13.5]`(제한) 또는 `[9,9,9]`(종료)로 교정됨을 직접 비교(157-a2c-before-after.log).
- 현재 회귀104통과(157-root-final.log). 실제 fresh root tetris_py를 먼저 로드하고 단일/대전 PPO 및 A2C/REINFORCE의 수집→업데이트→평가/저장 소규모 CPU 실행 통과(157-native-training.log,157-actor-critic-native.log). 최초 4오류 재현은157-before.log, 교정 직후157-after.log. CP 독립 확장 빌드·누적CTest12통과(157-cp.log), 최종 데모는 훈련 shaping/평가 shaping=0을 분리하고 마지막 한 전이 배치를 포함한 예제 예산을 지킨다(157-cp-demo-final.log). 장기 성능이나 전체 훈련을 검증한 것은 아니다.
- Part8§5/6/8/9 및 Part9§4/7 설명·발췌를 동기화. 153차시 현재 PPO 레퍼런스도1.0.1로 교정(안정적인 CP153은 유지). PPO clipping을 hard 정책 거리 보장으로 설명하지 않으며 KL/fraction은 진단치, 마지막 minibatch/가중 평균 범위를 구분. partial 대응 해시 갱신; 저장 복구·다른 알고리즘·ONNX 전체 완료로 확대하지 않는다.
- **소스 뷰어 생성기 교정**: Part 문장 재구성으로 reward 검사 파일명 언급이 사라져 152차시 레퍼런스가 누락됨을 생성기가 발견. reviewed 원고의 명시적 reference.path도 독립적으로 수집하도록 변경. 동일 allowlist·실제 파일·숨김/상위 경로/파일 및 부모 symlink 거절을 적용. 새 생성기 검사3통과(157-library-contract.log), check_learning_site에 연결하고 AUTHORING/DEPLOYMENT에 계약 반영. 최종 자료28문서·232소스. 처음 실패한 생성 뒤 구 bundle로 실행된 DOM 결과는 최종 검증에 사용하지 않는다.
- 최종 Part발췌·Markdown27·779객관식·lazy자산·Mermaid11·인라인/현재심볼/누적보존·전체157 DOM탐색/앵커/답안/완료/소스참조·저장불가경로 통과. 최종 release bc7a007ebcc17572,16파일, 상대 경로/자산해시/재현 ZIP 통과(157-site-final.log). HTTP 최종 자산/현재 소스 해시 일치(157-http.log). 수동 브라우저/스크린샷 없음. C++ 런타임 변경 없음으로 sanitizer 반복 없음. Windows/macOS native·CUDA·ONNX·Colab/전체훈련 미검증.
- 리뷰 재대조: 지연 library/Mermaid·SDL 준비/Config 우선·noscript·가변 수치·절 라벨/그래픽 지도·모바일 공백/CSS는 2026-10-03 수정이 현재도 유지됨. 새로 중복 수정하지 않았다. 로딩 1/3·macOS 완전 정상이라는 리뷰 결론은 채택하지 않음. 파일 gzip 추정과 실제 HTTP 압축/태블릿 파싱, 플래그 요청과 OS 실기를 구분한다. 공개1~55는 갱신하지 않았다.
- 모든157 외부/검증 작업 종료: 최종DOM82254 exit0까지 확인. 최종site11312/static24288/root77173, native87840/9128, CP66502/demo16560, DeepSeek60245 등 모두 완료. 로컬18767 서버 유지.
- **다음158 감사 지점**: 현재 checkpoint는 가중치 warm start이며 Adam/RNG/환경/부분 rollout 복구가 없다. 기본 모델만 복원, extra가 예약 meta를 덮어쓸 수 있음, 직접 저장 중 실패로 기존 파일 손상 가능, resume 경로가 없으면 조용히 새 모델로 시작. model config/버전·예약 필드·원자적 파일 교체·실패 진단과 재개 수준을 구분해 구현/설명할 것. native/CP snapshot은 현재 복원 API가 없으므로 hash나 clone을 영속 복원으로 주장하지 않는다. DQN/MuZero의 term|trunc 합산은159/160 관련 감사에 남아 있다. 새158 원고/체크포인트/외부 작업 없음.

- 2026-10-04: **156차시 정책 네트워크 제작/검수 완료(progress)**. 로컬1~156·774문제, 다음157 “PPO: 수집·업데이트·평가를 구분”. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 요청 유지. 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP156-policy-network는155의 README·bindings/CMakeLists 외 누적 소스 보존(실행 cache 제외). C++/바인딩 변경 없이 PolicySchema·PolicyNet·model_contract·policy_demo·실제 native 관측/gradient 검사 추가. 원고16절·5문제·인라인7. NCHW/배치·공유 필터/수용 영역·비선형성·flatten/메모리 간격·피스 결합·logit/value·gradient/step·eval/no_grad·장치·trace/호스트 검사·가중치 형상/의미를 설명.
- DeepSeek deepseek/deepseek-flash /tmp tools deny로 선택한 metadata helper 명세만 전달(11619 exit0). 156-contract-{material.txt,events.jsonl,draft.json}. primary가 생성자 소유 차원 검사의 매 forward 반복과 중복 설명을 제거하고 모델·스키마·검사·본문 직접 통합. 키/DB/설정/전체 저장소 전송 없음. Codex 하위 에이전트 없음.
- **현재 코드 교정**: 모델이 같은 면적의 전치 보드·합친 폭만 맞는 두 피스 벡터·빈 배치를 받는 문제 및 크기/dtype/device 오류 경계 개선. 수정 전 18검사 중14실패(156-before.log; 잘못된 예외 타입/필드 진단도 포함)→수정 후 새18+마스킹9+기본 checkpoint4=31통과(156-root.log). model_contract의 positive_size/validate_policy_inputs 공유. 입력 값 전체 reduction은 하지 않으며 관측 생성/native 상태 계약과 별도.
- 현재 정상 계산 그래프/파라미터 키·ARCH_VERSION 유지. HEAD의 실제 모델 클래스를 분리 로드하여 같은 seed의 기본 가중치와 정상 배치출력이 정확히 같음을 확인(156-legacy.log). 실제 fresh root tetris_py preimport 후 현재 SimGame→관측→기본 모델→마스크→실제 배치16결정 통과(156-native-model-final.log). 첫 smoke는 CP의 finished 이름을 root에 잘못 적용해 실패했으며 실제 game_over API로 교정; 제품 코드 오류 아님.
- CP 독립 확장·누적 CTest11·native 관측과 합법 선택/실제 apply_action·유한한 두 head/trunk gradient·backward 보존/step 변경·배치별 계산·B1가치축·eager/trace 계약·demo 통과(156-cp.log). 데모는 합성 표적 한 번 업데이트이며 게임 성능/전체훈련 주장 없음. 모델 schema는 native에서 읽고 CPU용 용량은 예제 설정. 현재 ID-1 순서와 CP 카탈로그 순서 차이를 명시.
- Part8§7/Part9§5 재구성. 고정 파라미터/콘텐츠 수 설명을 식과 설정으로 변경. strict load가 다른 일반 파라미터 shape를 허용한다는 오류, ARCH_VERSION의 자동 의미검증/비기본 구성 복원 과장 교정. 현재 _piece_one_hot의 unknown-ID 영벡터 호환 규칙을 CP의 엄격한 ID 검사와 구분. 두 절 partial 대응 해시 갱신; 훈련기/저장복구/ONNX 배포 전체 완료로 확대하지 않음.
- Python eager forward의 metadata 검사는 torch.jit.is_tracing 동안 생략. 실제 trace 정상 출력은 대조했지만 trace에 같은 면적 전치가 통과함도 검사하여 Python 예외가 그래프에 내장된다는 오개념 방지. Windows/macOS native·CUDA·ONNX Runtime·Colab/전체훈련 미검증. C++ 런타임 변경 없음으로 sanitizer 중복 실행 없음. 수동 브라우저/스크린샷 없음.
- 정적 검수 결과: Part발췌·Markdown27·774객관식·lazy자산·Mermaid11·인라인/현재심볼/누적보존·전체156 DOM탐색/앵커/답안/완료/소스참조·저장불가경로 통과. DOM 이후 영벡터/메모리 간격 문구만 보완하고 Part/스니펫/객관식·사이트 재검사. 최종 release bab3ec7d93c267db,16파일(156-site-final.log), HTTP 최종 자산/소스해시 일치(156-http-final.log). diff/coverage drift 없음. 최초 원고의 question 키를 prompt로 넣은 형식 오류는 생성기가 거절했고 교정 후 재생성/검수; 제품 JS 변경 없음.
- 모든156 외부/검증 작업 종료 여부: 모두 종료(DeepSeek11619,초기45772/최종root93083,CP88865,legacy50644,root smoke15111실패→58195통과,초기생성82773실패→30983/1263완료,정적45346/12599,DOM87050/67243,사이트49419실패→38753/3277완료,최종quiz2678).. 로컬18767 유지.
- **다음157 감사 지점**: PPO dones가 term|trunc를 합쳐 외부 제한의 bootstrap을 없애며 reset 후 관측을 읽는다. 실제 next_observation 가치와 terminal bootstrap 마스크·episode trace 마스크 분리 필요. PPO 평가/ep_lines가 reward를 줄 수로 누적해 versus 공격/승패항을 줄로 표기. T=1에서 unbiased std가NaN, 예산 remainder 초과·고정피스폭7·clipping이 안전을 보장한다는 주석도 검토. policy_gradient_tetris A2C 수집은 여러 에피소드 rewards를 경계 없이 discounted_returns로 연결하고 reset 상태로 bootstrap하므로 함께 감사. DQN은 term|trunc를 replay done에 합침(159/알고리즘 범위 연계 검토), rl_common 평가도 raw_reward를 줄로 사용. 새157 원고/체크포인트/코드 수정은 아직 없음.

- 2026-10-04: **155차시 휴리스틱 제작/검수 완료(progress)**. 직전154는 구현·문서·검증 산출물이 있는 progress다. 로컬1~155·769문제, 다음156 “정책 네트워크: 입력 형상과 출력 의미”. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 요청 유지. 개별 커밋·푸시·공개1~55 확대 없음.
- CP155-heuristic는154의 README·bindings/CMakeLists·python/versus_env 외 누적 소스를 보존(실행 cache 제외). C++·바인딩 변경없음. heuristic_features·heuristic·heuristic_demo·실제특징/정책검사추가. 원고14절·5문제·인라인9. 특징의정보손실/단위·argmax/한결정탐색·생존우선·사전식동점·정보와계산예산·튜닝/평가시드·수집제한지표를 설명.
- DeepSeek deepseek/deepseek-flash,/tmp,tools deny로선택한순수이진보드특징명세만전송(18622 exit0).155-features-{material.txt,events.jsonl,draft.json}. primary가int검사를numbers.Integral로보완하고불변높이tuple형표기·직사각/이진검사·삼각우물공식을검토.평가계수/후보선택/계획상대어댑터/본문/검사직접통합.키/DB/설정/전체저장소전송없음,Codex하위에이전트없음.
- **현재코드오류교정**: features가잘못된행열·NaN/Inf·소수/음수/복소셀·소수줄수를받고Greedy가비유한/문자열/bool계수를받는경계를16회귀로재현(155-before.log16fail→155-after.log16pass).공유schema/유한비음수정수셀·줄사건정수검사,높이배열형상/범위및int64변환,최종평가유한성검사추가. raw ghost8 점유를관측/C++와동기화(garbage9는점유유지).정상보드기본가중치/선택순서/종료우선정책은보존.
- unsigned 높이의 np.diff가음수를wrap해요철4를252로계산함을확인(155-unsigned-before.log).검증후int64변환으로교정하고회귀추가.가중치dict가NaN으로변경된경우도최종점수거절.행합계가최대높이를포섭한다는주석·특정깊이우물은I만채운다는단정·불명확가중치귀속교정.현재Greedy는추가줄항·native첫동점·종료별도우선없음을docstring에명시.
- **실제C++/Python정책대조**: tests/heuristic_dump.cpp는실제bot::heuristic_placement를호출하고검색전후hash보존및선택적용결과를출력,root CMake테스트타깃추가.새target빌드통과(155-root-build.log).현재fresh tetris_py와여러시드실제후보전개로각자의평가식/첫동점선택을검증;양수줄삭제와두정책의실제다른선택포함.우물/추가줄항이다르므로전정책패리티를강요하지않음.최종현재휴리스틱+대전경계/Gym/보상/마스킹/바인딩79회귀통과(155-root-final.log),전체훈련없음.
- 학습GreedyPolicy는Weights불변·후보Session복사/실제apply_action·선택적생존우선→점수→작은라벨키.없음(None)과모든후보종료구분.일반VersusEnv의선택훅만분리하고PlanningVersusEnv가명시적으로모델clone접근을제공;관측전용상대와정보예산차이를설명.공동반환검증/실패잠금/반영은원래환경유지.
- CP155독립확장빌드·누적CTest10·고정시드대전데모통과(155-contract.log).작은비대칭보드의높이/구멍/우물·입력거절/overflow·실제후보최고키·원본bytes보존·양수줄·정확한동점·실제생존/종료혼합후보·Gymchecker/상대재현검사.데모는예제결정예산에서줄/공격/보상/합법후보수/종료종류/계수를기록하며장기성능주장안함.새C++런타임변경없어서sanitizer중복실행안함.수동GUI·Windows/macOSnative·CUDA·ONNX·Colab전체학습미검증.
- Part8§10재구성,§6현재Greedy발췌동기화. Part9§1/11의가중치출처·비트동등성/동일정책·성능하한·휴리스틱학습불가능·fallback절대계속단정교정. BCTS-inspired이름을특정논문완전재현으로확대하지않음.해당partial대응해시갱신;모델/훈련/배포전체완료로확대하지않음.
- Part발췌·Markdown27·769객관식·lazy자산·Mermaid11·인라인/현재심볼/누적보존·정적ZIP재현성/자산해시통과. release3379bbd62e4b09b9,16파일(155-site.log).로컬18767lesson/library/course·features/versus/새회귀/실제C++선택driver소스해시일치(155-http.log).diff/coverage drift없음.전체차시DOM탐색/앵커/답안/완료/소스참조·저장불가경로도통과(155-navigation.log).
- 실행모두종료: DeepSeek18622,rootdriver빌드60739,CP93908,초기확장회귀34117/최종57952,생성67354,정적19079,DOM28307,사이트73994.로컬18767만유지.
- 다음156: 현재TetrisPolicyNet의Conv→flatten→피스onehot결합→공유MLP→policy/valuehead를만들며NCHW/batch·logit/value의의미·역전파/파라미터·마스크/평가모드를설명.현재forward는같은원소개수의전치행열을flatten으로통과시킬수있으므로실제입력계약감사필요;검사를넣을경우ONNX/export추적과충돌하지않게경계분리.고정콘텐츠수/arch_version과설정저장범위는156/158에서구분.157의PPO시간제한bootstrap(done합산)미해결수정예약유지.새156원고/체크포인트없음.

- 2026-10-04: **154차시 두 보드 환경 제작/검수 완료(progress)**. 직전153은 실제 구현·문서·검증 산출물이 있는 progress다. 로컬1~154·764문제, 다음155 “휴리스틱: 비교할 기준 정책”. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 요청 유지. 개별 커밋·푸시·공개1~55 확대 없음.
- CP154-versus는153의 README·bindings/session.h·study_py.cpp·bindings/CMake 외 누적 소스를 보존. 실행생성 __pycache__는 소스보존대상에서제외(최초검사에서이파일부재로실패한뒤154검사수정). Session에공격총계/수신큐/마지막주입량/검증된add_garbage를노출. python/versus_env·versus_reward·versus_demo,Python공동전이/C++Session검사추가. 원고14절·5문제·인라인9.
- DeepSeek deepseek/deepseek-flash,/tmp,tools deny로선택된순수유한보상명세만전송. 최초상대경로자료가/tmp에써져CLI exit1→절대경로교정후26763 exit0.154-reward-{material.txt,events.jsonl,draft.json}. primary가타입/수치거절·동시종료공식/부호정책검토후current와CP에적용,상대경계·후보반영·네이티브전투·본문직접작성.키/DB/설정/전체저장소전송없음,Codex하위에이전트없음.
- **현재실제오류21개재현**(154-boundary-before.log): 상대소수/문자열/bool행동암묵변환·None/범위외반환무시·상대예외/인수수정시원본부분진행·비유한계수허용. 현재TetrisVersusEnv는두SimGame후보를실행하고상대에게는B후보의폐기용clone을준다.반환의정수/도메인/현재마스크검증,합법행동있는데None거절. PolicyOpponent도int캐스트제거. 보상·출력성공후양쪽참조/횟수반영;중간실패는보드/카운터보존+reset요구.상대외부RNG/부작용롤백이나임의Python코드보안격리를주장하지않음.
- 유한보상계수생성자검사·전이최종결과검사,실패reset훅진행차단추가.누적공격복제카운터제거하고각후보배치직전/직후차분사용.종료한수신보드에는공격큐추가안함.상대가죽지않았다면A종료후에도이번응답을실행하고공동결정말미에승패판정.기존정상보상/관측/행동도메인유지. simA/simB참조는성공시새객체로교체됨을Part에명시.
- 현재새경계+Gym/보상/대전/마스킹/바인딩59회귀통과(154-root-final.log).추가실패경로는실제공격+승리항합산overflow·막힌상대행동·reset훅예외.첫overflow fixture는공격량이작아미발생하여1fail58pass였고,실제공격직전B수신큐로승리종료도겹치는fixture로교정후통과.실제fresh root tetris_py를preimport,전체훈련없음.
- 학습VersusEnv는RoundEnv의공간/관측/시드/수명을활용하고BSession·상대reseed/reset·A→B스케줄을추가.순서있는배치결정,실시간동시틱아님.상대에B관측/마스크복사본,권위마스크별도보존.두틱길이는각각기록하며대전은공동결정보상;단일보드RewardSpec인수거절/그discount출력제거. lines_cleared는이번줄사건.양쪽종료/외부제한·막힌결정예산·폐기후reset계약명시.
- CP154 독립Release확장·누적CTest9·demo·인라인/현재심볼/누적보존통과(154-contract-final.log).실제공격없는첫fixture를통과시키지않고개선해seed1에서공격생성경로확보(154-attack-fixture.log),별도네이티브호출순서와두상태bytes대조및B이번잠금/A다음잠금수신확인.시드/상대관측/수정격리/반환거절/계수overflow/종료후가드검사.마지막info사건이름을명확히한뒤실제바인딩7사례재통과(154-binding-final.log).새SessionC++ ASan/UBSan통과(154-session-sanitized.log);Python/전체학습/Windows/macOSnative·GPU·ONNX미검증.
- Part8§6의현재발췌·공동전이·실패보존/외부부작용·보상유한성·참조교체추가. Part9§3/4의공격API공유=실시간호출순서동일단정교정. 해당partial해시갱신,휴리스틱/훈련/ONNX전체대응완료로확대하지않음.
- Part발췌·Markdown27·764객관식·lazy자산·Mermaid11·전체차시DOM탐색/앵커/답안/완료/소스참조·저장불가경로통과.최종info명확화는코드본문/참조만변경하여snippet재대조·정적배포재검사.최종release308da185f36655b7,16파일(154-site-final.log).로컬18767lesson/library/course및현재versus/보상helper/새회귀소스해시일치(154-http.log).수동GUI없음,diff/coverage drift없음.
- 실행모두종료: DeepSeek26763,오류재현79637,초기root7504,초기CP50996,추가root61875,최종CP23728/7606,최종root30442,C++compile8249,생성48260/23918,정적74457,DOM64745,사이트64032/최종83831.로컬18767만유지.
- 다음155: 현재GreedyBCTSOpponent/heuristic_placement·features/bcts_score의특징정의·줄항중복가중·타이브레이크·후보clone/종료평가/합법성·Python/C++차이를읽고,관측만받는상대와모델전체를검색하는상대의정보예산차이를구별한다.기준정책의강함을품질보증으로치환하지않는다.157의PPO시간제한bootstrap(done합산)미해결감사/수정예약유지.새155원고/체크포인트없음.

- 2026-10-04: **153차시 Gym 환경 제작/검수 완료(progress)**. 직전152도 구현·문서·검사 산출물이 있는 progress다. 로컬1~153·759문제, 다음154 “두 보드 환경: 상대와 함께 진행”. module12(149~166) 완료 후 범위 감사·단원 커밋·goal paused 요청 유지. 개별 커밋·푸시·공개1~55 확대 없음.
- CP153-gym-environment는152의 README/bindings CMake 외 누적 파일을 보존. C++·바인딩 변경 없음. python/gym_contract·gym_env·gym_demo 및 실제 확장 계약 검사 추가. 153.json16절·5문제·인라인10. adapter·spaces/합법mask·엄격한정수·수명상태·RNG소유/시드열·후보반영·자기루프할인·종료/외부제한·bootstrap·배열소유·wrapper를 설명한다.
- DeepSeek deepseek/deepseek-flash, /tmp, tools deny, 선택 정수 경계 명세만 전송(51880 exit0). 153-contract-{material.txt,events.jsonl,draft.json}. primary가 중복 타입검사 제거·options의 거짓값 타입도 명시거절·실제환경/후보반영/검사/본문을 통합했다. 키/DB/설정/전체저장소 전송 없음, Codex 하위 에이전트 없음.
- **현재 코드 오류 수정**: 두 환경의 super.reset 누락·매 reset 같은 판 반복·소수/문자열/bool 행동의 암묵적int변환·종료후step 반복 보상·무효행동으로 대전제한 우회·랜덤상대 RNG가reset에안돌아감·명시 opponent_seed 덮어씀을 교정. 명시 시드는 기존 네이티브 시작값 유지, 생성자 시드는 첫reset에적용, 이후None은환경RNG연속. max_pieces는 호환옵션명 유지하되 도메인안 step결정횟수로정의; 막힌배치도소비. action_applied/episode_seed/decisions진단추가. 모델입출력/규칙/계정보상 변경없음.
- 실제 환경11회귀 수정전실패(153-gym-before.log)→수정후통과(153-gym-after.log). 최종새Gym+이전보상/대전/마스킹/바인딩35회귀 통과(153-root-final.log). 실제새root tetris_py 경로를preimport/확인. 공식check_env양쪽, 시드열·랜덤상대·상대시드uint64wrap·잘못된행동·reset·close·막힌요청제한·실제승리뒤재호출거절을 확인. GPU/전체학습 없음.
- 학습환경은 타입/도메인오류예외, 현재막힌라벨자기루프 정책. 후보에서 실제전이·보상·출력성공후Session/횟수반영. 무효결정은0틱이며 decision할인gamma, tick할인1; 잠재함수도 같은할인. 내장외부예산 max_steps와실제종료독립, 둘다True가능. 외부TimeLimit은 내부가모르므로바깥반환값을따름. 출력복사·최종관측/reset관측구별·수치오류후원본보존검사.
- CP153 독립Release확장빌드·누적CTest7·demo·인라인/현재심볼/누적보존 통과(153-contract-final.log). 최초152동일네이티브로사전검사후153자체빌드로재확인. C++변경없어서sanitizer중복실행안함. CPU NumPy/Gymnasium/기존PyTorch만사용. Windows/macOS native·CUDA·ONNX·Colab전체학습 미검증.
- Part8§5/6/9·Part9§3/4의 현재발췌/시드·수명·도메인·info·무효횟수계약 동기화. 모든프레임워크에즉시연결·동일관측이면훈련기수정불필요·정해진전략습득보장 단정교정. 152원고1.0.1은현재대전의카운터설명만갱신; CP152 안정코드유지. 관련부분대응해시갱신, 대전/훈련기전체완료주장안함.
- Part발췌·Markdown27·759객관식·Mermaid11·lazy자산·정적ZIP재현성/해시·diff 통과. release a31fcd5df133f030,16파일. 로컬18767 lessons/library/course 및새gym helper/검사·단일/대전소스해시일치(153-http.log). 전체차시DOM탐색/앵커/답안/완료/소스참조·저장불가경로도통과(153-navigation.log). 수동GUI없음.
- 실행작업모두종료: DeepSeek51880,초기회귀90509,수정후59813,CP빌드20659/최종23256,root51300,생성24203,정적검사4987,사이트4868,전체DOM31875. 로컬18767만유지.
- **미해결과 다음범위**: current PPO가 term or trunc를하나의done으로저장해시간제한부트스트랩을없앤다. Part와153현재레퍼런스에한계명시.157 수집·GAE에서마지막실제관측/가치·terminal마스크·에피소드경계마스크분리수정/회귀; 다른훈련기도같은문제감사필요.154는현재versus공격라우팅·상대콜백의암묵int변환/오류시부분진행·상대시점관측/종료·보상유한성과누적Round양쪽Session을검토. 현재versus전체가실패원자적이라고주장하지않는다. 새154원고/체크포인트없음.

- 2026-10-04: **152차시 보상 제작/검수 완료(progress)**. 직전151도 코드·문서·검증 산출물이 있어 progress다. 로컬1~152·754문제, 다음153 “Gym 환경: reset·step·종료”. module12 완료 후 범위 감사·단원 커밋·goal paused 예약 유지. 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP152-rewards는151의 README/bindings CMake 외 누적 파일을 보존. C++·바인딩 변경 없음. python/rewards·reward_runner·reward_demo 및 순수/실제 바인딩 보상 검사를 추가. 152.json15절·5문제·인라인8. 점수/줄/공격/BP·리턴·할인 시계·고정 잠재함수·텔레스코핑·진짜 종료/수집 경계·보상 항 분해·실패 원자성을 설명한다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny, 선택 순수 보상 명세만 전송(75800 exit0). 처음 /tmp에서 상대경로 자료생성 실패·CLI도exit1, 이후 절대경로/set-e로 재시도. 152-reward-{material.txt,events.jsonl,draft.json}. primary가 거대정수 변환 오류/보상 곱셈 오류를 명시적 거절로 보완하고 shaping=0의 불필요한 overflow 연산을 제거. 보드 잠재함수·Session 후보 실행·검사·원고 직접 통합/검수. 키/DB/설정/전체 저장소 전송 및 Codex 하위 에이전트 없음.
- RewardSpec은 고정 설정이며 decision/tick 기준 구분. d틱 행동의 마지막 틱에 줄/종료 항이 발생한다는 계약에서 gamma^(d-1) 가중, 미래 할인 gamma^d 반환. 잠재함수 차이는 같은 할인 사용; 실제 종료만 끝 잠재값0. 수집 cutoff에서는 끝 항 유지. execute는 clone에서 상태/보상을 계산하고 성공한 후보만 반환하여 호출자가 반영. 현재 PPO 목적을 자동 교체하지 않음.
- 실제 CP152 확장 빌드와 누적/새 CTest6 통과(152-contract-final.log). 순수7사례에서 할인·가변 지속시간 상쇄·종료 경계·반복 벌점의 목표 역전·비유한/비정상 값 검사. 실제 Session에서 전체 prefix 상쇄·입력 틱 재생과 후보 상태 일치·진짜 종료·잘못된 행동 보존 확인. 추가 fixture는 실제 양수 줄 삭제와 점수 차이, 실제 후보 전이 뒤 보상 overflow 거절/원본 보존을 비공허하게 검사(152-binding-final.log). 새 C++ 변경 없으므로 sanitizer를 중복 실행하지 않음.
- **현재 코드 오류 수정**: PPO board_features가 전치 shape/NaN/비이진 관측을 받아 잘못된 보상을 만드는 경계를 먼저 재현(152-reward-before.log:1fail1pass). 모델 스키마의 HW/CHW·유한 이진 값 검사, shaping 계수/결과의 유한성 검사 추가. 계수0은 기존대로 보드 계산을 생략. 정상 보드의 기존 벌점 공식과 모델/가중치 형식 유지.
- 현재 단일/대전 환경 및 기존 마스킹20회귀 통과(152-root-final.log). 실제 새 root tetris_py 경로를 import 전에 지정/확인, Gymnasium1.3.0은 /tmp/study149-python에 격리설치. 단일 보상의 줄 사건/게임 점수 차이·대전 줄/공격/종료 항·무효 행동 시 두 보드와 카운터 보존·실제 공격 라우팅과 종료/결정론·기존 기울기/모델1회 갱신 검사. GPU/전체 정책 학습 아님.
- **설명 교정**: 작은 shaping 계수면 정책 보존·동시 탑아웃 벌점이면 모든 자폭 방지·휴리스틱에 못 이기면 반드시 버그라는 단정 제거. 현재 반복 벌점과 학습 잠재함수 방식의 차이 명시. versus의 잘못된 “불법 행동이어도 상대 진행” 주석 교정. features 계수의 근거 없는 출처/성능 단정도 제거. Part8 보상/대전/훈련/기준 평가와Part9 계약/특징 설명·현재발췌 동기화. Part8§5·6·9/Part9§3 partial, Gym/대전/PPO 전체 완료로 확대하지 않음.
- Part발췌·Markdown27·754객관식·Mermaid11·lazy자산·전체차시DOM탐색/앵커/답안/완료/소스참조·인라인/현재심볼/누적보존·정적ZIP 재현성/자산해시·diff 통과. release db0f729a53f4eb91,16파일. 로컬18767 lesson/library/course와현재ppo/versus/features/새reward검사 소스해시 일치(152-http.log). 수동GUI 없음. Windows/macOSnative·CUDA·ONNX·Colab 전체 학습 미검증.
- 실행 작업 종료: DeepSeek75800,초기실패회귀68008,수정후49485,CP빌드24009/검사95818,실제root70562,생성45267/91573/35103,전체DOM65750,사이트21912,나머지56365,다음계약사전감사72934. 로컬18767만 유지.
- **다음153에서 처리할 확인된 문제**: 실제 Gymnasium check_env에서 TetrisPlacementEnv와 TetrisVersusEnv 모두 reset(seed=...)가 super().reset을 호출하지 않아 RNG 계약 assertion으로 실패했다(152-next-gym-audit.log). 단일과대전의 기존 시드/상대 reset/반복에피소드 의도를 읽고 표준계약·행동 타입/범위·reset전/종료후step·terminated/truncated·시간제한/seed재현을 구현/회귀한다. 수집 중단을 진짜 종료로 합치지 말며, 학습용 RewardSpec과 훈련의 할인 기준을 맞춘다. 새153원고/체크포인트 없음.

- 2026-10-04: **151차시 행동 제작/검수 완료(progress)**. 직전150도 구현·문서·검증 변경이 있어 progress다. 로컬1~151·749문제, 다음152 “보상: 점수와 정책의 목표”. 사용자 요청에 따라 현재 module-12 완료 후 범위 감사·단원 커밋·goal paused를 적용한다. 단원 경계 전 개별 커밋·푸시·공개1~55 확대 없음.
- CP151-actions는150의 README·bindings/session.h·study_py.cpp·bindings/CMake 외 누적 파일을 보존. action_space/action_plan·Python actions/action_demo·C++/Python 행동 계약 추가. 151.json16절·5문제·인라인9. 원점/방향·전단사·실패 원자성·루프 감소량·경로군/전체 도달성·논리 틱/현실 시간·지속시간 할인·중복 라벨 확률·마스크/역전파를 연결한다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny, 선택한 행동 인코딩 명세만 전송(36359 exit0). 151-action-{material.txt,events.jsonl,draft.json}. primary가 범위 검사 후 불필요한 런타임 곱셈 검사 제거·constexpr/noexcept 보완, 실제 Round 틱 planner와 본문/바인딩/검사 직접 구현·검수. 키/DB/설정/전체 저장소 전송 없음, Codex 하위 에이전트 없음.
- planner는 복사본에서 실제 회전→이동→드롭, 회전 뒤 원점에서 이동 계산, 막힘·준비 중 잠금 거절, 성공한 결과만 적용. 모든 도달 위치 탐색은 아니다. 독립 실제 확장모듈·누적/관측/새 행동 CTest4 통과. 여러 시드·중력간격·전체 라벨·벽/바닥 fixture·조회/실패 보존·macro와 동일 입력열 재생 bytes 비교. C++ planner ASan/UBSan 통과. 마지막 수치 guard 추가 후 CTest4 재통과(151-contract-final.log).
- **실제 학습 오류 수정**: PPO/정책경사 엔트로피에서 where 이전의 0×-inf가 역전파 NaN을 만듦을 재현(151-mask-before/151-distribution-before.log,5fail1pass). 공통 masked_entropy에서 곱셈 전 불법 logp를0으로 변경. masked_log_softmax의 shape/dtype/device·빈행·합법logit/정규화값 유한성 검사, eps 인수는 호출호환 유지하되 상수 추가 제거. 정상 모델 형상/가중치 형식 변경 없음. 최종9회귀에서 합법 항만 계산한 기준과 값/기울기 대조·실제 TetrisPolicyNet의 작은 CPU 구성1회 optimizer 갱신·극단값 거절 통과. 전체 정책 학습 아님.
- **현재 배치 대조**: CMake에 action_codec_dump/placement_contract_test 추가. 실제 C++/Python encode/decode/expand 전체 도메인 대조, 기존 Python 전개/새분포 초기합계1615 통과(151-root-python.log). 실제 SimGame 열거/ApplyPlacement·잘못된 좌표 상태 보존·벽 너머 끝점과 입력 경로의 서로 다른 결과를 확인. sim_t_spin_test도 통과. 현재 C++ 배치 fixture ASan/UBSan 통과(52870 exit0). 즉시 배치 API의 의미는 유지하며 입력 경로와 동일하다고 주장하지 않는다.
- Part8§4를 원점·현재 행의 끝점·중복 확률/엔트로피·빈 마스크·기울기·T-spin 이력·시간 계약으로 재작성. 출처가 불명확한 고정 합법행동 평균 제거. “spawn 높이” 실제 주석을 live piece row로 수정. Part9 행동/분포 발췌·Python action_mask/input_expander 주석 동기화. Part8§4/Part9§3·5 partial, 관측 대응 보존. ONNX/전체 경로 탐색/학습 완료로 확대하지 않는다.
- Part발췌·Markdown27·749객관식·Mermaid11·lazy 자산·전체차시DOM탐색/앵커/답안/완료/소스참조·인라인/현재심볼/누적보존·정적ZIP 재현성/자산해시·diff 통과. release1fab726f76f31630,16파일. 로컬18767 lesson/library/course와 현재 모델·시뮬레이터·새 C++검사 source 해시 일치(151-http.log). 수동GUI 없음. Windows/macOS native·CUDA·ONNX·Colab 전체 학습 미검증; Python/PyTorch sanitizer 비계측.
- 실행 작업 모두 종료: root59789,초기CP22588,DeepSeek36359,초기분포11105실패→22494통과,확장회귀77729,수치guard최종85291,CP검사92023,root ASan52870,생성94202,DOM14365,사이트52166,나머지43507,최종드리프트63832(문서말미공백교정후diff재통과). 로컬18767 서버만 유지. 다음152는 실제 점수·공격·환경 보상과 shaping/종료·잘못된 행동 보상을 대조하고, 가변 지속시간과 관측에 생략된 상태의 전제를 함께 설명한다. module12 완료 후 중지; 새152원고/체크포인트 없음.

- 2026-10-04: **150차시 관측 제작/검수 완료(progress)**. 직전149도 실제 집필·바인딩·문서/뷰어 수정과 검증이 있어 progress다. 로컬1~150·744문제, 다음151 “행동: 매 틱 입력과 최종 배치”. module12 완료 경계에서 커밋하며 개별 차시 커밋·푸시·공개1~55 확대 없음.
- CP150-observation은149의 README와 bindings/session.h·study_py.cpp·bindings/CMake 외 누적 파일을 보존. Session 값 snapshot·카탈로그 순서 schema, python/observation.py·observation_demo.py, tests/observation_contract.py·observation_snapshot.cpp 추가. 원고150.json16절·5문제·인라인7. 상태 투영/부분 관측·Markov 전제·CHW/NCHW·stride/reshape/전치·범주순서·dtype 변환·배치·CPU 텐서 공유를 실제 구현과 연결한다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny·선택된 NumPy 변환 명세만 전송,1600 exit0. out/learning-jobs/150-observation-{material.txt,events.jsonl,draft.json}. primary가 float32 변환후검사(1+미세값/극소값이이진수로반올림되어통과)를 원래값검사후변환으로교정. ID정수형/음수index/중복목록/배치키·dtype검사보강. 키/DB/설정전송없음·다른Codex하위에이전트없음.
- 새 CPU PyTorch2.8.0+cpu를 /tmp/study149-python(CPython3.12.13)에 격리설치(83812 exit0). GPU/훈련없음. 공식PyTorch버전별설치안내를대조하여Linux/Windows CPU인덱스와macOS설치명령·Python버전/가상환경경로를구분했다. 이식성안내를네이티브실행증거로주장하지않는다.
- 독립bindings실제확장빌드·기존바인딩/새관측 CTest2통과. 비대칭2×3 fixture·불연속종류ID·실제모든카탈로그종류/고정/종료·NaN/Inf/복소/문자/shape/반올림전오류·정수ID/복사/stack/CPU tensor공유와수명검사. 새C++ snapshot ASan/UBSan통과(Python/PyTorch비계측). sameObservation/differentState 중력카운터실험이의도한부분관측을보임. 150-{contract,checkpoint,demo,snippets-final}.log.
- **현재 관측실제대조**: tests/observation_dump.cpp는실제SimGame+bot::observe를호출,root CMake의TETRIS_BUILD_TEST 타깃추가. python/tests/test_observation_parity.py에서같은시드/입력/가비지요청후hash와모든관측원소·shape/dtype/CPU/연속성을비교. 모든종류/잠긴셀/종료포함. 관측반복독립·현재legacy미등록ID영벡터/표시ID제외·숨긴가비지후동일배치의다른보드도검사. 최초패리티+이전바인딩8통과뒤shape거절검사추가.
- **현재 코드 보강**: Python 관측에서행열을뒤집은입력이그대로통과함을150-shape-before.log로재현. 모델shape와다르면ValueError로거절,bot/placement.cpp에는schema와SimGrid 행열 static_assert 추가. 현재정상관측/모델형상변경없음. 수정후관측5+이전바인딩4=9통과(150-root-final.log). actual C++observe/SimGame ASan/UBSan 입력·가비지실행도통과(150-root-sanitized{,-build}.log). Python관측모듈은torch/native없이import 가능함을 별도확인.
- Part8§3의“위치/시간은무관”·“가비지/레벨은단일에서무의미”·“info/보상으로관측누락이해결됨”단정을교정. shape/범주순서/저장소공유/실제두언어대조를추가. Part8§1과Part9§2의“관측직접검사없음”갱신, Part8/9현재builder발췌동기화. bot/placement.h의입력전개검사가관측도대조한다는주석교정. Part8§3/Part9§3은partial,기존Part8§1현재발췌해시갱신. 환경/모델/ONNX전체계약완료로확대하지않음.
- Part발췌·Markdown27·744객관식·Mermaid11·lazy자산·전체차시DOM탐색/앵커/답안/완료/소스참조·인라인7/현재심볼/누적보존·정적ZIP재현성/자산해시통과. 초기release2a0f47af4b286703→OS/Python설치안내보완후최종3651157f1f674057,16파일. 로컬18767 HTTP자료/현행obs·placement·dump source해시일치; 최종본문수정뒤150-http-final.log로재확인. 수동GUI없음. Windows/macOSnative·CUDA·ONNX·Colab학습미검증.
- 실행작업모두종료: 설치83812,DeepSeek1600,CP빌드19171,초기root70504,shape수정전40703의도실패,수정후29695,CP검사97697,생성35991,root ASan25321,DOM50120,기타검사43602,초기사이트84370,최종사이트61673. 로컬18767만유지. 새151원고/체크포인트없음.
- 다음151: current SimGame::LegalPlacements/ApplyPlacement와Python action_mask·input_expander/CPP expand_placement,Part8§4·Part9§3.2를대조한다. 누적Round에실제배치열거/적용을연결할때틱입력경로와수행순서·도달가능성·실패상태보존·행동index/합법mask를명시하고,관측의결정시점전제를함께설명한다. 단순순간이동으로틱입력과동등하다고주장하지않는다.


- 2026-10-04: **목표 재개 및149차시 제작/검수(progress)**. 직전 턴은 완료된 단원의 상태 확인·중지 재적용으로 새 산출물 기준 no progress였으며, 이번 active 목표 재개 요청 후149를 집필했다.134~148/외부리뷰는 ec7ff1f에 보관·중지 완료 이력. module-12(학습하는 봇)으로 이동했으며 다음 단원 경계에서 커밋한다. 공개1~55 동결·푸시/배포 없음.
- CP149-python-binding는148의 README/CMake 외 누적 파일을 보존. bindings/session.h·study_py.cpp·독립/하위 CMake·demo.py, tools/binding_oracle.cpp, tests/binding_contract.py·session_contract.cpp 추가. 원고149.json15절·5문제·인라인7, 실제 확장 모듈·값 소유·한 틱·복사/clone/reset·정수/예외·GIL/ABI·직접 C++ 경로 대조를 연결. 학습 grid는 list 복사, 현재 SimGame grid는 NumPy 복사/내부 블록 참조라는 차이를 명시. 모델/관측 텐서/환경 전체를 구현했다고 주장하지 않는다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny, Session의 선택 C++ API만 전송·65312 exit0. out/learning-jobs/149-binding-{material.txt,events.jsonl,draft.json}. primary가 없는 begin/end를 data/size로 고치고 실제 입력/해시 정의 헤더를 포함했다. Python 등록·CMake·검사·본문은 직접 집필/검수. 키/DB/설정 전송 없음, 다른 Codex 하위 에이전트 없음.
- 독립bindings Release 빌드와 누적SCRIPTED 구성의 같은 Python 타깃/계약 CTest 통과. 시드(전체uint64 경계 포함)·낙하간격·전체유효마스크/중립/종료열에 대해 직접 Round oracle과 모든 전이의 Step/점수/종료/정규bytes 일치. snapshot/clone/잘못된 정수·비트·reset 뒤 상태 보존, 종료후정상stopped/잘못된입력거절, 소유자삭제뒤복사본 사용 검사. C++ Session ASan/UBSan통과; CPython/확장모듈 전체계측 아님.
- 초기 uv 설치는 샌드박스캐시/네트워크로 실패 후 /tmp 전용cache와 승인된외부설치로 완료. 선택환경 /tmp/study149-python(CPython3.12.13,pybind11 3.0.1). 최초 ASan은 샌드박스 ptrace/LeakSanitizer 환경오류로 종료; 종료확인뒤 같은검사를 바깥에서실행하여 통과(149-checkpoint-final.log). 초기실패를통과로집계하지않음.
- 현재 tetris_py를 별도 새root빌드해 python/tests/test_binding_boundary.py4회귀 통과(149-root-{build,contract}.log). 현재 grid 독립NumPy·내부readonly블록의 값변화와부모수명·clone의 RNG포함독립상태·시드변환범위 확인. 이 범위에서 현재런타임동작의실패는없었고 주석/docstring만교정. 최초rootconfigure 인자 TETRIS_BUILD_TESTS는미사용경고였고 TETRIS_BUILD_TEST의기본값은유지; 실행검증주장은명시적으로빌드한tetris_py/4검사에한정한다.
- **실제 문서/뷰어 수정**: bindings/tetris_py.cpp의 “공유하면gap이아예없음”·복사비용영향없음·고정grid크기설명을교정. Part8의프로세스당sim한개·완전한차이방지·ctypes/cffi표준패키지/빌드설명·reference_internal의무조건수명보장단정을교정하고2.6언어경계검증추가. CMake의없는vendoredfallback주석삭제, Part8/9/13현재발췌동기화. Part8§1/2 partial 대응, Part13의발췌해시만갱신. 라이브러리허용root/본문경로추출에서빠진bindings/추가와python/sim/__init__.py명명으로바인딩참조버튼활성화.
- Part발췌·Markdown27·739객관식·Mermaid11·lazy자산·인라인7/현재심볼/누적보존·정적ZIP재현성/자산해시통과. release9c378797174b2fa8,16파일. 로컬18767 HTTP lesson/library/course 일치와/__learn/source의실제bindings해시일치. 수동GUI없음. 전체차시 DOM탐색/앵커/완료/답안/소스참조/저장불가경로도6317 exit0로통과(149-navigation.log). Windows/macOSnative·free-threadedPython·실제Colab/ONNX미검증.
- 모든작업종료: DeepSeek65312, 독립빌드61879, root53408, 초기검사11757환경실패→최종92569통과, 누적19116, 생성3566, DOM6317, 사이트32375, 퀴즈/lazy/도표83165. 로컬18767학습서버만유지.
- 다음150: python/common/obs.py와bot/placement.cpp의observe 및Part8§3을읽고, 누적Session에서관측형상·값범위·채널·활성피스/예고/시간상태의포함기준을구현한다. 현재Python/C++관측의실제패리티를먼저확인하고모델추론/학습미실행을환경완성으로확대하지않는다. module12마지막까지개별커밋없음. 새강의150은아직만들지않음.


- 2026-10-03: **148차시 및 큰 단원134~148 제작/검수 완료(progress)**. 직전147도 실제 집필/검증 변경이 있어 progress. 로컬1~148·734문제, module-11 모두 reviewed. 사용자의 중지 요청에 따라 누적 범위 감사/단원 커밋 후 goal paused로 전환한다. 전체 목표 완료가 아니다. 재개 지점은149이며 아직 원고/체크포인트를 만들지 않았다. 공개1~55 동결·푸시/배포 없음.
- CP148-abuse-review는147의 README/CMake 외 누적 파일 바이트 보존. meta/review_policy.h·review_sample.h·review_reader.h, tests/review_fixture.h·review_contract.cpp, tools/review_results.cpp·review_fixture.cpp 추가. 148.json16절·5문제·인라인7·전체파일5. 실제 AuthoritativeMatch fixture→기존 SQLite 저장→READONLY 제한 표본→쌍 정규화/관찰 분류를 연결한다. 자동 제재/실제 경제 상한 적용으로 과장하지 않는다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny, 선택된 순수 관찰 분류 명세만 전송·session68813 exit0. out/learning-jobs/148-review-{material.txt,events.jsonl,draft.json}. primary가 short-circuit 범위 검사·승리 합계의 overflow 없는 비교·임계값·한국어 사실 문구를 검토. 키/DB/설정 전송 없음, 다른 Codex 하위 에이전트 없음.
- READONLY open과 실제 SQLite read-only 확인·application_id/스키마 대조. 필요한 경기필드만 PK 내림차순 LIMIT+sentinel로 조회, 질의 완료 전 오류는 빈 정상목록으로 바꾸지 않음. canonical uint64 ID·순서/중복/승자/틱 검사. 정규화된 계정쌍에 무승부/양편승리/짧은경기 집계. 정책·표본 크기·ID범위·이전행 존재를 보고서에 남김. 임계값 신호는 의도 증명이 아니며 기저율/정밀도 가상예·NAT/주소/계정 차이·관찰과경제/제재 경계를 설명.
- SCRIPTED/SDL 각 새관찰contract·실제규칙/DB fixture와reader·누적bot contract 통과. 자리교환·draw·임계값·uint64끝·불가능한수·중복순서·빈/잘린표본·읽기전용/재시작·동일경기키·전테이블 불변 확인. contract/reader ASan/UBSan 통과(SQLite archive 비계측). vendorSQLite const/strlen 기존경고만 기록. 최초 원고fixture 스니펫의 닫는중괄호 경계를 전체작은헤더로 바꾸고 인라인7/현재심볼/누적보존 재검사 통과.
- **현재 코드/문서 대조**: meta/api_server.cpp RequestBudget의 rolling window 주석은 실제 주소별 최초 수락시각 기준 고정 길이창으로 교정. 런타임 정책 변경 없음. Part16§7.1 창경계 버스트·NAT·meta/relay 키차이, Part18§7.1 계정농사·규칙상가능한고의패배·별도UUID 반복지급·경제/관찰/제재 범위 추가. 세 절 partial 대응. 현재 동일상대별 PvP상한 부재는 새 실제meta회귀로 확인. DEPLOYMENT의 오래된 공개release고정문구는 PUBLICATION.json을 기준으로 바꿔 출판기록 중복을 제거.
- 현재 meta3HTTP 회귀 통과: distinct 경기키의 같은상대 BP/XP누적, 위조주소헤더 무시, 신뢰로컬proxy 오른쪽XFF. 최초 신규검사가 API에 없는 wins필드를 단정하여 실패했으므로 실제반환XP로 교정 후 통과. 첫선택은2건만 실행됐고 최종선택은이름을 바로잡아3건. 근거148-root-http-final.log. 저장포트fixture이며 사람의동기를입증하는검사로주장하지않는다.
- **단원 마감 회귀**: 현행 root137 서버/META/WSS/TEST 구성 전체빌드·CTest30통과(148-module-root-{build,ctest}.log). 별도등록형 시뮬레이션/Game 실행파일8통과(148-module-core.log). 인증·TLS·입장권·계정복구·결과검증·봇·DB 묶음134통과/3skip(148-module-http.log); skip은송신주입미설정3사례, 주입설정별도실행3통과(148-module-fault.log). GAME/ONNX/PY바인딩 전체빌드를검증한것은아님. 기존개별단원의그래픽/오디오증거는해당기록을보존.
- Part발췌·Markdown27·734객관식·Mermaid11·전체차시 DOM탐색/앵커/검색/저장/소스참조·lazy자산중복/닫기/재시도/도표·정적ZIP재현성/자산해시·로컬HTTP일치·diff통과. 초기release6c8134444ee72d34→course.js공백정리후최종fa59266a96002a50,16파일. 수동브라우저/스크린샷 없음. Windows/macOS native·Retina·ONNX추론·인터넷부하/운영오탐률미검증.
- **누적 범위 감사**: 기준22dfbc6 이후의 학습사이트/이전차시/관련런타임·회귀가모두미커밋인상태를확인. 148-commit-inventory.json에경로목록,148-secret-scope-audit.json에중복제거파일내용/키패턴/DB·빌드혼입검사기록. 검사시고유내용1957개에서private/APIkey후보및의도밖DB/바이너리이름없음. 새helper·빌드등록·기록에서파일명직접누락된변경을실제diff대조. 기존커밋누적을이단원경계에함께보관하며out로그/DB/키설정은제외. 감사문서는CHAPTER_11_AUDIT.md. CMake 헤더목록들여쓰기한곳교정. 이감사는모든플랫폼/보안의완전성을보증하지않는다.
- 외부모델68813, CP42333, 초기현재회귀16030실패→최종3건완료, module빌드/CTest26617, HTTP78410, 사이트/독립규칙1398, lazy86102, 탐색80369, fault81966 모두종료. 로컬18767학습서버만유지. 남은전체편성은그대로두고사용자재개전추가집필/푸시/공개확대없음. 커밋마감뒤도구상태paused를반드시적용하고작업중지.


- 2026-10-03: **147차시 봇 BP 제작/검수 완료(progress)**. 직전146도 실제 서버 오류 수정·집필·검증이 있어 progress. 로컬1~147·729문제. 다음148 “남은 어뷰징: 계정 농사·담합·고의 패배”. 이 단원 마지막 차시 완료→누적 범위 감사/단원 커밋→goal paused.149 시작 금지. 공개1~55 동결·푸시 없음.
- CP147-bot-rewards는146 누적 파일을 보존하되 README/CMake와 실제 결합에 필요한 meta/migrations.h·sqlite_results.h·tests/migration_contract.cpp만 수정. simulation/bot_replay.h, meta/bot_challenges.h·bot_reward_policy.h·bot_reward_schema.h·bot_settlement.h, tests/bot_fixture.h·bot_reward_contract.cpp, tools/bot_reward_probe.cpp 추가. 원고147.json16절·5문제·인라인11·전체파일5. 현재 모델/Controller를 미리 복사하지 않고 누적Round/Duel에 공식 간격 입력 정책을 연결하여 보상 흐름을 실행한다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny로 순수 지급 계산 명세만 전달·session71938 exit0. out/learning-jobs/147-policy-{material.txt,events.jsonl,draft.json}. primary가 음수 거절·비교 후 차감·INT64_MAX·0 지급 의미를 직접 검수하여 적용. 최초 상대경로 복사 실패(25794 exit1) 후 절대경로로 생성/재호출했으며 실패를 성공으로 집계하지 않음. 키/DB/설정 전송 없음, 별도 Codex 하위 에이전트 없음.
- 챌린지는 신뢰된 계정/시드/공식 상대 revision·발급 시각에 묶임. 형식/비트/틱 상한 뒤 서버 단조 시간의 TickAllowance 비교. 전체 용량·계정별 교체·충돌/완료영수증 재사용 거절·만료, try-lock 검증기, 복사본 재현 뒤 현재 티켓 재조회. 잘못된 증거 소모, 저장/검증 예외 미확인 재시도, completed receipt는 진행 표보다 먼저 조회. issuer/clock/UTC일/Verify는 신뢰된 내부 포트이고 프로브 고정값은 fixture임을 명시.
- 승리 재현은 누적Duel로 양쪽 진행/공격 교환, 첫 종료==기록 끝 및 사람 생존/봇 종료 조건. 기록의 잘림·추가suffix·금지bit·동시종료·알수없는 정책 revision 거절. 학습 디코더는 임시 vector를 완성해 출력에 대입하며 실패 시 기존값 유지. 현재 decoder는 부분 출력 가능하므로 반환값 false를 사용 금지로 설명.
- 공용 wallets와 bot_rewards 영수증을 BEGIN IMMEDIATE로 함께 저장. 티켓·계정·상대/revision 중복 일관성, UTC일 획득 영수증 합계로 남은 지급량 계산, 부분/0 지급도 영수증 저장. 재시작·다음날 재청구에도 같은 지급량, 지출로 한도 회복 없음. RP/XP/PvP 경기 불변. 스키마v7로 신규표/인덱스를 이행하고 기존지갑/선택아이콘/경기 보존.
- SCRIPTED/SDL 각각 bot contract(실제Duel+SQLite)·새프로세스프로브·기존migration/reward contract·notice CTest 통과. 검증 중 supersession, 별도스레드busy, admission capacity, 지급3지점 fault rollback, 지갑 상한, 별도 SQLite연결 두 개 동시동일티켓, DB재개 검사. ASan/UBSan bot contract와 실제 DB 프로브 통과(SQLite 정적 archive는 비계측). 신규C++ 경고 없음; vendor SQLite의 기존 const/strlen 경고만 기록.
- 초기 CMake 빌드86871은 lifetime 변수 오기로 실패→lifetime_ns 교정. 검사2058은 bot/probe/ASan은 통과했지만 migration/reward는 CTest 등록이 없어 실행하지 않았음. 등록형 notice만CTest, 인자형 DB검사는 temp DB로 직접실행하도록 checker 교정 후43388 최종 전체 통과. 이 초기 누락을 회귀검증으로 주장하지 않음. 최종로그147-checkpoint-final.log 및147-snippets.log.
- **현재 게임 회귀 추가**: python/tests/test_bot_rewards.py에 실제 meta/SQLite의 일일 부분/0 지급, 지갑 상한으로 INSERT+UPDATE 롤백, 같은 티켓 재시도와 소유자 검사 추가. 기존 포함6HTTP 통과(147-root-http.log). tests/bot_replay_test.cpp는 terminal suffix·금지bit 뒤 actual SimGame/Controller 검증거절 추가. Release CTest와 root ASan/UBSan CTest 통과(147-root-replay.log/147-root-asan.log). 현재 런타임의 확정 오류는 이 범위에서 발견하지 않아 동작 변경 없음. ONNX 미설치 경로의 heuristic fixture이므로 실제 모델/OS간추론 검증으로 과장하지 않음.
- Part15§6의 고정 용량/시간/지급량 설명을 정책 출처로 교체. 주기 시간 검사는 모델로드/단일추론을 선점하지 않는다는 실제 한계, verification 후 티켓재조회,0지급/rollback/프로세스 수명/모델파일 경로와내용버전의 차이를 보완. Part18§5에 공식상대 선택·계정/입력/저장 경계 연결. Part15§6 새 partial 대응, Part18§5 기존 partial/hash 갱신. 모델파일 pinning/Runtime배포·전체계정UI 완료로 주장하지 않는다.
- Part발췌·Markdown27·729객관식·Mermaid11·현재심볼/인라인/누적보존·정적ZIP재현성/자산해시·로컬HTTP일치·diff통과. releasefab64772a8a21fb8,16파일. 수동GUI·공개배포·중간커밋 없음. Windows/macOS native·실제ONNX·실사용부하 미검증.
- 외부모델71938, 초기빌드86871실패, 현재서버4692완료, 초기검사2058완료, 최종검사43388완료, 최종root/snippet88018완료, rootASan60561완료, site10935완료. 실행 중 외부 작업 없음, 로컬18767서버 유지. 다음148은 실제 계정/주소별 제한과 재화/반복대전 정책을 근거로 가능한 승리와 사람/동기 검증의 차이·오탐·운영 대응을 구현/설명하고 단원 범위 감사한다. 전역누적dirty에 과거집필/런타임 변경이 많으므로 git diff와PROGRESS/REVIEW_LOG 근거를 대조하여 커밋 범위를 정하고, 사용자별도변경을 임의로 덮어쓰지 않는다. 커밋 후 update_goal paused 및 중지. 전체 goal complete로 바꾸지 않는다.


- 2026-10-03: **146차시 판정 사유·결과 통지 제작/검수 완료(progress)**. 직전145도 실제 구현/검수 변경이 있어 progress. 로컬1~146·724문제. 다음147 봇 BP,148 남은 어뷰징. 단원134~148 완료→누적 범위 감사/단원 커밋→goal paused 예약 유지.149 시작 금지. 공개1~55 동결·푸시 없음.
- CP146-result-notices는145의 README/CMake 외 누적 파일 보존. notice_reason/result_notice/final_output, result_notice_contract·notice_probe 추가. 원고146.json16절·5문제·인라인7·전체파일5. 판정/저장 상태의 조합, 알 수 없는 사유와 프로필 보존, 한 FIFO의 부분 송신, 고정 기한의 배수 종료, kernel 수락/수신/저장 의미를 구분한다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny, 선택 상태 인터페이스 명세만 전송·session10523 exit0. out/learning-jobs/146-notice-{material.txt,events.jsonl,draft.json}. primary가 사용자 문구의 내부 전송 증명 설명을 제거하고 incomplete/budget/saved 표현과 enum 형식을 교정. 키/DB/설정 전송 없음. 별도 Codex 하위 에이전트 없음.
- **145에서 발견한 현재 서버 통지 문제 해결**: reactor send_result_frames의 socket 사본 직접 send를 live Conn의 queue_send로 교체. 기존 프레임 뒤 FIFO·partial suffix 보존. Channel sockA/B 사본 제거. delivering_result가 queue_send 실패→close_conn 재진입 중 상대 종료를 지연한다. 상대 이탈 시 결과 enqueue 후 drain_and_close; 신규 읽기/송신 유입 차단·쓰기 관심·고정 만기·빈 큐 종료. pause_peer_read가 draining 읽기를 재개하지 않으며 만기/오류/관심 실패는 기존 자원 반환 경로로 정리한다. shutdown의 원격 수신 보장은 주장하지 않는다.
- tests/learning/result_send_fault.c는 테스트 relay에만 LD_PRELOAD로 실제 send의 짧은 성공/EAGAIN/영구 정체/첫 대상 오류를 주입. 수정 전 reactor 결과 통지3사례가 불완전 프레임으로 실패(146-before-short-send.log), 수정 후 동일3통과. 실제 meta/SQLite 및 두 relay의 최종 정상/이탈/동시 제출/무효/허위 주장23통합 통과(146-root-http-final.log). 신규 FIFO/finite-close/fail-first3은 DB 미지급도 확인. 역사적 인증 호환 fixture로 새 TLS/입장권/실사용 부하 검증과 구분.
- 현재 reactor를 새 ASan/UBSan 구성으로 빌드하여12통합 통과, 프로세스 종료코드와 stderr의 sanitizer 보고 부재 확인(146-root-sanitized-http.log,session45555 exit0). LD_PRELOAD는 libasan→faultshim 순서, meta는 일반 빌드. peer reset/lobby disconnect/quiet survivor3통과. tx-budget은 두 relay 경로를 명시한 재실행1통과(146-tx-budget-current.log); 최초 암묵 경로 실행은 수정된 바이너리의 증거로 쓰지 않는다.
- CP SCRIPTED/SDL 각6CTest 통과: notice contract/poll·epoll TCP와 누적owner/pacing/submission. 상태 조합·전체raw byte·키·길이·출력 불변·FIFO·would-block·고정 만기·오류·객체 파괴 뒤 예산 반환 검사. 실제 TCP는 부분 쓰기/읽기 후 앞선 프레임→완전 통지→EOF. contract와poll/epoll 프로브 ASan/UBSan 통과. 현재 ResultStatus의256값에서 Applied/Draw만 reportedRP 적용·나머지 기존값 유지 검사도ASan/UBSan 통과. 마지막 명시헤더/enum qualification 수정 뒤 전체 해당검사 재실행68076 exit0.
- Part14의 상태/Channel/queue_send/write/pause/survivor 현재발췌와 소유 설명, Part18§3 FIFO/재진입/배수 의미 수정. §4 프로필/사유와 연결. 대응 해시5절 갱신, 부분 대응 범위 유지. 실제 UI 파일은 src/main.cpp이며145 기록의 src/app.cpp는 탐색 예정 경로 오기다. net/session.cpp와net/match_result.h 현재 소비 동작 대조; wire/기존UI동작 변경 없음.
- Part발췌·Markdown27·724객관식·Mermaid11·인라인/현재심볼/누적 보존·정적ZIP 재현성/자산해시·로컬HTTP일치·diff통과. 최종release9475217cb733b336,16파일. Windows/macOS native·실사용 경기 부하 미검증. 수동GUI·공개배포·중간 커밋 없음.
- 근거 out/learning-jobs/146-{checkpoint-final,part,coverage,library,lessons,markdown,quiz,diagram,site,current-status,root-http-final,root-sanitized-http,tx-budget-current}.log 및 out/learning-checkpoints/146-result-notices-check/*build.log. 초기프로브ReceiveState closed 오기를eof로 수정 후 빌드; 실패를 통과로 집계하지 않음. 외부모델/검사 세션 모두 종료, 로컬18767서버 유지. 다음147은 실제 봇 검증 경로·학습 체크포인트 연결·공용BP 지급 조건을 대조한다.148 끝에서만 누적 감사/커밋·paused.


- 2026-10-03: **145차시 두 릴레이의 실행 소유권 제작/검수 완료(progress)**. 직전144도 구현/교재/검사 변경이 있어 progress. 로컬1~145·719문제. 다음146 “판정 사유: 무효·미완료·저장 불확실”. 단원134~148 완료→누적 범위 감사/단원 커밋→goal paused 예약 유지.149 시작 금지. 공개1~55 동결·푸시 없음.
- CP145-relay-ownership는144의 CMake/README 외 누적 파일 보존. net/result_handoff.h·relay_channel.h·thread_relay_channel.h·loop_relay_channels.h, tests/relay_owner_contract.cpp, tools/relay_owner_probe.cpp 추가. 원고145.json16절·5문제·인라인10·전체파일6 reviewed. shared policy와 mutex/owner-loop 실행 어댑터를 분리하여 실제 스레드/저장으로 연결.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny, 선택한 handoff 인터페이스 명세만 전달·session30036 exit0. out/learning-jobs/145-handoff-{material.txt,events.jsonl,draft.json}. primary가 복사된 선점 권리 방지를 위해 copy 삭제·key const·시뮬레이션 동기화 설명을 교정. 초기 /tmp cwd에서 상대 material 경로 생성 실패 후 절대 경로와 set-e로 수정. 키/DB/설정 전송 없음. 별도 Codex 하위 에이전트 없음.
- RelayChannel은 PacedMatch와 ResultHandoff의 같은 소유 경계; claim 이후 입력 sealed, 완결 record만 저장. not_eligible와 inflight/confirmed/unconfirmed 분리. ThreadRelayChannel은 clock을 잠금 뒤 호출하고 claim/request 복사를 같은 임계 구역에서 수행, sender 잠금 밖 실행, 응답 잠금 안 적용. view는 값 복사. 모든 호출 스레드 종료 후 객체 파괴 계약.
- LoopRelayChannels는 생성 스레드의 소유권 검사, MonotonicId 조회 키와 durable 경기 키 구분. Offload는 요청 값만 캡처하여 sender 실행; 완료 함수만 레지스트리에 접근. 삭제 ID의 늦은 완료 무시, ID 재사용 없음. 용량/종료 거절·job 예외도 unconfirmed 전이. shutdown은 신규 작업 차단→수락 작업 join→owner drain/완료 실행, 다음 객체 파괴. 원격 작업 기한은 sender 책임, 로컬 제거는 원격 취소가 아님.
- **현재 코드 개선**: server/relay.cpp finalizeRanked의 summaryHandled 선점과 verified->result 복사를 하나의 sumMu 구간으로 묶음. 사이의 재잠금 틈에서 입력 관찰이 끼어들던 순서 모호성을 제거. 실제 악용 재현을 주장하지 않으며 저장 호출/송신은 계속 잠금 밖. python/tests/test_match_summary_crosscheck.py에 barrier로 동시·반복 양쪽 요약 제출 추가, 실제 한 경기/기대 보상 확인.
- Part18§3에 원자적인 선점/스냅샷·잠금 밖 I/O·값 캡처·ID 재조회/재사용금지·join/drain·원격 저장/로컬 생존 구별 추가. Part10§17 현재 finalizeRanked 발췌를 수정, Part7§13 말미의 “신뢰 경계를 없앤”을 “자기 신고 대신 규칙 재현”으로 교정. coverage part18--section-04 부분 대응 새 연결, part10--section-18/part7--section-14 기존 대응 해시 대조 갱신. 현재 소스 라이브러리/HTML 갱신.
- SCRIPTED/SDL 각5CTest 통과: 새 owner, 누적 offload/판정/pacing/submission. 실제 thread 수신·clock 동시 진입 부재·느린 sender 안 view·동시 최종화 한 선점/한 sender·입력 봉쇄·미완료 저장 없음. loop의 용량 거절/worker와 owner 구분/삭제된 채널 응답/새 ID/잘못된 owner/예외/잘못된 영수증/종료 drain 확인.
- 실제 SQLite 프로브는 두 owner에서 규칙 종료→저장. 같은 DB에 새 프로세스로 동일 키 재실행 후 matches/wallets/careers/match_rewards/match_progress 전체 행 일치. 신형 소유권 contract ThreadSanitizer 및 ASan/UBSan 통과. 실제 SQLite C++ 경로도 ASan/UBSan 통과(기존 SQLite 정적 archive 비계측). 기존 vendor SQLite const/strlen 경고 로그 보존. 신규 테스트 misleading-indentation 경고 교정 후 신규 타깃 경고 없음.
- 현재 두 relay/ranked_game_test Release빌드 및 meta/SQLite20통합 통과: 정상 terminal의 동시 반복 요약/일반 요약/연결 종료, 허위 일치 주장, 입력 무효, 미완료 이탈. 역사적 인증 호환 fixture이며 새 TLS·입장권 통합/실사용 부하 검증으로 주장하지 않음. Windows/macOS native 미검증. 수동GUI/공개배포/커밋 없음.
- Part발췌·Markdown27·719객관식·Mermaid11·인라인10/현재심볼/누적보존·정적ZIP 재현성/자산해시·로컬HTTP일치·diff 통과. release272c33eb1e3f57d2,16파일. 최초 검사 도구의 match_awards 오타를 실제 match_rewards로 수정 후 재실행; 초기 사이트 검사에서 Part10의 stale 현재발췌 검출하여 동기화 후 통과. 실패를 성공으로 집계하지 않음.
- 근거 out/learning-jobs/145-{configure,build,test,contract-final,root-build,root-http,tsan-build,tsan,snippets,part,coverage,library,lessons,markdown,quiz,diagram,site}.log와 out/learning-checkpoints/145-relay-ownership-check/*build.log. 외부모델30036, 초기빌드11878, root76425, 검사99429실패→34424완료, TSan21155, 사이트14820실패→79305완료 모두 종료. 로컬18767서버 유지.
- **146에서 반드시 처리할 실제 소스 문제**: server/reactor_relay.cpp send_result_frames(약2205)는 일반 Conn::tx를 우회해 tcp_send_some을 직접 호출하고 sent를 버린다. 기존 대기 프레임의 뒤에 넣지 않아 순서가 깨질 수 있고, would-block/partial suffix가 유실될 수 있음. net/socket.cpp와 tests/learning/socket_stream.cpp가 tcp_send_some의 부분 성공을 명시적으로 증명. close_channel_survivor는 결과 뒤 즉시 close_conn하여 tx를 버리므로 queue_send로만 바꿔도 종료 시 유실. 현재검사는 작은 결과/loopback 정상 송신만 확인했으므로 미해결로 명시; 사용자에게 다음 통지 차시에서 수정·검증한다고 알렸음. Part18§3 끝에 소유권과 송신 계약 분리 설명을 남겨 과장 방지.
- 다음146은 net/match_result.h·net/session.cpp·src/app.cpp의 사유/프로필 적용을 대조하며 위 결과 전달을 함께 수정한다. 결과를 기존 tx 뒤에 보관하고 live Conn에 일반 write-interest로 배수; 생존자 종료는 유한 기한의 drain-then-close가 필요. on_readable/read_paused·on_writable·on_timeout(2071)·pause_peer_read가 종료 대기를 다시 풀지 않게 검토. queue_send 실패가 close_conn으로 재진입하여 상대를 결과 enqueue 전에 닫을 수 있으므로 두 대상 결과 발행 중의 finalize/closing 보호도 검토. queue budget/interest failure/기한/둘 다 끊김/shutdown 회귀 필요. 기존 kernel grouping에 기대지 말고 실제 syscall partial/EAGAIN fault injection(테스트 linker wrap 또는 선택적 LD_PRELOAD)으로 수정 전 실패를 재현하는 방안 고려. 현재 코드 변경은 이 문제에 아직 적용하지 않았음.
- 147 봇 BP,148 남은 어뷰징 검수 뒤 단원 범위 감사/커밋·paused.149 시작 금지. 전체 goal complete로 바꾸지 않는다.


- 2026-10-03: **144차시 입력 검증·시간 허용량 제작/검수 완료(progress)**. 직전143도 실제 집필·검사 결과가 있으므로 progress. 로컬1~144·714문제, 다음145 “두 릴레이: 같은 판정과 다른 실행 소유권”. 단원134~148 완료→누적 범위 감사/단원 커밋→goal paused 예약 유지.149 시작 금지. 공개1~55 동결·푸시 없음.
- CP144-input-pacing는143의 README/CMake 외 누적 파일 보존. net/tick_allowance.h·paced_match.h·paced_input_stream.h, tests/pacing_contract.cpp, tools/paced_probe.cpp 추가. 144.json16절·5문제·인라인12·전체파일5 reviewed. 계산기와 입력 수신/판정 소유 분리; 기존 untimed AuthoritativeMatch는 독립 기준 실습으로 보존, 새 보호 경로의 내부 mutable 접근은 없음.
- DeepSeek OpenCode deepseek/deepseek-flash 선택 명세 초안 사용(/tmp,tools deny), session71027 exit0. out/learning-jobs/144-pacing-{material.txt,events.jsonl,draft.json}. 키/DB/설정 전송 없음. 정수 허용량 초안을 직접 범위 증명/독립 oracle로 검토하고 연결 어댑터·테스트·원고를 작성. 다른 Codex 하위 에이전트 사용 없음.
- 허용량은 min(max,lead+floor(elapsed_ns*rate/1e9)). 몫/나머지 분리·곱하기 전 상한·남은 용량 비교, 생성자 정책 범위 검증. 경기 활성화 기준은 고정, 서버의 신뢰된 단조 시각만 주입. 권한/종류/디코딩/경기 범위 뒤 시간 검사, 전체 끝 위치→시각 역행→시간 한도 뒤 규칙 submit. stored/duplicate만 수용 시각 갱신. 실패는 경기 abort·기록 차단, 시계 오류와 too_fast 사유 분리.
- signed 시각의 순서 확인 후 unsigned 차로 INT64_MIN~INT64_MAX 거리도 정의된 계산. typed frame 밖에서 now를 주입. TCP 분할 수신 프로브는 실제 소켓/관찰 대기 시계와 결정론적 테스트 시각을 구별. 시간 계산은 패킷/바이트 예산·토큰 버킷·사람 조작 증명과 별도임을 설명.
- 현재 런타임의 확정 버그는 발견하지 않아 동작 변경 없음. tests/ranked_game_test.cpp에 현재 정책의 초기 여유·1초 경계/직전·재전송 후 여유 재지급 없음·앞선 보관 창·전체 범위·빈/잘린 본문 검사 추가. python/tests/test_match_summary_crosscheck.py에 length/tick_overflow 통합 추가. Part18§2.1은 고정 정책 수치 대신 값 출처/계산식, 반열린 구간·범위 검사·초기 여유·단조 시계·첫 INPUT/활성화 및 연속 기록/유한 창 차이로 보완. coverage hash/부분 대응 갱신.
- SCRIPTED/SDL 각7CTest 통과. 모든 입력 byte·형식·전체 한도·권한·wronground·고정 기준·분할 제출·중복/충돌·미래 구멍·stale·창 상한·부분 변경 방지·시각 경계/역행/극한·시간 실패 뒤 입력 종료·완결 기록 검사. actual TCP poll/epoll 정상 종료/직접 규칙 비교와 조기 묶음 무효 확인.
- 새 학습 contract와 소켓 프로브 ASan/UBSan 통과. Python 임의 정밀도12345사례(전구간·포화 전·한 틱 증가 경계±·정책/정수 끝 값)과 정수 계산 일치. 현재 RankedGame/direct SimGame 검사도 새 시간/창/범위 검사를 포함해 ASan/UBSan 통과. 신규 및 현재 타깃 빌드 경고 없음.
- 현재 root137 ranked_game_test 재빌드 및 정확한 CTest 이름 ranked_game 통과. 초기 '^ranked_game_test$' 조회는 No tests were found로 실제 검사 없이 종료되어 증거로 쓰지 않았으며 --no-tests=error와 올바른 이름으로 교정. 실제 thread/reactor relay+meta+SQLite의 잘못된 입력10회귀 통과. 역사적 인증 호환 fixture, 실제 지연 임계는 주입 테스트로 확인하므로 실사용 오탐률 검증으로 과장하지 않음.
- Part·Markdown27·714객관식·Mermaid11·인라인12/현재심볼/누적보존·정적ZIP 재현성/자산해시·로컬HTTP일치·diff 통과. release09dd1d4045781c1a,16파일. Windows/macOSnative·실사용 지연/오탐률·사람 조작 여부 미검증. 수동GUI·공개배포·커밋 없음.
- 초기 임시 원고 생성 스크립트의 중첩 괄호 SyntaxError를 수정한 뒤 원고 생성/스니펫 검사 통과. 라우터 파일명은 scripts/check_learning_checkpoints.py(복수형). 잘못 탐색한 relay_server.cpp/relay_reactor.cpp는 없고 실제 파일은 server/relay.cpp와 server/reactor_relay.cpp다.
- 근거 out/learning-jobs/144-{contract,contract-final,root-build,root-test,root-http,part,coverage,library,lessons-final,markdown,quiz,diagram,site}.log와 out/learning-checkpoints/144-input-pacing-check/*build.log. 외부모델71027, 초기검사57249, root95028, 최종검사22174, 생성55516, 사이트9771 모두 종료. 로컬18767서버 유지.
- 다음145: server/relay.cpp Channel::sumMu/verified·결과 선점과 HTTP 잠금 밖 실행, server/reactor_relay.cpp Channel::verified·offload·continuation 소유 loop를 실제 소스로 대조한다. CP144에는 PacedMatch single-owner 계약만 있으므로 실제 동시 도착·결과 제출 단일 선점·종료 중 완료 응답 수명까지 두 실행 모델의 실습 필요.146 상태 사유,147 봇 BP,148 남은 어뷰징 뒤 감사/커밋·paused.149 시작 금지.


- 2026-10-03: **143차시 서버 재현 판정 제작·검수 완료(progress)**. 직전142는 실제 코드/강의/검사 변경이 있어 progress로 분류한다. 로컬1~143·709문제. 다음144 “입력 검증: 비트·순서·양·진행 시간”. 사용자 예약 유지: 단원134~148 완료→범위 감사/단원 커밋→goal paused.149 시작 금지. 공개1~55 동결·자동 푸시 없음.
- CP143-authoritative-simulation는142의 CMake/README 외 누적 파일 바이트 보존. net/authoritative_match.h·match_input_stream.h, tests/authority_fixture.h·authoritative_contract.cpp, tools/authoritative_probe.cpp 추가. 143.json16절·5문제·인라인9·전체파일4·Mermaid 흐름 reviewed.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny로 선택 인터페이스/명세만 전달·exit0. out/learning-jobs/143-match-{material.txt,events.jsonl,draft.json}. primary가 정의되지 않은 Actor를uint64로 교정하고 생성자의 경기/계정32비트 축소를64비트로 수정. 예산이 진행을 “구동”한다는 주석을 입력쌍 진행/예산 제한으로 교정. 키/DB/설정 전송 없음.
- AuthoritativeMatch는 서버 seed의 독립 Round 값들을 Duel로 소유하고 기존 InputAuthority 공통 입력 쌍만 소비. malformed/conflict는 무효, 다른 권한/범위 거절은 진행 없음. 성공한 틱만 세고 두 보드 진행/공격 delta 뒤 첫 종료 스냅샷. 마지막 허용 틱의 실제 종료를 예산 소진보다 먼저 처리. 종료 뒤 묶음 suffix 진행 없음, 완결 후 입력 inactive. 유효하지 않은 스트림 소유자는 abort 호출. 시간 기반 pacing은144 범위이며 이 단계의 유한 계산 예산만으로 빠른 진행 방지를 주장하지 않음.
- 완결 상태에서만 MatchRecord로 변환하고 서버에 고정된 HOST/PEER 계정 ID를 사용. 라인 수 축소 전 범위 검사. MatchSubmission 저장 포트 연결 검사는 대역이며 실제 지급과 구분. actor는 인증 완료 fixture, 학습 wire는 현재 INPUT과 다름. 현재 RankedGame의 전체 입력 기록/종료 후 무효화 가능성과 학습 창/완결 입력 종료 정책 차이를 본문에 명시.
- 이번 범위에서 현재 런타임 오류는 발견하지 않아 동작 변경 없음. tests/ranked_game_test.cpp에 네트워크 배치 없이 두 SimGame을 직접 진행하는 oracle 추가: seed·편별 neutral/drop·동시 종료·배치 크기·양쪽 도착 순서를 바꿔 승패/점수/라인 비교. Shared rule의 모든 오류를 독립 증명한다는 주장 없음.
- Part18§1.2에 공통 접두사·중립/누락·두 보드 후 공격 증가분·첫 종료/수신 종료/저장 확정 구별·Applied 이름의 판정 범위·직접 비교의 한계를 보완. §1/2 coverage 부분 대응/해시 갱신. 코어 검증·소유권·전체 어뷰징 정책 완료로 과장하지 않음.
- SCRIPTED/SDL 각7CTest 통과: authoritative contract와 poll/epoll actual TCP, 누적 combat/lockstep/input-authority/match-submission. 최종 테스트에 종료 틱 뒤 같은 묶음 suffix와 malformed 후 final 거절 추가. 상태 바이트·통계·배치/순서·missing/neutral·draw·예산·full-width IDs·제출 게이트 확인. 신규/현재 타깃 경고 없음.
- 학습 contract와 실제 TCP poll/epoll 소스 직접 빌드 ASan/UBSan 통과. 현재 RankedGame/SimGame 직접 비교도 ASan/UBSan 통과. 실제root thread/reactor relay·ranked_game_test Release빌드/CTest 및 meta+SQLite 통합14사례 통과: 허위 일치 신고, 정상 terminal 뒤 거짓 신고/상대 종료, 잘못된 입력3종, 미완료 생존자 신고 보상 없음. 역사적 인증 호환 fixture이므로 새 TLS/입장권 통합을 검증했다고 주장하지 않음.
- root137 CMakeCache에 TETRIS_BUILD_RELAY=ON,TETRIS_BUILD_REACTOR=ON을 추가(142의WSS/Boost설정 유지). root 실행파일과 새 학습 바이너리 위치는 각 out/learning-checkpoints 아래. 마지막 테스트 표준 헤더 명시 후 해당 타깃 재빌드/실행 통과.
- Part발췌·Markdown27·709문제·Mermaid11도표 parse·인라인9/현재심볼/누적보존·정적ZIP 재현성/자산해시·로컬HTTP일치·diff 통과. 마지막 ranked_game_test 헤더 변경 뒤 library가 stale이라는 export검사 실패를 확인하여 library/lessons 재생성 후 통과. 최종release9b0fc7864b0ec445,16파일. 수동GUI·공개배포·커밋 없음. Windows/macOS native·실사용 경기·장치 장애·사람 조작 여부 미검증.
- 근거 out/learning-jobs/143-{final-verification,root-build,root-test,root-http,contract-final-build,current-final-build,part,site}.log 및 out/learning-checkpoints/143-authoritative-simulation-check/*build.log. 외부모델78855, 초기빌드40721, root73045, 검사45721/61113, 사이트3710/75512실패→34061원인확인→4095완료, 최종컴파일12006 모두 종료. 로컬18767서버 유지.
- 다음144: 현재 RankedGame::observe의 mask·from/count·중복/변경·연속성·max_ticks·미진행 앞선창·실제 경과시간 slack 검사를 읽고 CP143에 필요한 시간/입력 정책을 명시적으로 더한다. 현재core/src 입력검사와 CP InputAuthority/TickInputs는 policy 차이가 있어 wire·ID·오래된 입력·허용 창을 혼동하지 않는다. 프레임 경계는 기존 파서 재사용. 실제 시간은 주입 가능한 monotonic clock으로 검사하고, 틱 예산과 rate/pacing을 구분.145 두 relay 소유권,146 상태 사유,147 봇 BP,148 남은 어뷰징 뒤 단원 커밋·paused 예약 유지.


- 2026-10-03: **142차시 복구 저널 제작·검수 완료(progress)**. 직전 141 턴도 구현·실제 오류 수정·검증이 있으므로 progress로 분류한다. 로컬 1~142·704문제. 다음143 “서버 시뮬레이션: 신고 대신 재현 판정”. 사용자 예약 중지 유지: 단원134~148 완료 → 범위 감사/단원 커밋 → goal paused.149 시작 금지. 공개1~55 동결·자동 푸시 없음.
- CP142-recovery-journal는141의 CMake/README/meta/local_account_store.h 외 누적 파일 보존. journal_flow/wire/store/http와 account_journal CLI·journal_contract 추가. 142.json16절·5문제·인라인7·전체파일4 reviewed. connect는 저널 재개 후 idle일 때만 일반 부트스트랩; 일반 LocalAccountStore도 활성 pending 및 접근 파일 없이 남은 복구 자료를 신규 프로필로 처리하지 않음.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny로 선택한 타입·순서 명세만 전송·exit0. 이벤트는 text/step만 있었음. out/learning-jobs/142-journal-{material.txt,events.jsonl,draft.json}. primary가 “ID 일치가 응답의 증명”이라는 초안 주석을 일관성 검사/TLS 책임으로 교정하고 enum 처리·교체 후 false 설명을 보완. 키/DB/설정 전송 없음.
- JournalStore는 기존 account.lock을 전체 작업 동안 소유. origin/모든 계정 문서·엄격 JSON·필드 간 관계 확인 후 CSPRNG 후보 생성→비공개 원자 pending 저장→동일 HTTP 요청. accepted+기대 ID 확인→복구 파일→접근 파일→origin/idle 표시. 현재 게임의 unlink와 달리 학습은 기존 원자 writer로 비밀 없는 완료 표시를 저장하여 별도 durable-unlink 구현을 요구하지 않음. 물리 안전 삭제·같은 사용자 악성 코드 격리 주장 없음.
- **현재 코드 오류 재현/수정**: meta/account_client.cpp resume_locked는 문자열 형식만 검사하여 backup/rotate/recover의 값 관계가 깨진 저널을 서버로 보내고400거절 뒤 삭제했음. 실제 wss_probe+meta 테스트3사례 수정 전 return9 재현. 관계 검사 추가로 전송 전 손상 상태 return8·파일 바이트 보존·epoch0 유지. 기존 backup/rotate/restore·부분 저장·다른 origin·프로세스 잠금·응답 유실까지 실제 native8회귀 통과.
- Part17§5에 필드 관계 검사·손상/서버 거절 구별·독립 파일 교체·rename 뒤 동기화 실패와 재시작 관찰을 보완. coverage 부분 대응/해시 및 현재 코드 라이브러리 갱신. 사용자용 본문에는 집필 환경 결과 보고를 넣지 않음.
- SCRIPTED/SDL 계정 저널·부트스트랩 타깃 빌드 및 각 journal/bootstrap CTest 통과. 실제 HTTP 프록시+DB+파일+새 프로세스로 응답 유실/다른 ID/손상 성공 본문/429/503, pending·복구·접근 저장 직후 _Exit, 접근/정리 실패, 실제 교체 뒤 false 보고를 검사. 재시도 본문 동일·ID 유지·세대 단일 증가·0600·비밀 출력 부재·활성 저널의 기존 부트스트랩 우회 차단 확인.
- 손상/다른 origin/의미 오류/디렉터리 경로/프로세스 잠금에서는 전송 없음. 실제 복구 파일에서 접근 키를 재생성하고 확정 거절의 완료 표시도 검사. 새 저널 CLI와 private_file/account_file_lock C++를 ASan/UBSan으로 빌드해 동일 HTTP/파일/재시작 행렬 통과. 서버와 기존 bootstrap 바이너리는 비계측임을 구분.
- GNU15 sanitizer 빌드의 표준 regex 내부 maybe-uninitialized 경고와 기존 vendor SQLite const 폐기/strlen 경고는 로그 보존. 경고 억제·라이브러리 전체 무결성 주장은 하지 않음. Windows/macOS native·실제 전원/장치 장애·공격자가 같은 사용자 폴더를 변경하는 경쟁 미검증. 수동GUI/공개배포/커밋 없음.
- 최초 root137 빌드에는 WSS=OFF라 wss_probe 타깃 없음. 그때 pytest 실패는 미구축 환경 실패이며 버그 재현 증거가 아님. 같은 root를 TETRIS_BUILD_WSS=ON,TETRIS_BOOST_INCLUDE=/tmp/study136-boost/root/usr/include로 구성한 뒤 실제 전/후 검사 수행. root 빌드 설정 변화는 현재 로컬 CMakeCache에 유지. 초기 검사 Python else숫자 공백 SyntaxError를 수정하고 최종 검사를 실행했음.
- Part 발췌·Markdown27·704문제·Mermaid10도표 parse·인라인7/현재 심볼/누적 파일·정적 ZIP 재현성/자산 해시·로컬HTTP 일치·diff 통과. release1d75fe9461e9f495,16파일. 근거 out/learning-jobs/142-{final-verification,root-before-real,root-http,root-final-build,sdl-test,sanitized-build,sanitized-test,part,site}.log. 모든 외부모델/빌드/검사 세션 종료, 로컬18767서버 유지.
- 다음143: server/ranked_game.h·tests/ranked_game_test.cpp·Part18§1~2와 누적 net/input_authority.h/게임 규칙·대전 진행을 읽는다. 현재 RankedGame은 서버 seed의 두 SimGame, 양쪽 입력 공통 prefix만 Tick, 공격 총계 delta 교환, 첫 terminal 상태에서 통계 확정. winner는 편 번호이며 relay가 계정 ID로 변환. 학습 CP에는 server/ 디렉터리가 없고 net/에 누적 릴레이 코드가 있음.143은 같은 코어의 서버 재현·신고 불신에 집중하되 입력 검증 전제는 생략하지 않는다.144 입력 검증 세부,145 두 relay 소유권,146 상태 사유,147 봇 BP,148 남은 어뷰징 후 커밋·paused 예정.


- 2026-10-03: **141차시 원자적 자격 교체 제작/검수 완료(progress)**. 로컬1~141·699문제, 다음142 “복구 저널: 응답을 잃어도 다시 시도”. 단원134~148 완료→범위 감사/커밋→goal paused 예약 유지.149 시작 금지. 공개1~55 동결, 푸시 없음.
- CP141-account-change는140에서 account_keys 복구/세대/영수증 스키마v6·교체 helper·loopback 계정 API·계약 검사를 추가. 누적 account/migration/shop/progression 검사를 보존·v6 기대값 및 명시적 INSERT 열로 갱신. 141.json16절·5문제·인라인7·전체파일3 reviewed.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny로 선택 인터페이스/명세만 전송·exit0. out/learning-jobs/141-change-{material.txt,events.jsonl,draft.json}. primary가 해시 예외 경계·SELECT 수명·세대 상한·확장 오류 분리·영수증 우선 재사용 검사 보완. 키/DB/설정 전송 없음.
- **현재 서버 오류 재현/수정**: Database::changeAccount에서 UPDATE 뒤 read_player_by_token의 digest 예외를 주입하면 같은 연결에 old=0,new=1 상태와 열린 트랜잭션이 남았음. AccountTransaction RAII로 모든 return/예외 롤백과 출력 초기화; 수정 후 old=1,new=0 및 정상 재시도/epoch 통과. tests/account_change_exception_test.cpp는 실제 database.cpp와 digest fault wrapper 연결; root CMake 타깃 추가. SQLITE_DONE/읽기 오류·bind 결과 구별 보완.
- **현재 서버 정책 누락 재현/수정**: 다른 요청으로 현재 복구 코드를 backup/rotate 후보에 재사용하거나 recover에서 현재 접근 키를 재사용하면200이었음. 소유자 조회에서 현재 해시를 읽어 새 요청의 재사용400거절. 동일 요청의 최신 영수증을 먼저 검사하여 성공 재전송은200·세대유지. HTTP회귀3사례 수정전실패/수정후통과. 초기 pytest 실패 로그의 임시fixture 자격은 실제 사용자 비밀이 아님; 외부전송하지 않음.
- 실제 서버의 모든 SQLITE_CONSTRAINT를 후보충돌로 취급하던 분류도 UNIQUE/PRIMARYKEY만409, trigger/CHECK등503으로 분리. 트리거 오류 후 자격/세대 보존·트리거 제거후재시도 검사. Part17§4 재사용/영수증 순서·RAII예외·확장코드·커밋/응답 경계 보완; mutable IP/티켓 정책수치 설명 제거. coverage 부분대응/해시 갱신.
- SCRIPTED/SDL study_account_db·account_change_contract 빌드/실행 통과. 누적 account·migration·shop·progression 계약 통과. 독립 DB연결의 복구경쟁 한승자·ID/BP/XP/아이콘 보존·후보중복·예외/트리거롤백·최신영수증·과거영수증거절·세대상한·현재비밀재사용거절 통과. 실제v5 fixture를v6서비스로열어 계정/재화/소유/접근해시보존 확인.
- 학습 HTTP 엄격입력/no-store·병렬복구·재전송·트리거503·재시작 통과. 현재 meta Release빌드·HTTP8회귀통과(회전/입장권폐기·복구/재화·경쟁·충돌/예산·트리거오류·재사용3). 새 C++교체경로 ASan/UBSan통과; 기존SQLite정적archive는비계측. Windows/macOSnative·운영DB·실장치장애 미검증. 기존 vendor SQLite경고는로그보존.
- Part발췌·Markdown27·699문제·Mermaid10도표parse·인라인7/심볼/누적파일·정적ZIP재현성/자산해시·로컬HTTP일치·diff통과. 생성기 첫검사에서 전체파일 경로를docs/learn기준으로중복지정한오류를검출하여 checkpoints/...로수정후통과. release35d59af730f17847,16파일. 수동GUI/공개배포/커밋없음.
- 근거 out/learning-jobs/141-{final-verification,sdl-final,http-root-final,root-final-build,before-reuse,part-final,site}.log 및 exception전후로그. 모든 모델/빌드/검사 세션 종료(27391,13796,53925,97235실패수정후90851완료). 로컬18767서버유지.
- 다음142: meta/account_client.cpp·account_store.cpp·private_file.cpp와Part17§5를읽고, 현재origin검사/프로세스잠금/전송전pending/서버성공뒤부분파일완료/동일재전송/확정거절·불확실을누적실습에연결. meta/account_operations.cpp는존재하지않음(탐색실패,올바른파일account_client.cpp).141의서버영수증은그대로사용;클라이언트journal/네트워크결과를모형만으로끝내지않고실제HTTP및재시작실험필요.148완료후paused전환을잊지않는다.


- 2026-10-03: **140차시 토큰 이관 제작/검수 완료(progress)**. 로컬1~140·694문제. 다음141 “회전·폐기·복구: 하나의 원자적 교체”. 단원134~148 완료→범위 감사/커밋→goal paused 예약 유지;149 시작 금지. 공개1~55 동결, 푸시 없음.
- CP140-credential-migration는139의 CMake/README 외 누적 파일을 바이트 보존. 새 credential_migration.h·CLI·계약 검사. 누적 계정 DB는 처음부터 해시 저장이므로 그대로 두고, 실험용 옛 players(id,token,bp) 파일에서 실제 서버의 변환/정리 구조를 재구성. 140.json16절·5문제·인라인8·전체파일3 reviewed.
- OpenCode deepseek/deepseek-flash,tools deny로 선택한 인터페이스/명세만 전송·exit0. 이번 호출 cwd는저장소였으며 도구권한은전부deny; 추가파일읽기/키/DB/설정 전송 없음. out/learning-jobs/140-migration-{material.txt,events.jsonl,draft.json}. 이후 호출은 기존 /tmp 관례로 복귀. primary가 중복keyset분기·중첩/attached 거절·명시적정수PK검사를 보완.
- **현재 코드 개선**: meta/database.cpp migrateCredentials의 활성 SELECT를 step하면서 같은 테이블 UPDATE하던 가정을 제거. SQLite 공식 동일연결 격리 계약은 이때 후속조회 결과를 보장하지 않는다. ORDER BY만으로 해결된다는 교재 설명도 교정. 한 행 원문/ID 복사→SELECT finalize→UPDATE→id>last 재조회. ID덧셈·전체원문vector없음, bind/reset/clear/변경행수 검사 추가. 기존 실제 데이터 손실/공격을 재현했다고 주장하지 않음.
- 논리 변환은 BEGIN IMMEDIATE에서 열변경/전체행/hash marker 커밋. 커밋 후 checkpoint/VACUUM/checkpoint와마지막scrub marker. 각단계예외·_Exit프로세스종료·옛WALreader에 따른 상태를 검사. SQLite 설정 readback, storageclass/전체바이트·NUL/BLOB·keyset 최소/최대ID·부분갱신롤백·재해시방지 확인.
- SCRIPTED/SDL 신규도구·계약타깃 빌드/실행 통과. 누적게임소스는139동일이므로새GUI/전체게임재실행없음. ASan/UBSan은 새C++이관경로에 적용; 기존SQLite정적라이브러리는비계측. GNU15 vendorSQLite const폐기/strlen overread경고보존, 신규코드경고없음.
- 실제프로세스 crash-row/crash-commit/crash-vacuum 종료코드와재시작·반복시작통과. 현재DB/WAL의공개fixture바이트부재와외부백업잔존을함께검사. 예외RAII복구와프로세스저널복구를구별하고전원장애/SSD삭제의증거로확대하지않음.
- 실제meta Release빌드·HTTP/DB5회귀통과: 여러행/최대ID·인증/통계유지·재시작·SQL덤프복원, 잘못된문자/NUL뒤바이트/BLOB전체롤백, 옛reader로hashcommit후scrub실패→reader해제→같은원문으로인증/정리재개. root tests test_account_security.py 확장.
- Part17§3 keyset수명·논리/정리marker·옛reader스냅샷·프로세스중단/전원장애·사본경계보완. coverage해시/lesson140부분대응, 실제전원/장치/운영백업/RP이관전체를covered로표시하지않음.
- 인라인8·현재심볼·누적파일·Part발췌·Markdown27·694문항·Mermaid10도표parse·정적ZIP재현성/자산해시·로컬HTTP일치·diff통과. release72fe61fa59c05457,16파일. Windows/macOSnative·실제전원장애·운영DB이관·전장치삭제미검증. 수동GUI/공개배포/커밋없음.
- 근거 out/learning-jobs/140-{build,sdl-build,sdl-test,root-build,http,verification,part,site}.log. 초기원고생성기의 괄호SyntaxError는수정후생성/발췌검사통과. 모든외부모델/빌드/검사세션종료, 로컬18767서버유지.
- 다음141: 실제 rotateCredentials의입력검사·트랜잭션·계정세대(auth_epoch)·기존원문/복구코드·교체영수증과Part17§4를읽고누적account_keys및migrations구조에원자교체를연결한다. 현재실험용legacy파일과누적서비스DB를혼동하지않도록각진입조건설명유지. 응답유실/클라이언트저널은142의별도중심범위지만141의원자성설명에필요한계약은생략하지않는다.


- 2026-10-03: **139차시 CSPRNG·해시·조회 제작/검수 완료(progress)**. 로컬1~139·689문제, 다음140 “토큰 이관: 평문을 지우는 전체 과정”. 사용자 예약 중지 유지: 단원134~148 완료→범위 감사/단원 커밋→goal paused. 149 시작 금지. 공개1~55 동결, 자동 푸시 없음.
- CP139-credential-crypto는138의 CMake/README 외 누적 파일을 그대로 보존. credential_lab.h와 실패 주입·조회 계약 검사 추가. 139.json16절·5문제·인라인10·전체파일2, reviewed. 게임용 난수/인증용 CSPRNG·바이트/hex·추측/충돌·역상/충돌 저항·목적 분리·DB 조회·직접 비밀 비교를 구분한다.
- OpenCode deepseek/deepseek-flash,/tmp,tools deny로 선택 명세만 전달·exit0. out/learning-jobs/139-credential-{material.txt,events.jsonl,draft.json}. primary가 배열 반환을 wire 문자열 반환으로, void 삽입을 bool 충돌 결과로 교정; hex 길이 오버플로/null·빈 view·단회 EVP 계산 보완. 비밀/설정/DB 전송 없음.
- **현재 코드 개선**: meta/api_server.cpp의 volatile XOR 비교를 meta/credentials.cpp의 equal_secret(CRYPTO_memcmp)으로 이동·호출 교체. 기존 코드에서 공격 성공을 입증한 변경이 아니라 명시적 라이브러리 계약으로 근거를 강화한 변경. 길이와 HTTP 전체 시간을 숨긴다고 주장하지 않는다. CMake credentials_test·tests/credentials_test.cpp와 HTTP 같은 길이 잘못된 secret 거절 추가.
- Part10의 volatile 보장·일반 == 동작 단정·충돌 재시도 설명을 교정하고 현재 발췌 동기화. Part12 함수 경로·Part16 consume 호출·Part17 생성/표현/조회/읽기 유출 범위와 lifetime 정책 연결. 대응 해시 갱신; Part10§11 및Part17§2는부분 대응으로 유지. 전체 Part/보안 완료로 표시하지 않는다.
- SCRIPTED/SDL credential_crypto_contract CTest와 SDL 누적 tetris 빌드 통과. 두 구성의 account_contract를 각각 임시 DB로 실제 실행(CTest 미등록 타깃이라 checker에서 별도 실행하도록 교정): 원자 생성·해시 저장·충돌·재시작 통과. 학습/현재 helper ASan/UBSan·고정 벡터·NUL·도메인·모든 위치 비밀 비교 통과. 난수 실패 출력/음수/예상 밖 반환·null callback는 발급 없음.
- 현재 meta Release 빌드·credentials CTest·실제 HTTP 2검사(hash 원문 재사용 거절, 같은 길이 secret 앞/뒤 불일치 거절 후 ticket 보존·병렬 단일 소비) 통과. 최초 root multi-target에서 CMake 재생성 전 make가 새 credentials target을 몰라 meta 성공 후 실패; 재호출 build/CTest 통과. 초기실패 로그와 최종성공 로그 구분.
- GNU15가 기존 third_party/sqlite3.c에서 const qualifier 폐기·strlen overread 최적화 경고를 냄. 139-initial-sqlite-warnings.log 및 SDL-build.log에 보존. checker는 해당 외부 경고 두 종류만 기록하고 새 경고는 실패 처리. SQLite 전 경로 안전을 입증하거나 vendor 코드를 수정/경고 억제하지 않음. 새 helper 경고 없음.
- Part발췌·Markdown27·689문항·Mermaid9도표 실제 parse·누적 파일·심볼·인라인·정적 ZIP 재현성/자산 해시·로컬HTTP 일치·diff 통과. 로컬release33ab9b4404e2b980,16파일. 수동GUI/공개배포/커밋 없음. Windows/macOS native·시간 부채널 분석·CSPRNG 암호 분석·메모리 안전 삭제 미검증.
- 근거 out/learning-jobs/139-{final-verification,account-scripted,account-sdl,root-build,root-test,http,part,site}.log. 139-verification.log는 외부SQLite 경고를 무조건 거절한 초기검사 실패, 최종은final-verification+account-* 기록. 모든 모델/빌드/검사 세션 종료, 로컬18767서버 유지.
- 다음140: 실제 meta/database.cpp migrateCredentials와Part17§3를 읽고 논리 이관/물리 정리·marker·트랜잭션·재시작·NUL/SQLITE_TEXT·백업 경계를 누적 실습에 재구성한다. 안정적139 학습 코드와 현재 소스 레퍼런스를 구분한다.


- 2026-10-03: **외부 리뷰 대조·사이트 로딩/SDL 이식성 보완 완료(progress)**. 집필은138 완료 상태, 다음139. 사용자 요청은 현재 큰 단원134~148까지만 완료→단원 커밋→goal paused. 149 시작 금지, 재개 요청 대기. 이번 리뷰 작업도 단원 커밋 범위에 포함한다.
- 리뷰의 고정176주제/정점14필드 문구는 이미 수정돼 있었다. part<=18 반복은 실제 coverage Part 집합으로 변경. 미리보기는 현재 GPU 강의 링크가 있는 지도로, noscript는 실제1차시만 가능하다고 수정. 모바일 제목의 줄바꿈 앞 공백, fallback색/DeepSeek구현자 주석 정리. 보조 설명 ref.omitted 누락에서 undefined가 출력되는 문제도 조건부 표시로 수정.
- 전 절의 “함께 만들기” 라벨은 제거하고 번호·제목 사용. 제목만 보고 CS/구현 분류를 자동 생성하지 않았다. 선수 지식은 개념 중심을 유지하여 이전 차시 언급 반복을 되살리지 않았다. 결정론 순서·소리 콘텐츠는 기존 의도 유지.
- **assets.js** DeepSeek 선택 명세 초안→검수. 승인된 명세만 /tmp OpenCode deepseek-flash/tools deny로 전송, exit0. 원본/자료/events는 out/learning-jobs/review-assets-*. prototype 이름 차단, 동기 삽입 예외도 캐시 해제, 실패 재시도, hash URL 보완. library.js는 뷰어 최초 열기, Mermaid는 실제 다이어그램이 있는 강의/원문에서 로드한다.
- reader는 로딩 중 닫기/다른 자료 선택의 request 세대를 지키고 retry 제공. 작은 편성 목록은 lessons의 coursePlan으로 분리하여 roadmap이 library에 의존하지 않는다. 강의 Mermaid가 일반 fenced code로만 남던 누락을 발견하여 안전한 HTML wrapper와 지연 렌더 연결. 라이브러리 로딩 없이 강의 도표 사용, 실패 시 코드 펼침. 배포 config에 lazy 자산 해시 포함, ASSETS에 assets.js 추가, DEPLOYMENT 갱신.
- 초기 lesson-1 script 파일 원본18270207→9415917바이트, gzip추정5260417→2665724바이트. HTTP압축/태블릿parse측정으로 표현하지 않는다. 본문 lessons.js는 여전히 통합 파일이며 차시별 전송분할을 구현한 것은 아님. review-loading-size.json에 파일별 근거.
- **SDL 보완**: 초기~80 CMake의 Config 우선/SDL2 또는 SDL2-static→pkg-config 대안·StudySDL2 요구사항으로 통일. SDL 직접 main에는 실제SDL_MAIN_HANDLED 정의를 헤더보다 먼저, SDL_Init 앞에 SDL_SetMainReady. RAII 생성자도 준비 뒤 init. 누적 복사본·강의 inline 스니펫 모두 같은 교정; 안정적인 실습 변경으로 해당 원고 버전/검토 이유 갱신.
- GL 진단기의 누락된 forward-compatible 요청도 추가. “4차시부터 추상화했으므로 Windows 안전” 주장은 SDL 준비 계약을 생략하므로 채택하지 않았다. 2차시 빌드 설명/CS사이드노트, Part2§13.3 추가·대응해시 보완. root 게임런타임은 변경하지 않고 tests/learning/loader_real.cpp도 동일 보완.
- 검사: 누적SDL소스4968개 정책감사·내용이 다른116파일 Linux문법 컴파일·실제 Config/pkg fallback 빌드. 초기02/03/04/05/06/09의45경로 초기화/종료/실패와현재SDL포인터회귀통과. Windows/macOS native, Retina 장치 실행 미검증; 이를 플래그·문법으로 대체하지 않음.
- DOM 자동검사: 기존 모든 차시 단일mount/앵커/검색/저장/자료참조 통과. 신규 lazy-assets 초기미로딩·중복·프로토타입거절·닫기/자료변경·재시도·강의도표·hash·subpath·file URL 통과. jsdom26은 /tmp/study-review-dom(레포의 node 의존성 변경 없음). Mermaid 실제 parse로8강의도표 문법통과. 브라우저 조작/스크린샷 없음.
- Part발췌·Markdown27·684문항·정적자산해시/ZIP재현성·로컬HTTP일치·diff통과. 최종release b89aae8966981993,16파일. 공개1~55 동결 유지, 공개배포/커밋 아직없음.
- 로그 out/learning-jobs/review-{lazy-assets,navigation,diagrams,sdl-portability,sdl-runtime,site}.log. SDL수정목록review-sdl-changed.json은첫변환기록(후속 매크로 보완도있음). /tmp/repair-study-sdl.py는1회마이그레이션이므로재실행하지말고canonical파일사용. 자동검사 scripts/check_learning_sdl_portability.py로재확인가능.
- 모든 외부모델/검사세션 종료, 로컬18767서버 유지. 다음턴139를 계속하되 단원148 완료후 반드시 커밋 범위 감사와 paused 전환을 수행한다. goal 전체 완료로 표시하지 않는다.


- **사용자 중지 예약(2026-10-03): 현재 큰 단원134~148까지만 완료하고 단원 커밋 후 goal을 paused로 전환한다. 149 이후 집필은 재개 요청 전까지 시작하지 않는다. 리뷰 반영 작업은 이 단원 범위의 유지보수로 포함한다. 지금 즉시 중지가 아니다.**

- 2026-10-03: **138차시 비동기 TLS·취소·객체 수명 제작/검수 완료(progress)**. 로컬1~138·684문제. 다음139 “CSPRNG와 해시: 예측·노출·검색의 차이”. 공개1~55 동결, 단원134~148 완료 뒤 커밋. 전체 목표 active.
- CP138-async-tls는137의 CMake/README 외 누적 파일을 그대로 보존. async_wss_attempt.h, 실행 진단기, io_context 계약 검사 추가. 138.json17절·5문제·인라인11·전체파일3, reviewed. 실제 게임 런타임 소스는 이번 차시에서 변경하지 않았다.
- DeepSeek는 선택 명세만 전송한 async header 초안에 사용. OpenCode deepseek/deepseek-flash,/tmp,tools deny,exit0. out/learning-jobs/138-async-{material.txt,events.jsonl,draft.json}. primary가 없는 resolver.cancel(ec) 오버로드·IPv6 authority·호스트 NUL/포트 검증·deadline 범위·누적길이 뺄셈·post 순서 과장·flat_buffer 소유 설명을 수정했다.
- Attempt는 한 run 스레드, 외부 io/TLS context 수명, 콜백 shared self, host만 값 캡처한 인증 콜백. DNS/TCP/TLS/Upgrade/write/read·total deadline·첫 최종결과·late completion gate. finish는 socket/cancel만 하고 공유 io.stop을 호출하지 않는다. write 원본·read buffer는 객체 해제까지 보존, 결과와 메모리 해제를 별도로 관찰.
- 실제 현재 gateway+loopback echo로 성공·각 단계 취소·반복 취소, TLS ClientHello/Upgrade 요청/backend 수신 확인 뒤 stall 마감, wrong echo/SAN충돌/TCP연결거절을 검사. ASan/UBSan 실제비동기 행렬14사례 모두 reports1·late>=1·weak만료·destroy1·공유 루프 별도타이머 진행 확인. io.stop/restart/문맥파괴와 타이머취소 계약도 sanitizer 통과.
- 현재 WssClient는 외부 소유+전용 worker join 방식. current_wss_lifetime.cpp에서 pending read/write·반복 close 뒤 마지막 외부 소유자 해제를 확인. 초기 검사 exit6은 tcp_close가 shared_ptr를 없앤다는 잘못된 검사 가정이었다. 연결 상태와 복사본/소유자 해제를 분리하여 수정·통과; 런타임 버그로 보고하지 않는다.
- SCRIPTED/SDL의 async_context_contract CTest, SDL 누적 tetris·async probe 빌드 통과. GNU15 Release Boost1.83 basic_resolver_results.hpp:147 memcpy 최적화 경고가 발생; 억제하지 않았다. 정상 OS endpoint 가정과 관련 헤더 경계 확인·실제 경로 sanitizer 통과, 외부 헤더 전체의 안전을 입증했다고 주장하지 않는다.
- Part16§4.3 추가: 논리종료/완료/해제, this캡처와join, 공유루프/strong캡처/순환, buffer뷰, stop의범위, 정상WS/TLS종료와abort 구분. section05에138 부분대응·새해시 반영. docs 본문에는 작성 환경 보고 대신 구현 계약을 둔다.
- 새 강의의 질문 키 prompt를 기존 question으로 교정하여 생성 검증 통과. 인라인11/심볼/누적 파일·Part·Markdown27·684문항·정적ZIP재현성·로컬HTTP자산일치·diff 통과. 로컬 릴리즈82435ccaad732a6d(15파일). 공개 배포·커밋·수동 GUI 없음.
- 근거: out/learning-jobs/138-{build,sdl-build,native-build,sanitizer-build,final-runtime,site}.log. 초기138-verification.log는 잘못된 소유권 검사에서 실패한 기록이며 최종실행은138-final-runtime.log. checker에 --sanitize와통합138 분기를 추가했다. Windows/macOS·다중run스레드·OS별DNS취소전체·정상close_notify교환·전체게임은 미검증.
- 외부 모델/빌드/검사 세션 모두 종료, 로컬HTTP18767 유지. 다음139는 meta API의 CSPRNG 생성과 DB의 자격 해시/비교/검색, Part17 계정 자격 저장을 읽고 난수 예측 불가능성과 해시의 단방향·검색 목적을 분리한다. 138에는 I/O수명만 다루었으며 암호 저장 전 범위 완료로 간주하지 않는다.


- 2026-10-03: **137차시 일회용 입장권 제작·검수 완료(progress)**. 로컬1~137·679문제. 다음138 “비동기 TLS: 취소와 객체 수명”. 공개1~55 동결, 단원134~148 완료 뒤 커밋. 전체 목표 active.
- CP137-admission-tickets는136 누적 파일을 CMake/README 외 그대로 유지. AdmissionTickets/난수·wire 경계/계약 검사/상태 진단기 추가. 137.json17절·5문제·인라인10·전체파일3, reviewed.
- DeepSeek 선택 저장소 명세+현재 GameTickets만 전송. OpenCode deepseek/deepseek-flash,/tmp,tools deny,exit0 회수. 자료·events·draft는 out/learning-jobs/137-ticket-store-*. 계정 인증·난수·네트워크를 저장소 밖으로 유지하고 주석 교정. 키/설정/DB/전체 저장소 전송 없음.
- **실제 수정**: GameTickets의 충돌 실패 시 기존 유효 입장권 삭제를 137-issue-before.log로 재현. 만료 정리→키 충돌/신규 용량 검사→삽입 성공→옛 동일 player 제거로 수정. 재해시 뒤 새 순회, 잠금 안의 임시 추가 항목, 교체는 full에서도 허용. max_pending 정책 명명, 시간 덧셈 범위 검사. API expires_in은 lifetime에서 계산하고 503 오류를 ticket_unavailable로 정확히 표현.
- CP는 고정 배열 Capacity와 명시적 now/lifetime. 실패 시 유효 항목 보존·같은 키 충돌·슬롯 재사용·값 반환·단일 소비·만료 equality·overflow·epoch 후속 거절·removed-key 재발급 가능/호출자의 fresh CSPRNG 계약. 전체 게임 연결은 유지하고 별도 진단 타깃으로 실험한다.
- 검사: SCRIPTED/SDL의 계약·진단 CTest2씩, SDL 누적 tetris 빌드 경고 없음. 실제 meta 및 game_tickets_test Release 빌드/CTest. 학습·실제 저장소 ASan/UBSan 통과. 실제 HTTP 선택5검사(secret/목적/병렬소비/재발급/발급예산, 성공헤더 뒤 본문 폐기·재시도, 같은 DB로 meta 재시작, 키 교체 epoch) 통과. 단일 소비는 응답 전달·입장 완료를 보장하지 않음.
- README 명령·소유권/시간/실패 범위, Part16§1~3의 흐름·교체 실패 보존·재시작/다중 프로세스·응답 유실·TTL 안내를 수정하고 대응 해시 갱신. 현재 소스·학습 발췌10·누적 파일 보존, Part 검사, Markdown27사례,679문제, 정적ZIP 재현성·diff 통과.
- 사용자 가변 수치 지침을 재점검. 32/33은 셀 수×셀당 정점 수와 명시적 실습 조건,95는 큐 용량 정책,96은 실험의 입력 창,107은 워커/연결 계수의 수명 차이로 설명 보완. 실습 코드 숫자·프로토콜·검수 이력은 유지.
- 로컬 정적 릴리즈7dd8676530654250(15파일). 현재 http://127.0.0.1:18767/lessons.js 에 lesson-137 제공 확인. 공개 배포/커밋/수동 GUI 없음. Windows/macOS·전체 실제 경기·다중 메타 원자성·할당 실패 주입은 미검증.
- 실행 증거 out/learning-jobs/137-{verification,http,root-build,root-test,sdl-build,part,site}.log. 외부 모델/빌드/검사 작업 모두 종료. 다음 턴138은 현재 wss_client의 cancel·shared ownership·실행 문맥과 Part16§4를 먼저 읽고 작은 수명 계약으로 분리한다.


- 2026-10-03: **136차시 WSS 종단·터널 범위 제작/검수 완료(progress)**. 로컬1~136·674문제. 다음137 “일회용 입장권: 발급·소비·만료”. 공개1~55 동결, 단원134~148 완료 뒤 커밋. 전체 목표 active.
- CP136-wss-tunnel은135 누적 파일을 CMake/README 외 그대로 유지. MessageStream/계약검사/wss_roundtrip을 추가했다. 136.json16절·5문제·인라인10·전체파일3, reviewed.
- DeepSeek 선택 FrameParser/명세→message_stream 헤더 초안(136-message-stream-material/events/draft). OpenCode deepseek-flash,tools deny,/tmp,exit0. sink 예외가 prior state를 보존한다는 잘못된 주석을 교정하고 const Frame 참조로 전달한다.
- MessageStream은 메시지 상한/타입/null 선검사, B−F 구간 공급→need_more까지 drain, 메시지간 꼬리 보존, streaming-prefix 실패·sink false/예외·최종 EOF 계약. 그래픽 게임은 그대로, 동기 WSS 프로브는 별도 프로세스로 외부 timeout 아래 실행한다.
- **실제 보강**: net/wss_client.cpp의 Asio 기본 CN-only 허용을 로컬 TLS로 재현. verify_peer와 OpenSSL DNS/IP 검증 매개변수·NEVER_CHECK_SUBJECT/NO_PARTIAL_WILDCARDS로 SAN 필수화. DNS에만 SNI, 설정 실패 거절, URL NUL 거절/출력보존 검사. server/wss_gateway.cpp의 기존 misleading-indentation 경고도 정리.
- 검사: SCRIPTED CTest2·SDL tetris/프로브 빌드와 message_stream CTest1, 현재 gateway/native 빌드 경고 없음. 실제 split/bundle/fragment×native/Origin 왕복, 제어Ping/Pong분리, 경로/Origin은 backend접속0, text/unmasked/oversized는payload0. 네이티브 정상 DNS/IP·SAN충돌·CN-only·이름불일치·미신뢰CA·만료·local* 거절, ASan/UBSan과 모든 두 절단·최대프레임/긴메시지/EOF/sink실패 통과.
- 테스트CA에keyUsage=keyCertSign,cRLSign 추가(Python3.14 strict 검증); 경로실패 검사에서TLS인증서오류를 성공으로 잡지 않도록 분리. local*는 수정 전에도 거절하므로 CN-only와 달리 수정 전 허용을 주장하지 않는다. 공용fixture 변경 후 기존 HTTPS 실제 GET/POST 매트릭스도 통과.
- Part16§3.2/4.1/4.2/5 보완과 가변 수치→정책 표현,135 현재소스링크 갱신. library28docs/209sources,coverage344중covered7/partial163/unassigned174/needs-review0. 퀴즈·Markdown·Part발췌·인라인·정적ZIP재현성·로컬HTTP일치·diff검사 통과. release c9d45fc11fae0794.
- 로그:136-{tunnel-baseline,native-after,identity-final,verification,final-verification,sdl-config,sdl-build,site}.log. LinuxGNU15/Boost1.83/OpenSSL3.5; Windows/macOS/native GUI/전체실제경기/공개배포 미수행. Boost는시스템변경없이/tmp/study136-boost/root/usr/include에준비,다음환경에서존재재확인.
- 실행중 외부모델/빌드 없음. 로컬HTTP18767 서버 유지. 다음에는137 범위를 읽고 GameTickets의 발급/소비/만료·세대·동시성·응답유실 의미를 작은 기준 코드와 연결한다.

- 2026-10-03: 가변 수치를 구조의 고정 조건처럼 서술하지 않도록 학습 지도·안내와 기존 강의의 현재 코드 설명을 교정했다. 주제 수/정점 필드/보이스 풀/큐 용량/시간 정책/보상량은 데이터·상수·계산식으로 설명한다. 계산 예제는 가정을 명시하며 과거 검수 결과와 프로토콜 규약은 보존한다. OpenCode DeepSeek의 선택 JS 문구 초안을 검수 후 반영했다. 다음 집필은136, 큰 단원 완료 커밋 경계는 기존대로 유지한다.

- 2026-10-03: **135차시 HTTPS와 게임 연결 제작·실제 보안 수정·정확성 검수 완료(progress)**.
  로컬 **1~135차시·669문제**. 공개 Pages **1~55차시·269문제 동결**. 전체177차시 goal active.
  다음 **136차시 “WSS: TLS 종단과 터널 범위”**. 현재 큰 단원134~148을 마친 뒤 커밋한다.
- 135.json:16절·5문제·인라인7(C++6/CMake1)·명령1·접힌 전체파일4.
  HTTP 업무/연결 수명·독립 origin/게임 주소·정책/접속 성공 분리·개별 loopback 예외·
  TLS 핸드셰이크/레코드·CA/체인/서비스 이름·DNS/IP SAN·SNI·CN 대체 금지·peer 존재·
  콜백의 기본 검증 대체·C 객체 해제·CA 파일·timeout·리다이렉트/평문 대체·종단/빌드 설명.
- DeepSeek135: OpenCode deepseek/deepseek-flash,/tmp cwd,tools deny,Endpoint 정책 선택명세만.
  135-policy-{material.txt,events.jsonl,draft.json},exit0회수. DNS/IP 문법 검사로 오해하지 않게
  is_valid_host를has_safe_host_text로 고치고 설명 교정. 원문전체/키/설정 전송 없음.
- CP135-secure-connections는134의CMake/README 외 누적 파일 동일성을 확인했다.
  plan_connections는 파싱된 두 Endpoint의 역할/모드/host 바이트/port와 각각의 평문 예외를
  검사한다. 공개HTTPS+WSS,개발 평문은정확한127.0.0.1/::1. URI/DNS 파서나 TLS구현 아님.
  https_probe는 실제 API만 연결하며 게임WSS는계획으로 남음. SSLClient+체인/peer/SAN,
  /healthz200+고정본문,각단계1초·리다이렉트미추적·TLS실패시종료4,HTTP거절5.
- **실제 문제 수정:** vendored cpp-httplib0.18.5의 기본 verify_host는 SAN실패 뒤CN으로
  성공할 수 있었다. meta/tls_identity.h를 추가하고 실제 MetaClient GET/POST 모두에
  custom verifier 적용. callback이기본체인검사를대체하므로SSL_get_verify_result와peer존재를
  먼저확인한뒤X509_check_host/즉시IP바이트비교. NEVER_CHECK_SUBJECT·NO_PARTIAL_WILDCARDS.
  vendor 전체를 변경하지 않음. CN만 있는 사설인증서는 SAN 재발급이 필요한 호환성 변화.
- 9/30 실제 TLS 재현/검증: 수정전SAN불일치/CN일치와CN-only는실제HTTP1회전송/성공.
  수정후정상DNS/IP성공,틀린SAN/CN-only/다른IP/미신뢰CA/만료는HTTP0회. POST와GET독립검사.
  HTTPS대상평문서버와302→HTTP리다이렉트대상으로평문대체/추적하지않음.
  scripts/learning_tls_fixture.py는 로컬임시CA/인증서/서버, 외부DNS나실제자격증명없음.
  tests/learning/current_https.cpp는실제MetaClient의요청/응답경로를사용한다.
- SCRIPTED/SDL 각6CTest·누적SDL tetris 빌드,각HTTPS프로브9구성통과. 신규빌드경고없음.
  현재HTTPS미지원빌드도HTTPS를valid=false로거절. 정책·SAN/IP/wildcard/NUL/NULL ASan/UBSan.
  tests/tls_identity_test.cpp를CMake CTest tls_identity에추가. 실제tls_identity/ranked_game/
  json_input3통과,두relay·실제tetris빌드통과. 첫타깃추가직후미구성오류는cmake재구성후해결.
  MinGW Windows는정책계약만교차링크. Windows HTTPS/네이티브실행은미검증.
- Part16§1.1두연결·§4.1HTTPS체인/peer/SAN코드/이행추가. WSS의CA추가와HTTPS의명시CA파일
  선택시기본저장소분기생략을구분했다. Part12§9의모든공개TLS가tunnel이라는표현범위교정.
  RFC8446/9525·OpenSSL X509_check_host/SSL_get_verify_result·vendored계약대조.
  coverage344:{unassigned176,partial161,covered7},needs-review0. 문서28·소스207.
- 7스니펫·현재심볼·Part발췌/링크·Markdown27·669문제·전체DOM검사통과(9/30로그).
  최종실제게임빌드기록을course-plan에추가한후library를갱신하지않아정적검사stale발생.
  10/3그원인을대조하고library/lessons재생성후정적재현성통과:release **26acec65ce8bf4ba**,15파일.
  환경재시작으로옛세션99821/9736핸들은없었고종료DOM로그를확인. /tmp의jsdom도사라졌으므로
  화면/DOM전체를새로실행했다고주장하지않는다. 변경된검수메타외본문/동작의기존DOM근거유지.
  HTTP18767도종료되어직접접속거절을확인한뒤서버복구(session64149). rootURL과live-source유지:
  python3 -u -c 'import scripts.serve_learning as server; server.DOCS = server.DOCS / "learn"; server.main()' --port 18767
  표시로그의/learn/문구와달리실제학습주소는http://127.0.0.1:18767/이다.
  최종6자산및meta/tls_identity.h실시간소스해시확인. 수동GUI/스크린샷·커밋/푸시/공개배포없음.
- 근거:out/learning-jobs/135-{root-before-build,root-before,first,verification,root-configure,
  root-build,root-test,game-build,snippets,part,coverage,library,lessons,quiz,markdown,dom,site,http}.log.
  실행중외부/빌드/검수작업없음. HTTP18767만유지. 10/3최신사용자요청:매큰단원완료시커밋,
  DeepSeek계속사용. AUTHORING작업계약에기록했고현재단원의다음커밋경계148을설명했다.
- 다음136: Part16§3.2/4/5와net/stream_transport·wss_client·server/wss_gateway를읽고
  TLS종단→WebSocket framing→기존게임바이트전달 경계를누적실습에연결한다. TCP/TLS/WS메시지
  단위와분할·binary/text·경로/Origin·내부TCP보호범위를코드로설명하고실제프로브검수.
  137입장권/138비동기TLS수명과중복은피하되필수선행계약은생략하지않는다.
  /tmp작업파일·MinGW·jsdom런타임은환경재시작으로없을수있으니존재를먼저확인한다.
  135.json/CP135가정본. 라이브러리는coursePlan도내장하므로plan변경뒤반드시다시생성한다.

## 직전 완료 기록 — 134차시

- 2026-09-30: **134차시 위협 모델 제작·문서 교정·정확성 검수 완료(progress)**.
  로컬 **1~134차시·664문제**(1차시 정적+133개 JSON). 공개 Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **135차시 “HTTPS와 게임 전송: 서로 다른 연결”**.
- 134.json:16절·5문제·인라인12(C++11/CMake1)·명령1·접힌 전체파일4.
  자산/공격능력/DFD·인증/권한/형식/규칙 구분·고정 주체·타입/경기 검사·배치 후보창
  원자성·중복/충돌/오래된 입력·양쪽 소비·스트림 실패 범위·거절 뒤 정상 대조·공모/담합
  한계·TLS/FNV 범위를 구현과 연결. 저자 검증 보고는 본문에 넣지 않았다.
- DeepSeek134: /tmp cwd·tools deny·deepseek/deepseek-flash, 선택 InputAuthority 명세만.
  out/learning-jobs/134-authority-{material.txt,events.jsonl,draft.json},exit0회수.
  std:: 정수 타입·멤버 명명·저장/거절 설명 정리. 비밀/설정/DB/전체저장소 전송 없음.
- CP134-threat-model은133 누적 파일을 CMake/README 외 보존한다.
  InputAuthority는 고정 경기/참가자·한 번 시작/영구 종료·서버가 준 actor를 검사하고
  round codec과 TickInputs에 위임. take는 출력 비별칭·실패 시 무변경·양쪽 소비.
  BoundInputStream은 연결별 parser/const actor와 경기 authority 참조,16바이트 읽기·배수,
  손상 경계 sticky 실패와 의미상 거절 후 정상 요청을 구분한다. 단일 루프 수명 계약.
  신규 게이트는 post-admission 독립 실습이며 기존 샤딩 전달기/창/운영 프로토콜 대체 아님.
- 계약:255허용외타입·설정/상태/주체/형식/경기/창·배치 충돌의 원본 보존·duplicate/stale·
  출력 별칭·거절+정상 프레임 모든 분할·파서 손상 후 실패 유지.
  실제 TCP poll/epoll:actor11/22/99/0 네 연결, 거절9·저장2·동일재시도1·양쪽4틱.
  인증은 고정 fixture이며 TLS/규칙재실행/보상/시간당진행량은 이 프로브 범위 밖.
- SCRIPTED/SDL 각26CTest·SDL누적 tetris 빌드. ASan/UBSan 계약+실제 poll/epoll 통과.
  MinGW Windows 계약/프로브 교차 링크; Windows 네이티브 실행 없음.
  기존 state_machine_probe enum/정수 조건식 경고가 각Linux빌드1개, 신규/교차 경고0.
- 현재 런타임 방어의 새 기능 수정은 없음. net/framing.h의 요약 교차검증/공격자 이익 없음
  주석을 서버 결과 확정 계기·파싱/자원 비용 구분으로 교정했다.
  python/tests/test_relay_adversarial.py는 SERVER_REJECT까지4타입 차단 검사와
  양쪽 공모 신고를 보완. thread/reactor 각각3passed28deselected.
  거절 뒤 정상 INPUT, 미완료 status3/RP무변경/meta미호출을 검사했다.
  현재 두 relay와 ranked_game_test 빌드, CTest ranked_game 1통과.
  최초 잘못된 CTest 정규식 ranked_game_test는0검사였고 정정 후 실제1통과를 확인했다.
- Part12§1.3 신뢰 경계/불변식 표, Part18§1.1 서버 재실행/주체/담합 한계 보완.
  Part12머리말·회고·마치며의 판정미구현/복구없음/공개평문 설명을 현재 Part16~18로 교정.
  lockstep의 완벽성/즉시해시탐지/고정RTT 단정도 결정성·관측시점·입력버퍼 정책으로 수정.
  OWASP Threat Modeling/Authorization 공식 가이드 확인. 운영 전체 안전성 판정 아님.
  coverage344:{unassigned179,partial158,covered7},needs-review0. 문서28·소스203.
- Part·인라인12·현재심볼·Markdown27·664문제·전체DOM/기록/앵커/소스/fileURL 통과.
  정적15파일/상대경로/해시/재현 ZIP 검증, release **ea30859c01ca6c31**.
  HTTP18767의6자산 최신바이트 일치. 수동GUI/스크린샷·커밋/푸시/공개배포 없음.
  외부/빌드/검사 세션 모두 종료·회수. HTTP18767 유지.
- 근거: out/learning-jobs/134-{first,verification,root-build,root-test,thread-relay,
  reactor-relay,snippets,part,coverage,library,lessons,quiz,markdown,dom,site,http}.log.
- 다음135: HTTPS 메타 API와 지속 게임 연결의 수명/핸드셰이크/신뢰경계를 설명한다.
  Part12/16/17·현재 Caddy/WSS gateway·HTTP 클라이언트의 실제 경로/검증을 확인하고
  인증서/호스트 이름/연결별 실패를 작은 누적 실습으로 연결한다.136WSS와137입장권의
  상세와 중복은 피하되 필수 전제는 충분히 설명한다. 단순 선택 작업만 DeepSeek에 위임.
  원본134.json/CP134가 정본. /tmp/author134.py는 후보창 설명/심볼/검증메타 보정 전,
  /tmp/finish134.py는 중복 실행 시 dispatcher 중복 위험이 있으므로 재실행하지 않는다.

## 직전 완료 기록 — 133차시

- 2026-09-30: **133차시 샤딩 제작·코드/원문 수정·정확성 검수 완료(progress)**.
  직전132는 완료한 progress였다. 로컬 **1~133차시·659문제**(1차시 정적 +132개 JSON).
  공개 GitHub Pages **1~55차시·269문제 동결**. 전체177차시 goal active.
  다음은 **134차시 “위협 모델: 클라이언트가 거짓말할 때”**.
- 133.json:18절·5문제·인라인16(C++15/CMake1)·명령1·접힌 전체파일4.
  경기 분할/공유 인덱스·객체 그래프·unique_ptr 이동/메모리 공개·루프별 등록 ID·
  부분 등록과 해제 실패·제한 우편함·wake/수락 구분·등록 공백의 바이트·절대 만기·
  생산자와 close 경합·서버 정지와 결과 판정·라운드로빈 한계·프로세스 격리를 설명했다.
- DeepSeek133: OpenCode deepseek/deepseek-flash, /tmp·tools deny·우편함 선택 명세만.
  133-inbox-{material.txt,events.jsonl,draft.json}. exit0 회수.
  utility 직접 include, 소비자가 같은 mutex를 획득한 뒤 소유해 읽는 공개 설명,
  불필요 mutable 및 displaced 표현을 교정했다. 추가 도구 실행/저장소 탐색 없음.
  키/설정/DB/저장소 전체 전송 및 Codex 하위 에이전트 없음.
- CP133-sharding은132 누적 파일을 보존(CMake/README 외 동일성 검사).
  RelayMatch는 두 소켓·목적지 큐·절대 만기를 보관하고 주소를 유지한 채 포인터만 옮긴다.
  MatchLoop는 루프별 등록 ID를 따로 갖고 dispatch 경계에서 attach/detach한다.
  attach는 두 watch 성공 뒤 소유권 확정, 실패 때 첫 등록 철회/호출자 포인터 보존.
  unwatch 실패는 backend를 먼저 파괴한 뒤 루프 전체 경기를 정리하며 재사용 불가.
  인계로 만기를 재설정하지 않음. 현재 서버의 최초 Forward 시작 유예와 차이를 설명.
  TransferInbox는 고정 슬롯 unique_ptr 큐, 성공 때만 소비하고 같은 mutex로 close/drain.
  Shard는 주기 회수+공개 뒤 wake, 종료 때 마지막 배치 폐기/활성 경기 정리/join.
  실제4경기/2작업스레드 프로브:보류1024+등록 공백 중64바이트/경기와 역방향1바이트,
  앞단의 오래된 배치 무시·각 샤드2수락·종료 예약0. 입장/프레임 판정은 별도 누적 프로브.
- 실제 RelayLoop::hand_off는 최대256대기, 닫힘/가득 참/칸 할당 실패 때 호출자 소유 보존.
  begin_forwarding은 추출 전 세 소유 관계 확인, remove 실패 중단, 앞단 타이머 취소,
  세 노드 추출 뒤 수락. 거절 시 원래 맵에 노드 복원 후 경기 중단. 성공 후 포인터 사용 없음.
  abort_unstarted_match는 인프라 실패의 결과 저장을 차단한 뒤 두 연결 정리.
  drain_inbox의 부분 add 실패에도 이를 적용. 기존 read_paused/want_write 재등록 유지.
  shutdown은 mutex 안에서 수락 차단+최종 인계 회수, 서버 정지는 신규 경기 판정을 만들지
  않고 송신/연결/경기 카운터와 소켓/슬롯 반환. 이미 제출된 결과 저장은 join/후속 적용.
  poll 치명 오류는 공통 g_running을 내려 다른 루프도 종료하도록 수정.
  머리말의 전역 인덱스 분할 불가/비용 단정과 pause credit의 옛 모순 주석도 교정.
- 실제 RelayLoop 전체를 계측/Reactor만 통제:state 정상 대조와 full/closed/
  pending-shutdown/live-shutdown/add-first/add-second/remove-first/remove-second/poll-error.
  수정 전9경계 실패·정상 대조 통과, 수정 후10통과. ASan/UBSan은 포함한 현재 클래스에
  적용하고 의존 cpp 객체는 build-polish 기존 빌드를 재사용(모든 의존 코드 계측 주장 없음).
  첫 검사 빌드의 로그 설정 이름/의존 경로 누락을 고친 뒤 다시 실행했다.
- Python 정규 회귀 test_reactor_shards_preserve_pair_streams_and_release_counts 추가.
  Linux 실제 --loops3 서버에서4경기 READY+INPUT 묶음·양방향2프레임·두 샤드 각2경기·
  연결/경기/tx 반환 및 정상 종료 확인. 기존 loops2정책/룸검사와3passed39deselected(3.19s).
  기존 백프레셔 실제 메서드12경로·Room8경로·loop_primitives 통과.
- SCRIPTED/SDL 각23CTest·누적 SDL tetris 빌드. 우편함4생산자2000항목·close경합·
  등록/해제실패·만기보존 계약과 실제 poll/epoll 프로브 ASan/UBSan·TSan 통과.
  초기 실습 누락 send_socket 헤더와 시간 계산 함수 이름 수정 후 검증.
  MinGW GCC13 계약/프로브 Windows PE 교차 링크(신규 코드 경고0), Windows 실행 없음.
  누적 state_machine_probe의 기존 enum/정수 조건식 경고1개는 각 Linux 빌드에 유지.
  최종 서버 빌드 통과(주석 변경 뒤133-root-comment-build.log도 회수).
- Part14§10.1~10.5 전체 재구성/코드 발췌 동기화. §6.2 최종 회수 발췌를 그 책임으로
  좁히고 전체 종료는10절로 연결. §11.4 표의 남은 동기화/지원 플랫폼 표현도 교정.
  원문의 장르별 구조 단정을 프로세스 격리/공유 서비스/라우팅 비용 비교로 보존·대체.
  LT/ET 원문은 man7 epoll/epoll_ctl 공식 문서와 대조. 성능 향상 자체는 측정하지 않음.
  §10 내용 대응 covered, §11은128/133관련설명만partial, §6의covered 유지.
  coverage344:{unassigned183,partial154,covered7},needs-review0. 문서28·소스203.
- 16스니펫·현재소스심볼·Part·Markdown27·전체DOM 탐색/답안/소스/fileURL 통과.
  정적 경로/해시/재현ZIP 통과, release **2d26410fdf722a02**,15파일.
  최종 HTTP6자산 바이트 일치(133-http.log). 수동GUI/스크린샷 없음.
  외부/빌드/검사 세션 모두 종료·회수, HTTP18767 유지. 커밋·푸시·공개배포 없음.
- 주요 근거: out/learning-jobs/133-{first,first-fix,verification,root-before,root-after,
  root-build,root-final-build,root-comment-build,root-test,live-relay,backpressure-regression,
  room-regression,snippets,part,markdown,coverage,library,lessons,dom,site,http}.log.
- 다음134: Part12/16/17/18과 현재 입력/계정/결과/봇 경로로 신뢰 경계를 정리한다.
  클라이언트가 보낸 값과 서버가 검증/소유하는 사실을 구분하고 공격자 능력·보호 자산·
  진입점·불변 조건·검증 위치를 구체적인 작은 코드 실습으로 연결한다. TLS/토큰/권위판정의
  상세는 이후 편성안과 맞추되 필수 전제를 생략하지 않는다. 구현 명세 중 단순 부분만
  DeepSeek에 맡기고 실제 코드/Part와 대조한다. 공개55동결·기록 이동 삭제 유지.
  133.json·CP133이 정본이다. /tmp/author133.py는 프로세스 비교 메모/검증 메타 전 초안,
  /tmp/part133.py는 확장 대안 보충 전이므로 재실행 금지. /tmp/finish133.py 중복 실행 금지.

## 직전 완료 기록 — 118차시

- 2026-09-30: **118차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전117은 실제 구현·회귀·교재/HTML·정적검증을 마친 progress였다.
  로컬 **1~118차시·584문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **119차시 “HTTP 실패: 상태 코드·시간 제한·재시도”**.
- 118.json:13절·5문제·검사 인라인14(C++13/CMake1)·실행블록2·접힌 전체파일4.
  문법/스키마/도메인/권한→구조파싱→원시바이트/크기→콜백/깊이→중복키/객체별범위→
  필드집합→정수폭→문자열/NUL/UTF-8→HTTP검증시점→허용/거절대조군으로구성.
  집필검수보고는본문에넣지않았다.문제는즉시풀이·복습은짧은상태연결을유지한다.
- DeepSeek118-fields: 작은공개명세만 /tmp/tools deny로전송,exit0.
  number/text helper는정확타입·UINT64보존·음수거절·전체문자열길이·optional구별을검수했다.
  초안상단의과도한범위나열주석을간결화했으며구현은명세에맞았다.
  out/learning-jobs/118-fields-{material.txt,events.jsonl,draft.json}보존.
  키/설정/DB/전체저장소전송없음.하위Codex에이전트없음.
- 118-json-boundary는117의스키마v5/계정/상점/정산을유지.
  json_document.h와json_fields.h로wire공통파싱/필드추출분리,shop은text재사용.
  account_service의본문한도도kJsonBodyBytes=1024공유.
  원시NUL사전거절,flat/중복키/필드수정책유지.변경대상외상속파일바이트일치확인.
- 실제meta/json_input.h의포함된nlohmann3.11.3파서가원시NUL을EOF로처리하여
  {}+NUL과{}+NUL+ignored를허용함을재현했다.기존서버도guest를400으로거절하지않았다.
  118-parser-before.log/118-root-before.log에수정전실패보존.
  전체std::string의NUL사전검사를추가하여잘못된본문이계정을생성하지않게수정.
  유효한문자열이스케이프u0000은유지하고도메인에서별도검사한다.외부라이브러리파일은수정없음.
- tests/learning/current_json_boundary.cpp:원시NUL모든삽입위치·제어문자·잘못된UTF-8/
  surrogate·일반/escape중복·유효한글/escape·64KiB±1·16/17깊이·독립객체키범위·
  signed64양끝/초과·bool·중첩부모·문자열NUL보존·경기응답뒤NUL검사.
  학습json_boundary_contract는1024±1·flat·필드수/필수키·UINT64MAX/초과·
  int32/uint32좁히기전범위·빈문자열/누락/다른타입·NUL ID·이스케이프키허용을검사.
- SCRIPTED8타깃,SDL2타깃빌드및각JSON CTest통과.각플랫폼HTTP검사에서
  잘못된guest뒤players0,잘못된buy뒤BP300/default유지,정상escape키구매뒤BP200확인.
  SCRIPTED계정/구매/정산/진행C++계약·HTTP본인/가격/경합/재시작·v1/v2멱등회귀통과.
  두파서ASan/UBSan통과.기존GUI/game전체재빌드는반복하지않음.
  118-verification.log;check_learning_json_boundary.py와통합dispatcher로재현가능.
- 실제메타재빌드및API58pytest통과(native/initial_guest관련9선택제외).
  python/tests/test_account_security.py에원시NUL2회귀추가.
  118-root-build.log/118-root-pytest.log.함수별본문재파싱은비용/설계논점으로설명,
  새보안결함으로단정하거나대규모인터페이스변경하지않았다.
- Part10§9:현재파서발췌동기화·원시/decodedNUL·객체별키·필드와도메인·반복파싱설명.
  직렬화에서escape만주의하면된다는단정을유효UTF-8전제와함께교정.
  §13의find_sub/parse_side수동파서설명이현재구현인듯남은문단을현재구조파서로교정.
  두관련절의부분근거만갱신.공식nlohmann콜백/NUL설명·RFC8259객체/문자열규칙대조.
  coverage344=covered3/partial143/unassigned198/needs-review0.문서28·소스197.
- Part·인라인14소스대조·Markdown27·584문제·전체DOM/기록/앵커/소스/fileURL통과.
  정적release **a9e2ebf92661c66e**,15파일·상대경로·해시·재현ZIP검사통과.
  out/learning-site/releases/a9e2ebf92661c66e 및동명ZIP.
  HTTP18767바이트일치:index24870/lessons8005681/library5531205/app14480/usability7039.
  118-part/snippets/markdown/dom/site.log보존,git diff --check통과.
- 모든모델/빌드/검사작업종료·회수,HTTP18767유지.
  수동GUI/커밋/푸시/배포없음.다른OS네이티브·운영부하·전체어뷰징감사미완료.
- 다음119:118의meta/http_sender.h와net/match_submission.h는한번전송과시도횟수소유를
  분리하지만HTTP모든비200을unconfirmed로합친다.새실패분류/재시도계약을작게설계할것.
  실제meta/http_client.cpp::post_match는동일UUID/body로network/429/5xx최대3시도,
  100/200msbackoff·공유steady마감과단계별timeout을사용한다.
  공유마감은다음시도허용의예산이며엄밀한전체취소마감이아님.
  tests/learning/match_http.cpp와match_response.cpp의기존회귀를읽고범위를이어갈것.
  실제실패로그에response.body를출력하는부분은추가검토후판단(새결함확정아님).

## 직전 완료 기록 — 117차시

- 2026-09-30: **117차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전116 goal turn은 실제 구현·회귀·교재/HTML·정적검증을 마친 progress였다.
  로컬 **1~117차시·579문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **118차시 “JSON 경계: 문법·타입·중복 키”**.
- 117.json:14절·5문제·검사 인라인11(C++10/CMake1)·실행블록2·접힌 전체파일4.
  카탈로그/소유/선택→서버가격/요청→조회스냅샷→인증/쓰기거래→중복차감/TOCTOU→
  선택/오류구분→응답유실/실패실습으로재구성.작업검수보고는본문과분리했다.
- DeepSeek117-catalog: 작은공개카탈로그/선택파서명세만 /tmp/tools deny로전송,exit0.
  초안의없는wire::object·nlohmann iterator.second·문자열타입검사누락을수정했다.
  정확std::string비교/NUL거절·추가필드·숫자/bool/null/중복키를검사.
  material/events/draft는out/learning-jobs/117-catalog-*보존.키/설정/전체저장소전송없음.
- 117-icon-ownership은116의스키마v5/키/정산을유지. CMake/README/DESIGN/
  sqlite_results/account_service만변경,shop_catalog/shop_view/contract/probe추가.
  가격default0/ruby100/gold250는서버배열.서비스시작seed_shop은참조ID만보충한다.
- inventory는한SQL문장으로프로필/선택/소유목록스냅샷을복원,누락타입/선택미소유거절.
  구매/선택은BEGIN IMMEDIATE뒤인증·소유/잔액판단,엄격한차감1행/삽입1행·응답조회·커밋.
  구매와선택은분리,이미소유409·반복선택200/무차감.추가price/player_id/owned거절.
  HTTP카탈로그/본인목록/구매/선택을기존계정서비스에연결.비밀출력없는shop_probe추가.
- SCRIPTED/SDL각13관련타깃·2CTest·이관/보상/멱등/진행/계정/상점계약통과.
  가격99/100·250경계,ABORT/IGNORE삽입전체롤백,독립연결동시구매한번차감,
  타계정선택거절·동일선택무차감·사전키폐기·재시작보존 검사.
  HTTP서버카탈로그가격/추가필드거절/200+409동시구매/403선택/현재목록·재시작통과.
  기존커밋전후kill·계정hash-only/정산v1/v2/동시재전송회귀통과.
  새C++/실제구매회귀ASan/UBSan통과.117-verification.log.인라인11최종소스대조.
- 실제구매에서독립연결둘이BEGIN전에미소유를읽는경합을barrier로재현했다.
  BP300→100/성공2회였고,소유조회IOERR도미소유로처리돼차감됐다(117-root-before.log).
  수정후한번차감(BP200)/Ok+AlreadyOwned,조회오류DbError/잔액보존을확인했다.
- 실제ShopTransaction RAII를구매/선택에공유,인증부터응답조회까지쓰기TX안으로이동.
  player_owns_icon은optional<bool>로소유/미소유/조회오류구분.소유권INSERT는OR IGNORE를
  없애고changes==1확인(기본등록도같은보조함수사용).조건부차감과선택도행수검사.
  응답프로필은COMMIT전읽고성공뒤에만out_player설정.조기반환/예외는롤백.
  RAISE(IGNORE)삽입시차감롤백·선택조회IOERR·정상/반복선택도실제회귀로검사했다.
  키폐기와구매의동시경합자체를별도재현한것은아님.트랜잭션순서계약을소스대조했다.
- 실제메타재빌드,기존메타/계정API56pytest통과(native/initial_guest관련9선택제외).
  117-root-build/pytest.log. tests/learning/current_shop.cpp로핵심실제회귀재현가능.
- Part10§8:실제구매/선택발췌·거래순서/조회3상태·행반영검사로갱신.
  조건부UPDATE가과거읽기값동일성을보장한다는오류,객체mutex가독립연결까지보호한다는
  오해를교정.이미지파일변경/서버소유권·판매중단/기존소유정책을구분했다.
  SQLite transaction/conflict공식문서대조/강의링크.원문해당절부분근거만갱신.
  coverage344=covered3/partial142/unassigned199/needs-review0.문서28·소스197.
- Part/스니펫11/Markdown27·579문제·전체DOM/기록/앵커/소스/fileURL검사통과.
  정적release **c4588c38c4d910df**,15파일·상대경로·해시·재현ZIP통과.
  out/learning-site/releases/c4588c38c4d910df 및같은ID ZIP.
  HTTP18767일치:index24870/lessons7957107/library5527599/app14480/usability7039.
- 모델/빌드/검사작업모두종료·회수,HTTP18767유지.하위에이전트/수동GUI/커밋/푸시/배포없음.
  타OS네이티브·운영부하·실전환불/반복소비상품미검증.전체어뷰징감사완료로표시하지않음.
- 다음118:실제meta/json_input.h/json_routes.h/protocol.h와학습wire.h를대조해JSON문법/
  타입/중복키/UTF-8/NUL/숫자폭/깊이·크기/요청검증시점을실습할것.현재root는64KiB/깊이16,
  객체별키집합이며json_post는본문수신뒤검사한다.필드helper가본문을재파싱하는구조도검토.
  위내용은읽은구현의연결점이며새결함으로확정한것은아니다.키/DB/설정전송금지유지.

## 직전 완료 기록 — 116차시

- 2026-09-30: **116차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전115 goal turn은 실제 구현·회귀·교재/HTML·정적검증을 마친 progress였다.
  로컬 **1~116차시·574문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **117차시 “아이콘 소유권: 서버에서 검사할 것”**.
- 116.json:14절·5문제·검사 인라인13(C++12/CMake1)·실행블록2·접힌 전체파일4.
  식별/인증/인가→ID/원문/해시→CSPRNG/엔트로피→해시/도메인분리→스키마/등록→
  본인조회/정수폭→클라이언트보관/분실·폐기→실패실습 순서. 작업검수보고는본문과분리.
- DeepSeek116-profile: 자격증명없는프로필 DTO의 작은명세만 /tmp/tools deny로전송,exit0.
  반환내용의 비민감정보 단정주석과 전역uint64_t를 교정. 64비트ID보존/범위검사확인.
  material/events/draft는out/learning-jobs/116-profile-*보존.토큰·키·DB·전체저장소전송없음.
- 116-guest-account는115에서CMake/README/DESIGN/sqlite_results/migrations와버전검사수정.
  v5 account_keys는공개ID→고유해시만저장.기존fixture는키를소급부여하지않음.
  OpenSSL Crypto필수.24난수바이트를공개ID후보8/비밀16로분리,원문32hex·해시64hex.
  실패시fallback없음. ID/해시충돌만최대4후보,저장/난수실패503.
- register_account는player/기본소유권/wallet/career/hash모두같은트랜잭션으로생성.
  공통insert_player는mutex/쓰기TX소유전제.프로필조회는LEFT JOIN으로행손상과키부재구분.
  study_account_db별도loopback서비스: POST guest{}→201ID/token,GETmeBearer→본인4필드.
  추가쿼리·중복Authorization·ID/DB해시인증거절.모든응답no-store,비밀로그없음.
  기존결과fixtureHTTP는그대로이며공개인증서비스로취급하지않음.
- account_probe.py는발급원문을메모리에만두고프로필만출력.반복실행은새계정생성.
  등록응답자체유실은멱등보장없음,파일저장실패와구분.현학습단계의폐기/복구/공개TLS/
  등록제한은생략명시,실제AccountStore의origin검사/비공개저장과레퍼런스로연결.
- SCRIPTED/SDL각12관련타깃·2CTest·이관/보상/멱등/진행/계정계약 통과.
  난수실패·각구성INSERT실패롤백·fixtureIDclaim거절·ID/해시충돌·해시만DB저장검사.
  HTTP2계정본인조회·잘못된원문/ID/해시·중복헤더/쿼리·no-store·SQL실패503·재시작검사.
  기존커밋전후kill·HTTPv1/v2·동시재전송·UINT64회귀통과.
  계정C++/실제등록ASan/UBSan통과.116-verification.log.인라인13최종소스와대조.
- 실제registerGuest의기본아이콘INSERT실패시계정만남고성공반환하는문제를트리거로재현
  (116-root-before.log).BEGIN IMMEDIATE/RegistrationTransaction RAII로부분성공거절,
  마지막COMMIT성공뒤반환.오류/중복키반환은롤백.함수입력/API응답형식유지.
  tests/learning/current_guest.cpp로실패시계정0행·다음정상등록·중복키1행보존확인.
- 실제메타를out/learning-checkpoints/108-meta-boundary-check/meta에서재빌드.
  test_meta_db_smoke+test_account_security의API회귀56통과, native/initial_guest관련9선택제외.
  116-root-build/pytest.log보존.클라이언트파일코드는변경하지않음.
- Part10§8.1실제발췌·원자등록설명으로교정,아이콘fallback을저장부분성공의근거로쓰던문장삭제.
  Part17§1~2선별대조,인증/보관/수명경계부분연결.원문전체감사로표시하지않음.
  coverage344=covered3/partial142/unassigned199/needs-review0.문서28·소스197.
  OWASP Session Management,OpenSSL RAND_bytes공식문서대조/강의관련메모에링크.
- Part/스니펫13/Markdown27·574문제·전체DOM/기록/앵커/소스/fileURL검사통과.
  정적release **3d79589cb7ffcd12**,15파일·상대경로·해시·재현ZIP통과.
  out/learning-site/releases/3d79589cb7ffcd12 및같은ID ZIP.
  HTTP18767일치:index24870/lessons7885617/library5523485/app14480/usability7039.
- 모델/빌드/검사작업모두종료·회수,HTTP18767유지.하위에이전트/수동GUI/커밋/푸시/배포없음.
  타OS네이티브·공개운영부하·물리전원장애미검증.자격증명수명전체/전체보안완료아님.
- 다음117:인증된계정의아이콘소유권/선택/구매를학습API로연결.실제purchaseIcon이BEGIN전
  소유권을읽고insert_icon_ownership은INSERT OR IGNORE의DONE만보는경합가능성을검토할것.
  이는코드읽기후보이며독립연결동시구매/키폐기경합을아직재현하지않음.재현후수정범위결정.

## 직전 완료 기록 — 115차시

- 2026-09-30: **115차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전114 goal turn은 구현·회귀·교재/HTML·정적검증을 마친 progress였다.
  로컬 **1~115차시·569문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **116차시 “익명 계정: ID와 자격 증명을 구분”**.
- 115.json:16절·5문제·검사 인라인19(C++18/CMake1)·실행블록2·접힌 전체파일5.
  수치 의미→기대점수/K/정수범위→XP 합/레벨/표시→스키마/이관→원자적정산→
  최초내역/어뷰징 경계→실패실험/실습으로 재구성했다. 집필환경 보고는 본문에 넣지 않았다.
- DeepSeek115-levels: 작은 레벨 함수 명세만 tools deny, /tmp cwd로 전송, exit0.
  정상 초안을 검수하고 반복을 최대59회 임계값 비교로 정리했다.
  material/events/draft는 out/learning-jobs/115-levels-*로 보존. 전체 저장소/설정 전송 없음.
- 115-progression은114 누적 코드에서 CMake/README/DESIGN/sqlite_results/migrations/
  migration_contract만 변경하고 progression_policy/levels/schema, 도구/계약검사를 추가.
  v4 careers(RP/XP)·match_progress(당시RP전후/XP/정책), level은 저장하지 않는다.
  기존BP는 보존, 과거경기는RP/XP policy0·0값으로 이관하여 소급 지급하지 않는다.
- 새정산: RP K32/24/16·0바닥·INT_MAX상한, XP100/50, 기존BP10/3. draw변화없음.
  양쪽은 경기전RP로 계산, 경기/BP/RP/XP/내역 모두 같은 트랜잭션으로 확정.
  XP상한·중간예외는 전부롤백, 재전송은 원래내역 확인, 누락내역은storage_error.
  v1/v2 HTTP형식 유지하며 v2policy는BP정책. progression_probe로현재/최초기록비교.
- SCRIPTED/SDL10타깃,각2CTest+이관/보상/멱등/진행계약 통과.
  60레벨전체임계값 직전/정확경계, RP양끝/K구간, 음수표시/INT극한값 검사.
  v3BP보존·무소급이관,XP상한/중간예외롤백,draw,재시작/재전송/최초내역 검사.
  이전프로세스커밋전후kill/복구·HTTP v1/v2/8동시요청/UINT64회귀 통과.
  새C++ASan/UBSan 및 실제numeric/DB검사 통과.115-verification.log 보존.
- 실제 meta/elo.h의 int덧셈/차감 넘침, levels의 큰level곱셈 넘침과음수진행값을
  UBSan/반환값으로 수정전재현(115-root-before.log). double선변환·넓은합/포화,
  레벨산술전범위제한·XP정규화·공통계수로 수정. tests/learning/current_progression.cpp.
- 실제DB프로필/리더보드/정산전RP/저장된RP응답은INTEGER형식·0..INT32범위 확인후변환.
  current_progression_db.cpp는과대/음수/실수/문자RP의정산거절·행/지급보존,
  최대RP정산·잘못된과거RPsnapshot거절을ASan/UBSan으로검사했다.
  실제메타 out/learning-checkpoints/108-meta-boundary-check/meta에서재빌드,
  기존43pytest통과(115-root-build/pytest.log). 처음상위경로빌드오류는올바른경로로수정후재실행.
- Part10§5~8 관련발췌 동기화. RP양끝범위/기대점수 불변성, 레벨59전이/공통상수/
  402전승/평균535판해석,곡선변경의기존표시영향,DB읽기검사 설명교정.
  학습안내README/learning-companion의80/107차시 낡은진도도115/569로갱신.
  coverage344=covered3/partial140/unassigned201/needs-review0.문서28·소스197.
- Part/스니펫19/Markdown27·569문제·전체DOM/기록/앵커/소스/fileURL 통과.
  정적release **31d7e0a1cc69ba67**,15파일·상대경로·해시·재현ZIP 통과.
  out/learning-site/releases/31d7e0a1cc69ba67 및같은ID ZIP.
  HTTP18767일치:index24870/lessons7817487/library5521774/app14480/usability7039.
- 모델/빌드/검사작업모두회수,HTTP18767유지. 하위에이전트/수동GUI/커밋/푸시/배포없음.
  타OS네이티브·운영부하·물리전원장애미검증. 전체보안/인증감사완료로표시하지않음.
- 다음116:학습fixture ID를실제익명계정 식별/자격증명 경계로확장할것.
  기존실제해시/회수/복구흐름을읽고ID와비밀·저장위치·분실/교체·권한계약을나눠설명.
  로그인없는익명계정도인증은필요하며,토큰/키/DB내용은외부모델에보내지않는다.

## 직전 완료 기록 — 114차시

- 2026-09-30: **114차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전113 goal turn은 실제 구현·회귀·교재/HTML·정적검증을 완료한 progress였다.
  로컬 **1~114차시·564문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **115차시 “RP·XP·BP: 서로 다른 목적의 수치”**.
- 114.json:17절·5문제·검사 인라인14(C++13/CMake1)·접힌 전체파일5.
  반복효과/전달/키수명→의미입력/JSON→경합/읽기오류→최초정산/불완전자료→
  응답버전/정수폭/대응검사→보관/권한→실패실험/실습 순서로 재구성했다.
- DeepSeek114-wire: 선택한 작은정산응답 명세만 /tmp cwd/tools deny로 전송, exit0.
  초안에서 key/row/참가자uint64를uint32로줄이는오류와중복JSON재파싱을발견해제거.
  ID는uint64그대로, bp/policy만범위확인뒤uint32변환. std타입과부정확한주석도교정.
  원문/이벤트/초안114-wire-material/events/draft 보존. 최대ID/잘못된필드검사로검수했다.
- 114-idempotency는113 중 README/DESIGN/CMake/sqlite_results/database_service 외
  기존파일보존. 스키마v3/보상정책0·1은그대로. settlement_wire,settlement_probe,
  idempotency_contract 추가. SqlResult는 원래 SavedAwards까지 값으로 반환한다.
- 동일키/내용은같은트랜잭션안에서저장된두지급행을읽어반환한다. 원래내역누락/잘못된
  정책조합은storage_error이며추측재지급없음. 현재잔액과원래응답을구별한다.
  새요청은커밋한지급량, 기존요청은당시내역. API v1네필드유지,v2일곱필드별도경로.
  공통submit람다는값캡처하고store가http보다오래살도록유지했다.
  클라이언트는한번POST/타임아웃·상태·파싱·키/참가자대응을검사하고미확인/409구별.
- 9비키필드각각변경→conflict/무변경, JSON순서/공백변경→동일작업,
  다른키/동일본문→별개작업,후속경기/재열기뒤최초응답동일,지급내역한쪽누락→오류검사.
  정책0·draw·UINT64최대정체성·음수/소수/누락/중복/길이/정책/지급조합파서검사.
- SCRIPTED/SDL8타깃,각2CTest+이관/보상/멱등계약 직접실행 통과.
  이전커밋전후kill/복구/재제출,구경기정책0,HTTP v1호환/v2당시지급·9필드409·
  JSON순서·8동시재전송·최대키진단클라이언트·재시작 검사통과.
  새C++ASan/UBSan,SQLite는Release C11아카이브. vendor경고2종만허용.
  114-verification.log. 인라인14개도최종소스와대조했다.
- 실제saveMatch는BEGIN앞중복조회에서I/O오류를없음으로취급해새경기를커밋하는문제를
  수정전재현했다. 또한독립연결둘이없음을읽는틈에barrier를넣어한쪽UNIQUE오류도재현.
  수정후BEGIN IMMEDIATE뒤조회,ROW/DONE/error분리,MatchTransaction RAII로모든
  미완료반환/예외정리. 기존결과반환은이번읽기만한트랜잭션을롤백하며최초기록은유지.
- 실제C++회귀는수정후조회오류때행0/저장오류·다음정상요청가능,
  독립2연결동일영수증/경기1행·BP합40/XP합150·승패각1 확인. ASan/UBSan통과.
  tests/learning/current_idempotency.cpp, checker의root_contract로재현가능.
  메타재빌드·기존43pytest통과.114-root-before/after/build/pytest.log 보존.
- Part10§7:실제발췌·흐름도를BEGIN→조회로갱신,prepare/step실패,
  RAII종료/기존결과/전체의미입력/JSON/키보관/인증경계를설명했다. 해당H2만부분근거갱신.
  coverage344=covered3/partial138/unassigned203/needs-review0. 문서28·소스197.
- Part/스니펫/Markdown27·564문제·전체DOM/기록/앵커/소스/fileURL 검사통과.
  정적release **bd17bf00e142bbb6**,15파일·상대경로·해시·재현ZIP통과.
  out/learning-site/releases/bd17bf00e142bbb6 및같은ID ZIP.
  HTTP18767일치:index24870/lessons7752217/library5516439/app14480/usability7039.
- 모델/빌드/검사작업모두종료·회수,HTTP18767유지. 하위에이전트/수동GUI/커밋/푸시/배포없음.
  타OS네이티브·운영부하·물리전원장애미검증. 완전한어뷰징/권한/원문전체감사로표시하지않음.
- 다음115:학습의작은BP정책위에RP/XP/BP역할·수치계산·범위·표시계약을구현할것.
  실제elo::expected의int차감/update의int덧셈과DB값읽기,levels의극한값을우선검토.
  Part10§5의기대승률평행이동/K·바닥/제로섬교정은이미113에서완료했으므로유지한다.
  재시도키보관·현재정책으로옛결과재계산금지유지.105worker부분시작/완료예외는별도미검토후보.

## 직전 완료 기록 — 113차시

- 2026-09-30: **113차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전112 goal turn은 구현·오류 수정·원문/HTML·검증을 완료한 progress였다.
  로컬 **1~113차시·559문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **114차시 “멱등성: 같은 요청이 다시 도착하면”**.
- 113.json:18절·5문제·인라인13(C++10/SQL2/CMake1)·접힌 전체파일6.
  부분지급 문제→불변조건/ACID→잔액/지급근거→기존경기 정책→순수계산/범위→
  쓰기/중복/양쪽지급/커밋→당시내역/격리/실패→현재코드/실습 순서로 구성했다.
- DeepSeek113-policy: 작은 순수 지급 함수 명세만 /tmp cwd/tools deny로 전달해 exit0.
  awards_for와 credited_balance 초안을 직접 검수해 음수 short-circuit와 limit-before의
  표현 범위를 확인했다. 자료/응답/초안은113-policy-material/events/draft에 보관한다.
- 113-transactions는112 누적 파일 중 README/DESIGN/CMake/migrations/sqlite_results/
  tests/migration_contract 외 기존 파일 보존. reward_policy/reward_schema,
  reward_contract/reward_probe 추가. 기존 이관 검사는 v3/미래v4에 맞춰 유지했다.
- v3: wallets와 match_rewards. 기존 선수 지갑0, 기존경기 양쪽0/policy0으로 보강하며
  소급지급하지 않는다. 선택아이콘/기존경기 유지. 새경기 policy1은승10/패3/draw0.
  이는 실제 서비스 밸런스와 별개다. 지급 기본키(match_id,player_id), FK/타입/범위 제약.
  시작시 빠진 지갑/지급쌍을 확인하고 새계정/seed도 지갑을 함께 준비한다.
- put은 mutex+BEGIN IMMEDIATE 뒤 기존키를 판별한다. 새경기행·A잔액/내역·B잔액/내역
  모두 성공한 뒤 COMMIT하고 고정크기영수증 반환. 기존키/같은내용은 최초행, 다르면conflict.
  private credit는 쓰기트랜잭션을 전제로 현재잔액+지급량을 검사해 UPDATE하며 changes==1 필수.
  overflow/SQL/hook/COMMIT실패는 전체rollback 및 storage_error/빈영수증.
  awards는 당시 지급량/정책을 조인해 조회한다. 현재balance와 구별하며 별도balance조회가
  동일snapshot이라는 주장은 하지 않는다. 후크/대기옵션은 로컬 실험용이며HTTP노출없음.
- SCRIPTED/SDL 변경·신규6타깃, 각2CTest+직접migration_contract/reward_contract 통과.
  예외4지점, 실제두번째지급SQL ABORT, B잔액상한의A포함rollback, COMMITbusy,
  draw/재시도/conflict/없는선수,8독립연결의서로다른키/동일키,파일재열기 검증.
  별도프로세스커밋전/후kill,미커밋부분값비노출,복구/재제출,구경기0정책,
  HTTP8동시재전송/409/재시작 검사통과. 새C++ ASan/UBSan,SQLite는Release C11.
  로그113-verification.log. vendor SQLite경고2종만허용, 학습C++새경고없음.
- 실제 meta/database.cpp의보상갱신을보강했다. BP/XP/증가할승패카운터의INTEGER타입과
  signed32범위를WHERE로검사하고SQLITE_DONE+changes==1일때만성공한다.
  수정전B의4종상한이모두HTTP200으로커밋되는문제재현, 수정후500·전체경기/양쪽수치/
  history원복·상한해제뒤동일키정상처리·같은키재전송응답동일을검사했다. 전체43pytest통과.
  root이진재빌드,113-root-before/build/pytest.log. winner=null의구식검증실패주석도교정.
- Part10§7현재발췌갱신, IMMEDIATE가모든잠금실패를제거한다는설명교정,
  DONE/변경행수·누적값범위·전체rollback·커밋후응답유실설명추가.
  §5 및 meta/elo.h주석:평행이동은기대승률만보존,K구간/바닥은별개이며
  서로다른K면항상제로섬이아님을교정. FIDE직접리베이스라는근거없는표현제거.
  실제함수299vs300은+16/-12(합+4),100/101→300/301은기대승률같고gain16→12.
  근거113-rating-explanation.log. 계산정책자체는이번에변경하지않았다.
- Part10§5/§7만부분대응근거갱신. coverage344=covered3/partial138/unassigned203/needs-review0.
  문서28·소스197. 완전한원문대응·전체보안검증으로표시하지않는다.
- Part/스니펫13/Markdown27·559문제·전체DOM/기록/앵커/소스/file URL 검사통과.
  정적release **c02f752f00e3a6d9**,15파일·상대경로·해시·재현ZIP통과.
  out/learning-site/releases/c02f752f00e3a6d9 및 같은ID ZIP.
  HTTP18767일치:index24870/lessons7690704/library5513284/app14480/usability7039.
- 모든모델/빌드/검사프로세스종료·회수,HTTP18767유지. 하위에이전트/수동GUI/커밋/푸시/배포없음.
  타OS네이티브·실서비스부하·물리전원장애미검증.집필검증보고는본문밖에보관.
- 다음114:완료된지급근거를살려동일작업식별/응답유실/경쟁요청을더깊게구현할것.
  실제saveMatch는키조회가BEGIN앞에있고step오류를없음처럼진행하는후보가남아있음.
  독립연결경합과읽기오류를재현후수정여부판단.115에서는RP int극한값덧셈/차감표현범위도검토할것.
  105 worker부분시작/완료예외경로는별도미검토후보유지.

## 직전 완료 기록 — 112차시

- 2026-09-30: **112차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전111 goal turn도 구현·원문·HTML·검증을 완료한 progress였다.
  로컬 **1~112차시·554문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **113차시 “트랜잭션: 결과와 보상을 함께 확정”**.
- 112.json:18절·5문제·검사 인라인16(C++15/CMake1)·접힌 전체파일5.
  선택 아이콘→구조/데이터 계약→지원 버전→이력/헤더→구조 판별→쓰기 경계→RAII→
  ALTER/보강→일관성 검사→반복/선택→실패/복원→실제 단계별 커밋→실습 순서로 구성.
  작성자의 검증 보고는 본문 밖 review/validation에 보관했다.
- DeepSeek112-transaction: 선택한 작은 RAII 명세만 /tmp cwd/tools deny로 전달해 exit0.
  BEGIN 성공 뒤 active, COMMIT 성공 뒤 해제, 소멸자 best-effort rollback, 비복사/비이동,
  중첩 거절과 빌린 DB 수명을 직접 검토했다. 자료/응답/초안은112-transaction-*.
- 112-migrations는111 중 README/DESIGN/CMake/sqlite_results.h 외 기존 파일 보존.
  transaction.h/migrations.h, schema_upgrade, migration_contract 추가.
  BEGIN IMMEDIATE를 버전/구조 판별 전에 시작한다. known unversioned/v1/v2와
  SQL 덤프의 header0 복원을 지원, 미래/다른앱/구조·이력 불일치/외부테이블을 거절한다.
  스키마 토큰 비교는 알려진 선언만 보수적으로 인식하며 SQL 의미 동등성 검사가 아니다.
  추가 인덱스는 허용하고 모든 인덱스 정의를 검증하지 않는 범위를 본문에 명시했다.
- v1→v2는 selected_icon_id NOT NULL DEFAULT default 추가, default 카탈로그/소유 보강,
  이력·헤더·검사를 한 커밋으로 묶는다. v2 재실행은 ruby 등 사용자 선택을 보존한다.
  add_player는 기본 소유와 함께 트랜잭션, select_icon은 소유 EXISTS 조건부 UPDATE.
  선택 열의 신규 FK는 없으며 이 쓰기 경계와 시작 시 도메인 검사로 계약을 유지한다.
- 검수 중 NOT LIKE sqlite_%의 _ 와일드카드가 sqliteXforeign 일반 테이블을 숨기는
  결함을 발견, NOT GLOB sqlite_*로 변경하고 외부 테이블 거절·원래 행 보존 회귀 추가.
- SCRIPTED/SDL 변경·신규5타깃, 각2CTest와 별도 migration_contract 통과.
  새/구/v1/v2/미래/외부/불일치/과거 FK 위반, 실패3지점, COMMIT busy rollback,
  반복 선택/새 계정 기본 소유를 검사했다. 프로세스 kill 뒤 저널 복구·동시 두 이관·
  SQL 덤프/Backup API 복원·동일 경기 HTTP 영수증·선택 보존 검사도 두 구성 통과.
  새 C++ ASan/UBSan, SQLite는 Release C11 아카이브. 첫 빌드 vendor 진단2종 기록.
  최종 로그112-verification-final.log. 마지막 본문 메모는 자동 DOM 단일차시 재확인.
- 실제 meta/database.cpp: user_version/두 marker/PRAGMA table_info 읽기 오류를
  0·부재로 취급하지 않고 시작 실패로 처리. 옛 token의 TEXT 타입과 전체 바이트를
  검증해 NUL 뒤 내용을 잘라 정상 키처럼 해시화하는 오류를 수정했다.
  수정 전 version 읽기 오류는 RP1500→300 재변환, 다른 메타데이터 오류도 잘못 처리됨을
  재현했다. 수정 후4종 오류 주입 모두 실패 진단과 RP1500 보존, ASan/UBSan 통과.
  재현 스크립트 scripts/check_learning_migrations_root.py와 tests/learning/current_migrations.cpp.
- 실제 NUL 토큰 회귀는 수정 전 시작 허용 실패를 재현하고, 수정 후 시작 거절·옛 token
  열/전체 값 보존·hash marker 부재를 확인했다. 먼저 커밋된 RP1500→300은 남는 것이
  실제 단계 경계다. 메타 재빌드와 전체39pytest 통과.112-root-pytest.log 및 root-* 로그.
- Part10§3/§4: 현재 발췌 동기화, IF NOT EXISTS/duplicate column 한계, RP 반복 변환
  수치, SQL 덤프와 바이너리 backup/restore, 읽기 오류, RP/계정/물리정리 커밋 경계를 교정.
  Part17§3/§7: 전체 바이트 검증·메타데이터 오류·논리 이관/물리정리·백업 설명 동기화.
  해당4개 H2만 부분 근거 갱신. coverage344=covered3/partial137/unassigned204/needs-review0.
  문서28·소스197. 원문 전체 대응이나 모든 운영/보안 감사 완료로 표시하지 않았다.
- Part/스니펫16/Markdown27·554문제·전체DOM/기록/앵커/소스/file URL 검사 통과.
  최종 정적 release **02f4a7b2182993d5**,15파일·상대경로·해시·재현ZIP 통과.
  out/learning-site/releases/02f4a7b2182993d5 및 같은ID ZIP.
  HTTP18767 일치:index24870/lessons7622319/library5508219/app14480/usability7039.
- 모든 모델/빌드/검사 프로세스 종료·회수, HTTP18767 유지.
  하위 에이전트/수동GUI/커밋/푸시/배포 없음. 타OS 네이티브·물리 전원장애·실서비스 부하 미검증.
- 다음113: SQLite 결과 저장 경계 위에 결과·보상·영수증을 함께 확정하는 계약을 구현할 것.
  학습 user_version 정책과 실제 RP 호환 플래그를 혼동하지 말고 원자성 범위를 명시할 것.
  Part10§5의 Elo 평행이동/K-factor/제로섬 설명에 과장 후보가 보였으므로 보상 집필 시 검토할 것.
  105 worker 부분 시작/완료 예외 경로는 별도 미검토 후보로 유지한다.

## 직전 완료 기록 — 111차시

- 2026-09-30: **111차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  직전110 goal turn은 실제 구현·원문·HTML·검증을 완료한 progress였다.
  로컬 **1~111차시·549문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **112차시 “스키마 이관: 기존 데이터를 보존”**.
- 111.json:18절·5문제·검사 인라인12(C++10/SQL1/CMake1)·접힌 전체파일5.
  결과 계약→저장id/요청키/시간 구분→B-tree/복합키/커버링→OR/UNION ALL→
  EQP/작업량/통계→쓰기/공간/유일성→실제파일목록 순서로 구성했다.
  SCAN을 무조건 전체읽기로, VM계수를 밀리초/디스크I/O로, 인덱스를 무조건 빠름으로 표현하지 않았다.
- DeepSeek111-observer는 선택한 작은 진단 helper명세만 /tmp/tools deny로 전달, exit0/step_start/text.
  query_observer.h 초안을 직접 검수해 utility include·SQL길이의 불필요한int narrowing·
  이름 충돌 가능 deleter 이름·반복 방어 주석을 교정. 자료/응답/초안은111-observer-*.
  입력/응답/계획을진단텍스트로복사하고NULL은optional로구분, StatementRAII·바인딩오류·계수확인.
- 111-indexes는110 파일 중 README/DESIGN/CMake/sqlite_results.h 외 바이트 보존.
  history_queries.h와 query_observer.h, tools/index_probe.cpp/history_list.cpp 추가.
  SqliteResults는 생성시 두 인덱스를 준비하고 recent(player,limit)를 추가했다.
  양쪽참가위치 UNION ALL·전체 ORDER BY id DESC LIMIT1..50, 양수ID검사·mutex·타입복원 유지.
  player_a<>player_b CHECK가 두 결과집합의 비중복 전제다. 정렬은 저장접수id순서로
  실제경기종료시간이나TEXT요청키순서와구별한다. 목록은 row/key의값복사로반환한다.
- 두(player,id DESC,match_key) 인덱스가 검색/정렬/반환열을지원한다. 진단은HTTP경로밖이다.
  history_list는기존로컬학습DB만열고없는파일/잘못된ID/상한거절. 실제계정DB와혼용불가.
- SCRIPTED/SDL에서 변경/새4타깃(study_meta_db/sqlite_tables_contract/index_probe/history_list),
  각2CTest·각HTTP저장/반대참가위치/상한/재시작/기존110형DB인덱스추가 검사 통과.
  meta_submit소스와의존은110에서바이트보존, 이번프로세스검사는고정HTTP요청으로연결했다.
- 12,000경기/64선수×1·5·50상한의독립생성oracle와결과/순서를대조했다.
  OR/UNION ALL·인덱스전후·ANALYZE·누락선수·TEXT숫자정렬·NULL/빈문자열·바인딩오류·
  UNIQUE생성실패/중복거절·성능인덱스삭제후데이터계약도검사했다.
  새C++진단/조회는ASan/UBSan,SQLite는동일Release C11아카이브. vendor진단2종만별도기록.
- 고정fixture선수17/limit5: 무인덱스UNION ALL fullscan305/sort0/vm1003,
  복합인덱스UNION ALL 0/0/97, OR복합인덱스0/1/4556. 결과5행동일.
  논리페이지306→506. 데이터/엔진에대한관찰예로본문에명시했으며속도비율주장없음.
- 실제 meta/database.cpp는중복 idx_player_icons_pid생성을DROP INDEX IF EXISTS로교체.
  두키소유확인이복합PK커버링검색인것을확인. 새회귀는수정전옛인덱스잔존실패재현,
  수정후기존DB2회재시작에서소유행/인증/복합PK중복거절/조회경로보존. 전체38pytest통과.
  데이터행/계정/PK인덱스를삭제하지않는다. 이름이지정된기존성능용인덱스만정리한다.
- 실제랭킹SQL과스키마도100개합성계정으로동점/순서검사:
  SCAN players USING INDEX idx_players_elo, 결과는RP내림/ID오름과일치.
  이는PythonSQLite3.46.1진단이며학습계수는vendoredSQLite3.46.0실행으로구별한다.
- Part10§3 발췌갱신, 실제랭킹정렬/비커버링반환열/소유보조인덱스정리,
  실행계획·일반인덱스/UNIQUE·통계/쓰기비용설명을같은양식으로보강했다.
  §3만부분근거갱신. coverage344=covered3/partial134/unassigned207/needs-review0,
  문서28·소스197. 원문전체대응이나실서비스부하검증완료로표시하지않는다.
- Part/스니펫/Markdown27·549문제·전체DOM/앵커/기록/소스/fileURL 검사 통과.
  정적 release **6031db1d350e7f3f**,15파일·상대경로·해시·재현ZIP통과.
  out/learning-site/releases/6031db1d350e7f3f 및 같은ID ZIP.
  HTTP18767 일치:index24870/lessons7554059/library5502335/app14480/usability7039.
- 모든모델/빌드/검사프로세스종료·회수. HTTP18767유지. 하위에이전트/수동GUI/커밋/푸시/배포없음.
  타OS네이티브/실서비스부하/디스크지연/전원장애미검증. 검증보고는본문밖에기록했다.
- 다음112는기존행을보존하는스키마이관. 실제execSchema의ALTER/marker/token변환을읽고
  새DB·구DB·재실행·실패시보존의전제를검사할것. IF NOT EXISTS는정의갱신이아님을유지한다.
 105 worker부분시작/완료예외경로는별도미검토후보로남는다.

## 직전 완료 기록 — 110차시

- 2026-09-29: **110차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~110차시·544문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **111차시 “인덱스: 조회 비용과 유일성”**.
- UI 요청은 이미 완료: 단색✓·같은 origin 브라우저 완료 기록·현재 위치와 구별한 이전 읽던 위치.
  이번에는 중복 수정하지 않고 전체 DOM 회귀 검사를 통과했다.
- 110.json:18절·5문제·검사한 인라인19(C++13/SQL5/CMake1)·접힌 전체파일5.
  행의 의미→안정적 ID→타입/정규 표현→다대다/복합키/외래키→NULL→RAII/바인딩→
  저장/복원→HTTP 연결→재시작 순서로 재구성. SQLite 공식 문서를 해당 설명에 연결했다.
- DeepSeek110-sqlite의 작은 RAII 명세 초안을 검수해 sqlite_handle.h로 사용했다.
  unique_ptr 보호자·실패시 핸들 정리·TRANSIENT 바인딩·storage class 검사·ROW/DONE/error 구분.
  <utility>와 선언 타입/실제 값 타입 주석을 교정했다. 추가110-format은 선택된 저장 클래스만
  전송해 서식 정리, C++ 전체 토큰/문자열/주석 동일성 검사 후 반영하고8스니펫을 동기화했다.
  두 작업 모두 /tmp cwd/tools deny, step_start/text·exit0. 자료/응답/초안은110-*-material/events/draft.
- 체크포인트110은109의 파일을 README/DESIGN/CMake 외 바이트 보존한다.
  신규 meta/schema.h/sqlite_handle.h/sqlite_results.h/database_service.cpp 및 계약 검사 추가.
  메모리 study_meta 타깃은 비교용으로 보존, 새 study_meta_db가 파일을 소유한다.
- 스키마는 players/icons/player_icons/matches. uint64 wire의 전체 범위를 정규 십진 TEXT로 보존,
  숫자/선행0/길이/상한/NUL 검사. 라인 uint32는 범위/실제INTEGER 검사. 행 id는 signedINTEGER PK.
  필수 키 NOT NULL, 복합 소유 PK, FK 활성화 확인, 소유cascade/경기참조삭제거절,
  참가자다름·nullable승자참가관계 CHECK. DB의NULL과 학습 HTTP winner0/1/2를 구분했다.
- SqliteResults는 연결과 mutex를 소유한다. 동일 키/본문이면 최초 행, 다르면conflict,
  알수없는참가자invalid, SQL실패storage_error. 조회는 부재만nullopt이며 오류를 빈 결과로 숨기지 않는다.
  다른 연결의 조회/삽입 경합은 DBunique가 중복을 막되 이번요청503일 수 있다고 범위를 명시했다.
- SCRIPTED/SDL 새3타깃 빌드·각1CTest 통과. C11 SQLite 아카이브 링크, 새 C++에 경고 없음.
  vendor SQLite3.46/GCC15의 discarded-qualifiers/stringop-overread2경고는 별도기록하며
  외부코드를 변경하거나 전체경고를 숨기지 않았다. 체크스크립트는 이 두 vendor경고만 허용한다.
- 계약 검사는 이름변경/동명이인, SQL모양이름, 복합중복/FK/NULL/부호·범위·NUL/타입,
  삭제cascade와참조보존, UINT64_MAX왕복, 16스레드한키, 파일재열기, RAII오류후사용을 확인했다.
  ASan/UBSan은 새 C++ 래퍼/저장코드에 적용했고 SQLite엔진은 동일 Release아카이브다.
- 각구성 실제HTTP검사: 호출자재시작, 서비스재시작 뒤 같은행,12동시동일요청,409/400/413,
  최대정수·무승부NULL, 별도writer의파일잠금503/rollback뒤동일요청200, 잘못된DB경로는수신전실패.
  검사프로세스/임시DB정리, 기존경로덮어쓰기/삭제없음. 로그110-verification.log.
- 실제 meta/api_server.cpp의 winner 해석오류를 수정. find_int빈값은 명시적null과
  누락/문자열/소수/불리언/배열/객체/범위초과를 구분하지 못해 잘못된무승부와 UUID확정이 가능했다.
  객체의필드존재와명시적null을별도검사하고 나머지는정수해석성공필수. malformed7종수정전실패,
  수정후37 HTTP·SQLite검사통과. 400뒤같은UUID정상요청/정상보상, null무승부도검사.
- Part10§3/§10: bot_rewards 표누락, 카탈로그의C++책임, FK와참가관계차이,
  nullable키·TEXT PK예외, 복합PK인덱스선두검색, busytimeout범위, 실제API발췌,
  명시적null과해석실패, 범위밖signed변환의C++17구현정의설명을교정했다.
  rootDB제약/인덱스/마이그레이션은 이번에 바꾸지 않았다.
- coverage344=covered3/partial134/unassigned207/needs-review0, 문서28·소스197.
  Part발췌/Markdown27/544문제·전체DOM·기록/앵커/소스/fileURL 통과.
  최종서식은토큰동일성을검사하고 스니펫/HTML/정적검사를갱신했으며 동일DOM전체를중복실행하지않았다.
- 최종정적 release **5314114cf0f6adc7**,15파일·상대경로·해시·재현ZIP통과.
  out/learning-site/releases/5314114cf0f6adc7 및 같은ID ZIP. HTTP18767바이트일치:
  index24870/lessons7487923/library5496157/app14480/usability7039.
- 모델/빌드/검사작업 모두종료·회수, HTTP18767 유지. 하위에이전트/수동GUI/커밋/푸시/배포없음.
  타OS네이티브/전원손실/WAN/공개인증없는학습서비스운영은미검증. 집필검증은본문에넣지않았다.
- 다음111:복합키의선두열과쿼리계획·커버링/정렬/쓰기비용·유일성의역할을 실제조회로구현할것.
 110의정규TEXT키는기본숫자정렬/집계와다름을유지하고, 실제idx_player_icons_pid중복후보는
 실행계획과범위를검토해판단할것. 105의worker부분시작/완료예외경로는별도미검토후보로남는다.

## 직전 완료 기록 — 109차시

- 2026-09-29: 직전108차시는 progress. **109차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~109차시·539문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **110차시 “테이블과 키”** (정확한 제목은 course-plan 확인).
- 109.json:15절·5문제·인라인11(C++10/CMake1)·접힌 전체파일6.
  번역단위/모듈·스레드·프로세스, 주소 공간과 값 직렬화, HTTP와 DB 소유, 요청 동기화,
  호출자/서비스/영속 기록의 수명, liveness/readiness를 구현 순서로 재구성했다.
  SQLite serverless와 cpp-httplib 공식 자료는 필요한 범위로 연결했다.
- OpenCode DeepSeek109-store 정상 종료(step_start1/text1), 도구 호출 없음.
  /tmp cwd/tools deny로 작은 MemoryResults 명세와 필요한 타입 계약만 전송했다.
  material/events/draft는 out/learning-jobs/109-store-*에 보존. 반복적 제한/검수 안내
  주석을 정리하고 실제 저장·HTTP·프로세스 검사와 설명은 직접 검수했다.
- 109-meta-service는108 누적 파일을 README/DESIGN/CMake 외 바이트 보존한다.
  MemoryResults<Capacity>는 고정 배열·mutex를 소유하고 같은 키/내용은 원래 행 반환,
  다른 내용은 conflict, 새 키의 공간 소진은 full이다. 기존 키 비교를 용량 검사보다 먼저 한다.
  기본 유효성 검사는 제출 prepare를 재사용하고 lookup은 잠금 안에서 복사한 값을 반환한다.
  행은 프로세스 메모리이며 재시작 복구·인증·보상·영속 저장으로 표현하지 않는다.
- wire.h:1024바이트·평평한 객체·중복 키 거절·정수 타입/부호/폭·필드 수 계약.
  학습 /study/v1/matches 형식은 실제 /v1/matches와 비호환이다.
  HttpSender는 한 번만 HTTP 호출하며 전송/해석 실패는 미확인이다. 시간 제한은 단계별이다.
- 별도 study_meta와 meta_submit 실행 파일을 추가했다. 서비스는127.0.0.1 고정,
  HTTP worker2/대기열8·행64, POST400/409/503/200과 단순 healthz를 제공한다.
  main이 저장소를 먼저 소유하고 핸들러가 빌린다. normal listen 반환과 OS 강제 종료를 구분한다.
  호출자는 실제 Duel을 종료시키고 고정 선수/시드의 결과를 제출한다.
  최초 Release 경고로 Intent의 미초기화를 확인해 left{},right{} 값 초기화로 교정했다.
- 새 저장/JSON 계약은 ASan/UBSan,24스레드 같은 키,용량 소진 뒤 재전송·값 복사·
  정수/타입/중복/구조/본문 크기·UINT64_MAX 왕복을 검사했다.
  새3타깃 SCRIPTED/SDL 빌드와 각각1CTest, 각 구성의 실제 두 프로세스 실험 통과.
  호출자 재시작 뒤 동일 행, 새 키 행,12 HTTP 동시 요청의 동일 결과,내용 충돌409,
  본문400/413,서비스 재시작의 메모리 초기화,서비스 종료 뒤 미확인과 종료코드3 확인.
- 실제 meta/main.cpp는 비밀키 없는 --allow-public-matches에 숫자loopback만 허용한다.
  0.0.0.0/::/192.0.2.1/localhost의4조건이 수정 전 DB 열기로 진행하는 것을 재현했다.
  의도적으로 존재하지 않는 DB 부모를 써 외부 리스너 없이 검사했다.
  수정 후에는 DB 접근 전 코드2/정책 오류로 거절한다. 비밀키가 있는 운영 경로는 유지한다.
- 실제 tetris_meta 재빌드 및 HTTP·SQLite29검사 통과. 별도 재시작 검사에서 임시 DB의
  커밋된 경기 응답·인증·RP/BP/XP가 메타 프로세스 종료/재시작 뒤에도 보존됨을 확인했다.
  신규 fixture 프로세스는 terminate/wait와 timeout kill/wait로 회수한다.
  이번 변경은 메타 진입점과 주석이며 게임/relay 전체 CTest를 반복하지 않았다.
- API header의 구식 입장 토큰 설명·healthz 범위를 교정했다. DB header의
  모든 public 메서드 잠금/스키마 실패 optional/항상500이라는 부정확한 주석을 교정했다.
  DB 생성자는 schema 실패 시 연결을 닫고 throw하며 카탈로그 조회는 연결을 쓰지 않는다.
  빈 목록을 반환하는 일부 조회의 오류 표현은 전체 API 감사 완료로 취급하지 않는다.
- Part10§1/§2/§10/§12: 메타의 일시/영속 상태,내장 SQLite,프로세스 분리 비용,
  파일 저장의 원자성 가능성과 비용,healthz 범위,DB/API 수명,개발 주소 제한을 반영했다.
  Part12§4·Part13§3에도 동일 개발 모드 정책을 반영했다. 검토 소절만 partial 근거 갱신.
  coverage344=covered3/partial133/unassigned208/needs-review0. 문서28·소스197.
- Part/Markdown27·539문제·전체 DOM/단일차시/앵커/소스/완료저장/이전위치/file URL 통과.
  정적 release **394c9fcdb575ea5a**,15파일/상대경로/해시/재현ZIP 통과.
  경로 /data/Tetris-Multiplayer-RL/out/learning-site/releases/394c9fcdb575ea5a 및 같은 release ZIP.
  HTTP18767 일치: index.html: 24870, lessons.js: 7410320, library.js: 5490839, app.js: 14480, usability.js: 7039.
- 모든 모델/검사 작업 종료,HTTP18767 유지. 하위에이전트·수동GUI·커밋/푸시/배포 없음.
  타OS 네이티브·IPv6 실제 연결·WAN/부하·전원 손실·공개 서비스 인증 없는 실습 운영은 미검증.
- 다음110은 같은 저장 경계 아래 실제 테이블·행·열·키와 타입의 의미를 구현한다.
  현재109의 메모리 대역을 영속 저장으로 오인시키지 말고, 안정된 제출 값/HTTP 계약과
  SQLite 정수 범위·키 표현·초기화/열기 실패를 함께 설계할 것. 인덱스 전체는111 주제다.
 105에서 남긴 worker 부분 시작/완료 예외 경로는 별도 미검토 후보로 유지한다.

## 직전 완료 기록 — 108차시

- 2026-09-29: **108차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~108차시·534문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **109차시 “메타 서비스: relay와 DB를 나누는 이유”**.
- 최근 UI 요청은 이미 반영: 완료 단색 ✓, 같은 origin/브라우저의 completedLessons 저장,
  현재 주소로 덮어쓰지 않는 이전 읽던 위치. 이번에는 중복 재구현하지 않고 DOM 회귀 확인.
- 108.json:15절·5문제·인라인11(C++10/CMake1)·접힌 전체파일3.
  전달/저장/확인, 부분 실패, 원자성/영속성, 동일 작업의 식별, 미확인 지식 상태,
  소유한 값의 불변성, 콜백 계약, 결과 검증, 호출 수/시간 제한을 구현 순서로 재구성했다.
  공식 SQLite 원자적 커밋과 RFC9110 멱등 정의는 본문에 필요한 범위로 연결했다.
- OpenCode DeepSeek108-submission 정상 종료(step_start1/text1), 도구 호출 없음.
  /tmp cwd와 tools deny로 좁은 제출 명세만 보냈다. material/events/draft는
  out/learning-jobs/108-submission-*에 보존. 받은 net/match_submission.h를 직접 검토하고
  콜백의 비재진입·요청 참조 수명·시간 예산 한계와 enum 고정 underlying type을 보강했다.
- MatchSubmission: 한 소유자/직렬 호출, 복사·이동 금지, empty에서 prepare 한 번.
  값 복사 후 요청은 const로만 읽는다. 기본 ID/승자 검사는 실제 경기 검증과 구별한다.
  submit은 최대 한 번 noexcept 콜백을 호출하고 먼저 횟수/미확인 상태를 기록한다.
  key·양수 row·참가자 순서가 맞는 영수증만 confirmed. 횟수 소진은 기존 상태를 보존한다.
  재시작 outbox/무조건 전달/정확히 한 번 효과를 객체 단독 보장으로 주장하지 않는다.
- 새 TCP probe: 실제 두 연결의 라운드/틱/개수 검사→서버와 두 클라이언트 RoundPlay
  정규 상태 일치→11틱에서 실제 보드 종료→서버 값으로 MatchRecord 생성.
  한 행 메모리 대역이 최초 반영 후 미확인을 반환, 동일 요청 두 번째 호출에 영수증1.
  writes1·내용 변경 거부·확정 후 무호출 확인. key17/round1/seed77/player101/202는fixture.
  방/입장예산은 보존된107 probe와 별도 실습이며 전체 서비스 인증/DB 통합으로 과장 안 함.
- 실제 meta/match_response.h 추가: 전체 JSON 파싱, 중복 키·크기·깊이, 정확한 부모 객체,
  양수 match_id, 정수 타입/범위, 비음수 RP 전후값과 delta 관계를 검사한 뒤 결과를 공개.
  기존 수동 parser62조건 중19실패, 새 parser ASan/UBSan62전부 통과.
  실제 MetaClient HTTP fixture: 첫503 뒤 동일본문 재시도·정상공백 응답·잘못된 부모 응답 검사.
  nullopt는 미저장 증명이 아니며 timeout은 단계별 제한/재시도 진입 마감임을 주석 교정.
- 실제 Database::saveMatch는 동일 UUID의 모든 저장 입력(순서있는 참가자·nullable승자·
  점수·라인·시간)을 비교한다. 내용 변경은 MatchSaveError::IdentityConflict,
  API409 match_conflict로 거절한다. 동일 내용은 최초 응답 반환·중복 보상 없음 유지.
  변경내용7조건 수정전200 실패 재현, 수정후409와 원래 응답/계정 집계 보존 확인.
  전용 실제 tetris_meta 빌드와 HTTP·SQLite24검사 통과. 시험 DB는 fixture 임시경로 사용.
- 체크포인트107 누적 파일은 README/DESIGN/CMake 외 바이트 보존.
  새 contract/probe ASan/UBSan, SCRIPTED2/SDL2 CTest와 인라인11 대조 통과.
  root83/54 구성 재빌드 및27/26CTest 통과, 변경 빌드 로그 warning 없음.
  Part/Markdown27·534문제·전체 DOM/단일차시/완료저장/이전위치/file URL 계약 통과.
- Part10§7/§10/§13의 현재 코드 발췌, UUID 내용 충돌, 미확인/전체시간 오개념,
  응답 파싱을 교정했다. Part7§3 스텁 nullopt 의미만 교정했다.
  검토한 소절만 partial 근거 갱신. coverage344=covered3/partial130/unassigned211,
  needs-review0. 문서28·현재 소스197. 다른 인증/카탈로그 파서는 전체 감사하지 않았다.
- 정적 release **19f47a6eeee8d836**, 15파일/상대경로/해시/재현ZIP 통과.
  경로 /data/Tetris-Multiplayer-RL/out/learning-site/releases/19f47a6eeee8d836 및 같은 release ZIP.
  HTTP18767 파일 일치: index.html: 24870, lessons.js: 7351836, library.js: 5483378, app.js: 14480, usability.js: 7039.
  원문 대응표 갱신 뒤 lessons.js 재생성이 한 번 필요했고 최종 최신성 검사 통과.
- Windows/macOS 네이티브·수동GUI·WAN/부하·크래시/전원장애·재시작 outbox 미검증.
  집필 검증 보고는 review/validation/기록에만 둔다. 하위에이전트·커밋/푸시/배포 없음.
  모든 모델/검사 작업 종료, HTTP18767 유지.
- 다음109는 실행 파일·HTTP 포트·DB 소유와 계정 서비스의 의존 방향을 세운다.
  테이블/키(110)·인덱스(111)의 전체 내용을 미리 끌어오지 않는다.
  현재 검사하지 않은105 worker 부분 시작/완료 예외 경로는 별도 후보로 유지한다.

## 직전 완료 기록 — 107차시

- 2026-09-29: 직전106차시는 progress. **107차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~107차시·529문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **108차시 “메타와 연동: 전달과 영속성의 경계”** (unit-108).
- 107.json16절·5문제·인라인17(C++16/CMake1)·접힌 전체파일3.
  107-connection-budget는 README/DESIGN/CMake 외106 누적 불변 파일 바이트 보존.
  동시점유/속도/체류시간/저장량 단위, 안정상태 Little 관계의 범위, 출처별/전체/단계별
  예산, 큐/worker의 서로다른인구, 이동소유·최종State파괴·정수경계·강한예외보장을 재구성했다.
- OpenCode DeepSeek107-budget 정상종료, step_start1/text1, 도구 호출 없음.
  /tmp cwd/tools deny로 승인한 작은 학습 Budget 명세만 전송. 선택자료/응답은
  out/learning-jobs/107-budget-{material.txt,events.jsonl,draft.json}에 보존.
  초안은 전체상한0만비교를건너뛰어 무제한처럼 처리했으며 최초contract 실패 재현후수정.
  Lease빈상태는 nullState로 구별하고 이동원본slot은0으로 초기화해 Release -Warray-bounds
  경고도 제거했다. utility직접include·불변조건을숨기는조건부감소·mutex대기범위 주석교정.
- 기준 ConnectionBudget<MaxKeys>: 생성시한번sharedState할당, 고정키표O(MaxKeys), 입장당
  별도힙할당없음. 전체/출처별 세션·핸드셰이크, 전체/연결별 선언바이트의6상한.0은거절.
  mutex안검사+동시확정, 실패무변경. 이동전용Lease가 세션1/선택적핸드셰이크/바이트소유.
  finish_handshake는한번만반납, reset멱등, 이동대입은대상예약먼저반납·자기이동유지.
  n<=limit-used로오버플로방지. 마지막State소유참조는mutex해제후소멸.
  발급Lease는Budget보다오래살수있지만 Budget메서드사용자완료전파괴는허용안함.
  같은Lease접근은호출자직렬화, 서로다른Lease공유계수는mutex보호. stop_accepting은drain아님.
- TCPprobe: Peer첫멤버Lease로해당Peer의parser/socket보다늦게반납. 모든loopback키1,
  입장당활성FrameParser sizeof만선언예약. 임시/이동후인라인공간·전체RSS/커널제한으로과장안함.
  호스트/게스트2세션에서추가실제TCP수락→Budget거절→서버소켓닫음→클라이언트EOF.
  게스트퇴장소켓정리/모든옛Peer소유자해제→1세션→같은코드재입장→수락/입력전달/
  실제RoundPlay정규상태일치→모든Peer소유범위끝에서세션/핸드셰이크/바이트/키0확인.
  accept이전커널자원/인증/서비스wire구현으로주장안함. 학습HDAAA/round1/seed77/역할fixture유지.
- 실제 IpAdmission·PlayerSessionLease acquire: mutex안예약확정뒤new/controlblock할당하던
  경로에서 객체할당실패의예약누수와 shared_ptr제어블록실패의소멸자재잠금교착을 재현.
  미등록candidate를mutex전에할당, mutex안상한/중복/map삽입뒤counter와registered_확정.
  미등록소멸자는표를건드리지않아 중복요청·실패후다른활성소유자를해제하지않음.
  성공객체의마지막참조가반납하며, 전체프로세스OOM복구/모든호출자예외처리 보장은별도.
- tests/learning/admission_allocation.cpp: fresh IP/existing IP/player × 할당위치0..6의
  21조건별별도프로세스. 수정전3예약누수+3교착timeout, 수정후21전부통과.
  timeout subprocess는검사기가종료회수. 기존소유자·별칭유지/최종해제후재획득도확인.
  tests/learning/admission_contract.cpp:24스레드IP상한3/계정상한1·단계독립·최종소유반납ASan/UBSan.
  재현가능checker scripts/check_learning_connection_budget.py에등록, before원본은out에보존.
- 학습contract:0/정확상한/초과/SIZE_MAX·이동/반복해제/객체수명/마감후명시정리·24스레드상한,
  12000결정적연산을독립적인활성연결행합산모델과비교. contract/probe ASan/UBSan,
  새2타깃 SCRIPTED2/SDL2 CTest 경고없음. 누적불변파일확인, 이전CP전체빌드반복없음.
  실제두relay각IP상한/퇴장재입장/운영종료3passed·0skip, root두구성27/26CTest통과.
- Part7§13:계정lease발췌·등록전할당·worker/연결수·단계예산교정.
  Part12§6:IpAdmission현재발췌·예외안전·RAII만능/공정성단정교정.
  Part14§12.3·§13:동시점유와속도구별, 공유기본값과고정창/tokenbucket/burst차이,
  워커상한과전체연결상한의범위교정. 관련Python통합docstring도동일오개념교정.
- Part/링크·Markdown27·529문제·전체DOM/단일차시/완료저장/이전위치/file URL 계약 통과.
  Windows/macOS네이티브·수동GUI·WAN/부하·인증/DB통합·전체프로세스OOM복구 미검증.
  집필환경검증보고는review/validation/기록에만유지.
- coverage344: covered3/partial127/unassigned214/needs-review0. 문서28/소스196.
  Part7기존partial H2갱신, Part12하나/Part14둘은검토소절만partial근거추가.
  전체Part/보안완료로취급하지않음. 최종정적release **5a8ccb104f772508**,
  15파일허용목록/상대경로/해시/재현ZIP통과. HTTP18767일치:
  index24870/lessons7295123/library5480441/app14480/usability7039바이트.
  out/learning-jobs/107-{allocation-before,allocation-after,checkpoint,checkpoint-final,
  final-contract,snippets,root-tests,threaded,reactor,markdown,dom,site,http}.log.
- 모든 모델/검사작업 종료·결과회수, HTTP18767유지. 하위에이전트·수동GUI·커밋/푸시/배포 없음.
- 다음108읽은지점: server/relay.cpp finalizeRanked(128)은summary를확정계기로만쓰고
  verified서버결과를meta->post_match로전달, optional실패시SaveFailed표시.
  meta/http_client.cpp post_match(362)은match_uuid로동일본문재시도/남은시간예산,
  응답하위객체를수동문자열find로찾아parse한다. 응답유실과영속성불확실/멱등키,
  수신성공과저장확인의차이를작은누적port로설계할것. 수동JSON공백/잘못된값/응답객체
  검증과전체wall-clock보장주석은추가대조후판정할후보. 아직실패재현/수정안함.
  아직108 원고/체크포인트없음. 105의worker부분시작/완료예외경로는별도미검토로남음.

## 직전 완료 기록 — 106차시

- 2026-09-29: 직전105차시는 progress. **106차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~106차시·524문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **107차시 “연결 제한: 메모리·큐·시간의 상한”** (unit-107).
- 사용자 최신 ‘계속’에 앞서 UI 요청 반영 상태 확인: 미완료 문구 제거, 단색 ✓,
  기존 completedLessons 저장 유지, 현재 페이지로 덮어쓰던 링크를 이전 읽던 위치로 변경은
  이미63차시 때 완료·공개된 내용. 같은 브라우저·origin 저장이며 기기 자동 동기화 없음.
  이번 턴에서 중복 UI 재구현/공개 배포하지 않고 자동 DOM 회귀로 재확인했다.
- 106.json17절·5문제·인라인14(C++13/CMake1)·접힌 전체파일3.
  106-room-exit는 README/DESIGN/CMake 외105 누적 불변 파일 바이트 보존.
  주소 등록 epoch/참가 ticket/알림 revision, 선형화 지점, 중복·동시 퇴장,
  snapshot·gate→state 순서, mutex 밖 자원 해제, close와 작업 완료 대기를 재구성했다.
- OpenCode DeepSeek106-membership 정상종료, step_start1/text1, 도구 호출 없음.
  /tmp cwd/tools deny로 좁은 학습 멤버십 명세와 승인된 정상 퇴장 발췌만 전송.
  초안의 callback bool 무시·noexcept 누락, close=drain, shared_ptr=Peer 내부 동기화,
  실패 시 호출자 인자 보존, gate가 leave도 막는다는 설명을 직접 교정했다.
- 기준 RoomMembership: 두 슬롯을 state mutex로 관리, epoch/side/member로 현재 참가 검증.
  join은 gate→state로 확정, leave/close는 state만 사용. 정리할 shared_ptr를 결과로 반환.
  동일ticket 중복 퇴장 정확히1removed, 양쪽 퇴장2removed·1empty. 같은Peer 재입장도 새member.
  deliver는 revision/수신자/endpoint/인원을 확인 후 state를 놓고 gate 아래 noexcept callback.
  callback bool을 그대로 전파, false는 stale 또는 송신실패이며 자동 추방하지 않음.
  leave/close는 callback 중 진행 가능, 알림 순서와 순간 최신성을 구분. 외부별칭/Notice는
  Peer 수명을 연장할 수 있으며 Peer I/O/close는 별도 동기화 필요. close는 사용자 drain 아님.
  번호 wrap 대신 exhausted 거절·close 정리. 실제 서비스 카운터에 이 정책 적용했다고 주장 안 함.
- TCP probe: 실제게스트 TYPE16퇴장→서버슬롯제거/소켓정리→클라이언트EOF→같은코드 재입장,
  옛ticket stale/지연notice callback0, 새 인원2 알림을 양쪽클라이언트 수신.
  멤버십 close로 소켓을 닫지 않고 인계해 READY/포워더/반대 TYPE41 입력/실제RoundPlay
  정규 상태 바이트 일치까지 진행. 단일 실행 주체의 소켓 I/O, HDAAA/round1/seed77/역할fixture,
  학습TYPE52는 서비스ROOM_INFO와 비호환. 정확한 동시네트워크퇴장/인증협상으로 과장 안 함.
- 현재 threaded RoomRegistry: roomLoop_에 원래 owning TcpSocket을 전달, ownsSlot_로
  진입/READY/CHAT/상태확인/starter predicate·erase/일반퇴장마다 현재소유 동일성 확인.
  oldreader가 같은 code/role의 새연결을 채택·변경·삭제하지 않도록 함. ROOM_LEAVE 뒤 frame처리 중단.
  일반퇴장 presence 변경 뒤 cv.notify_all로 starter를 깨움. 빈 slot의 TcpSocket참조 제거,
  lease/IP참조는 local로 이동해 mu 밖·peer I/O 전에 해제. 다른별칭 있으면 최종반납은 늦어질 수 있음.
  root 다른 starter/abort 경로 전체 자원파괴를 mutex밖으로 옮겼다는 주장은 하지 않음.
- 수정 전4실패 재현: reader진입 전 방교체, receive중 같은코드 방교체, guest퇴장 후 starter미깨움,
  host방이 생존할 때 빈guest슬롯 transport잔류. 수정 후 모두 ASan/UBSan 통과.
  tests/learning/room_{stale_owner,exit_wakeup,departed_resource}.cpp와
  scripts/check_learning_room_exit.py의 private복사header/스케줄hook으로 재현가능하게 남김.
  wakeup은 predicate가false인순서를 만든 뒤 shutdown없이300ms내완료 확인, 모든스케줄 증명 아님.
  weak_ptr검사는 참조해제 증거이며 실제OS fd계수 측정으로 표현하지 않음.
- 두 실제 relay 각각 정상방/잘못된READY4종/종료/퇴장재입장7passed·0skip.
  기존103 root 생성shutdown·할당예외·미완료join·난수/코드계약 재검사 통과.
  새2타깃 SCRIPTED2/SDL2, 128중복+128양쪽퇴장·gate·소멸·소진계약 및 TCP ASan/UBSan 통과.
  root 기존 두구성 빌드+27/26CTest 통과. 변경없는 이전CP 전체빌드/검사 반복 없음.
- Part7§8 현재발췌/소유확인/조건변수/lease정리/등록·알림번호 설명 교정,
  Part14§8 starter발췌 갱신. present를 실제연결생존 증명으로 설명하지 않음.
  roomInfoVersion은 registry전역발급이며 READY를 포함한 모든변경횟수가 아님.
  state검사는 송신전, gate는 송신중유지, leave중상태변화와 같은해시게이트경합 범위 명시.
- Part/링크·Markdown27·524문제·전체DOM/완료저장/이전위치/단일차시/file URL 계약 통과.
  DOM 실행뒤 최종Part 주석 교정만 추가, library/정적해시/Part 재확인.
  Windows/macOS네이티브·수동GUI·WAN/부하·모든스케줄·ranked인증/DB통합 미검증.
- coverage344: covered3/partial124/unassigned217/needs-review0. 문서28/소스196.
  Part7/14 각H2 한개의 대조근거만갱신, 전체Part/보안완료로 취급하지 않음.
  최종 정적release **41a9acbb824b3c21**,15파일 상대경로/허용목록/해시/재현ZIP 통과.
  HTTP18767일치: index24870/lessons7216905/library5478266/app14480/usability7039바이트.
  out/learning-jobs/106-{checkpoint,final-checkpoint,root-before,root-after,root-permanent,
  prior-root,root-tests,threaded,reactor,markdown,dom,site,http}.log.
  모든 모델/검사작업 종료·결과회수, HTTP18767유지. 하위에이전트·수동GUI·커밋/푸시/배포 없음.
- 다음107 읽은지점: server/ip_admission.h acquire가 lock(mu)안에서 table[key]=n+1 뒤
  new IpAdmission 및 shared_ptr제어블록 할당. 메모리할당 실패시 예약반납/소멸자 재잠금
  경로를 재현해볼 후보. 아직 실패주입/결함판정/수정은 안 함.
  주석의 handshake상한=속도제한, 세션상한은 반드시 더 커야함, 워커512=전체연결512 단정도
  실제 서로다른예산을 대조할 것. player_conn.cpp5초협조적마감, Matchmaker kMaxWaiting1024,
  relay512worker·64KiB버퍼/시간, Reactor max-conns와 단계별lease수명을 함께읽기.
  아직107 원고/체크포인트 없음. 총량·단계별·per-IP·시간예산을 작은누적정책으로 설계할 것.
  105부터 남은 worker부분시작/카운터완료 예외경로는 별도미검토이며 완전보안완료로 주장하지 않는다.

## 직전 완료 기록 — 105차시

- 2026-09-29: 직전104차시는 progress. **105차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~105차시·519문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **106차시 “동시 나가기: 상태를 지우는 순서”** (unit-106).
- 105.json15절·5문제·인라인16(C++15/CMake1)·접힌 전체파일3.
  105-forwarder는 README/DESIGN/CMake 외 104 누적 불변 파일 바이트 보존.
  전이중/방향별 parser·pending·offset, 로컬 송신 수락/상대 수신, backpressure,
  EOF·부분 프레임·미전달 프레임, 정지 요청/최종 소유자의 정리를 재구성했다.
- OpenCode DeepSeek105-forward exit0, step_start1/text1, 도구 호출 없음.
  /tmp cwd/tools deny로 승인한 학습 FrameParser/SendAttempt와 한정 명세만 전송.
  초안의 복사금지=단일접근 보장, 수락=ack 표현, feed 거절/상한 실패 구별을 교정했다.
- 기준 ForwardDirection: 단일 소유자, copy/move 금지, prefix 원본 초기화/먼저 prime,
  한 프레임 pending 동안 추가 수신 금지. flush당 noexcept 비재진입 송신 콜백 최대1회.
  count/error 검증 뒤 offset 전진, would_block/interrupted 보존, 부분 수락 후 오류에 재전송 없음.
  완성 후 다음 프레임을 prime하되 다음 flush에서 송신. 오류 후 재사용/재동기화 없음.
  EOF는 빈 상태/source_closed·부분꼬리/truncated·미전달완성/undelivered로 구별.
  타입 의미/인증/서버전용타입 필터는 기준 구현 밖이며 서비스의 신뢰 경계와 구별.
- 실제 TCP probe: 방 조회·두 수락 후 prefix로 A→B/B→A 시작, 최대3바이트 수신과
  최대2바이트 송신. 실제 반대 클라이언트가 TYPE41 입력을 받고 그 입력으로 RoundPlay
  양쪽 정규 상태 바이트 일치. 이후 A 닫기→서버 EOF→B 방향 stop/양소켓정리→B EOF.
  한 호출자가 두 방향에 진행 기회를 주며2초 협조적 전체 실험 마감 적용.
  HDAAA·round1·seed77·역할은 fixture. 이 실습을 auth/MATCH_FOUND/반닫힘 프록시로 과장하지 않음.
- 실제 두 relay의 과대 길이 처리: 버퍼만 비우고 다음 recv를 새 경계로 간주하던 경로를
  연결 종료로 보강. threaded는 소스 disconnectSide 기록, Reactor는 protocol_error와
  send_failed를 별도 결과로 반환해 종료 대상을 구별. ranked는 검증 상태도 invalidate.
  이미 상대에 수락된 바이트는 회수하지 않으며 오류 배치의 원자적 전달을 주장하지 않는다.
- Python _recv_frame: parse_frames 전체 목록에서 첫 목표 프레임만 반환해 뒤쪽 완성
  프레임을 잃던 문제 재현·수정. 한 프레임씩 소비하고 목표 뒤의 완성/부분 바이트 보존,
  큰 길이는 공통 parser의 오류/clear 계약 유지, recv마다 남은 시간 예산 사용.
  정상 prefix 통합의 CHAT도 text_len+본문 실제 규약으로 보강했다.
- 수정 전: threaded helper tail 손실+과대 헤더 연결 유지2실패, Reactor 과대 헤더1실패.
  수정 후 두 실제 relay 각각 helper/과대헤더/수락꼬리/정상방/종료5passed·0skip.
  마지막 CHAT 본문 보강 후 해당 통합만 두 relay 각각1passed·0skip 재확인.
  무인증 loopback 임시 포트/프로세스 범위. ranked invalidate는 소스/빌드 대조,
  인증·DB를 붙인 ranked 오류 통합으로 표현하지 않는다.
- Part7§10 현재 forwarder 발췌와 경계 오류/수신 억제/송신 위치/종료 정책을 갱신.
  단절=몰수패/벌점, 고정1초 창=임의1초 상한, idle15초/슬립1ms 실시간 보장 단정 교정.
  Part7§13의 두 바이너리 모든 제한 공유·keepalive보다 항상 빠른 감지 단정 교정.
  Part14§8의 역사적 몰수패/모든 return 중복 설명을 현재 공통 결과 확정/최종 소유자로 교정.
  Reactor on_queued의 사본 파싱은 실제 소스와 일치함을 확인했고 변경하지 않았다.
- 검증:6006본문크기×모든 지정 분할×송신cap 조합, would_block/EINTR/오류·invalid결과,
  prefix/상한/EOF/미전달/정지 후 불변 ASan·UBSan. 실제 TCP probe도 sanitizer 통과.
  새2타깃 SCRIPTED2/SDL2 CTest, root 기존 두 구성 빌드 및27/26 CTest 통과.
  변경 없는 과거 checkpoint 전체 타깃 재검사 없음. 인라인16·Part/링크·Markdown27·
  519문제·전체 DOM/완료저장/이전위치/단일차시/file URL 계약 통과.
  DOM 뒤 최종 Python fixture만 규약에 맞춰 수정해 해당 통합과 library/정적 해시 검사를 재실행.
  Windows/macOS 네이티브·수동GUI·WAN/부하·모든 시작/종료 스케줄·ranked 인증/DB 통합 미검증.
- coverage344: covered3/partial124/unassigned217/needs-review0. 문서28/소스196.
  Part7 H2 두 개·Part14 H2 한 개의 근거만 갱신. 전체 동시성/운영 대응 완료로 처리하지 않음.
  최종 정적 release **25e0c5d8a7b09163**,15파일 허용목록/상대경로/해시/재현 ZIP 통과.
  HTTP18767 일치: index24870 / lessons7137806 / library5469200 / app14480 / usability7039바이트.
  out/learning-jobs/105-{checkpoint,final-check,root-regression,threaded-before,reactor-before,
  threaded-after,reactor-after,threaded-prefix-final,reactor-prefix-final,part,dom,site,http}.log.
  모든 모델/검사 작업 종료, HTTP18767 유지. 하위 에이전트·수동GUI·기록이동·커밋/푸시/배포 없음.
- 다음106 읽은 지점: server/room.cpp roomLoop_ 진입/일반 종료(480 부근)는 code/isHost로
  엔트리를 찾아 준비·presence·lease를 바꾸고, mutex 밖에서 roomInfoVersion으로 알림 검증.
  abortRoom_는103에서 소켓 소유 동일성을 확인했으나 일반 reader/퇴장 경로의 재사용·
  오래된 작업 구별은 아직 검토하지 않았다. 같은 코드 재사용과 동시 퇴장을 재현해 판정할 것.
  server/relay.cpp의 counter초기2/두 worker 부분 시작 실패/외부 close/마지막 정리도 별도
  범위로 대조할 후보이며, 현재 leak/중복 판정을 단정하지 않는다.
  아직106 원고/체크포인트 없음. 삭제 결정·알림 snapshot·오래된 cleanup 식별을 설계한다.
- 검증 효율: 누적 불변 파일 바이트 확인 후 변경 타깃/필요 의존성만 검사한다.
  새 실패·공통 변경·미해결 우려 없이 이전 전체 빌드/검사를 반복하지 않는다.

## 직전 완료 기록 — 104차시

- 2026-09-29: 직전103차시는 progress. **104차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~104차시·514문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **105차시 “포워더: 방향별 상태와 종료”** (unit-105).
- 104.json15절·5문제·인라인18(C++17/CMake1)·접힌 전체파일3.
  104-acceptance-lobby는 README/DESIGN/CMake 외 103 누적 파일 바이트 보존.
  선택한 두 연결에서 수락 상태·단조 시각/절대 마감·본문 문법·분할 수신·소유권 인계로
  이어지며, std::move 캐스트와 고정 배열 원본 초기화의 차이까지 설명한다.
- OpenCode DeepSeek104-lobby 정상 종료, step_start1/text1이며 도구 호출 없음.
  /tmp cwd/tools deny로 승인된 작은 명세와 학습 FrameParser만 전송.
  초안의 parser만 인계한다는 설명을 socket과 함께 인계로 교정하고,
  LobbyHandoff 이동 후 원본 초기화·이전 단계 parser prefix 생성자를 보강했다.
- 기준 AcceptanceLobby: 한 소유자가 두 parser/ready 플래그 관리, copy/move 금지.
  READY exact1/0·1, CANCEL exact0. invalid_side/역행은 미변경 오류 반환.
  poll→append→한 제어 프레임 순서, now-start>=timeout에서 마감 우선, 활동 연장 없음.
  ready 쪽의 후속 바이트는 파싱 없이 보존하되 waiting EOF/상한은 검사.
  accepted 뒤 송신 실패는 close, 정상 통지 뒤 take 한 번, handed_off 이후는 다음 소유자 책임.
  준비 후 CANCEL은 게임 단계의 꼬리이며 로비 취소 재지원으로 설명하지 않는다.
  초기 prefix 안의 완성 READY/부분 헤더도 생성자와 빈 feed로 이어 처리한다.
- 실제 TCP probe: 코드 조회로 얻은 두 연결에 READY+TYPE41 입력을 연속 송신,
  최대3바이트 수신, 두 반대편의 READY 알림 확인, parser 인계 뒤 실제 RoundPlay
  정규 전체 상태 바이트 일치. round1/seed77/역할은 fixture 공급, auth/MATCH_FOUND 미구현.
  작은 loopback 송신 실패는 실습 실패이며 완성된 비동기 송신 스케줄러로 과장하지 않는다.
- 실제 server/relay.cpp 및 reactor_relay.cpp의 queue lobby: 체크섬 이후 READY 정확길이와
  값0/1, QUEUE_CANCEL 빈 본문을 확인하고 상태/상대 알림 이전에 거절.
  room.cpp 및 Reactor on_room도 READY 정확길이/값 검증 후 준비 플래그 변경.
  방에서0은 준비 해제, 랜덤 수락에서0은 매치 거절이라는 단계 차이를 유지.
- python/tests/test_relay_meta_smoke.py: malformed queue control5조건, malformed room READY4조건,
  READY+CHAT 동시 send의 꼬리 보존 통합 추가. 프레임 배치 전체를 모아 알림/게임 전달 확인.
  수정 전 threaded READY2/255/추가본문 세 조건 실패 재현(104-root-before.log).
  수정 후 두 실제 서버 각각 malformed9+정상방/종료2=11passed·0skip,
  별도 동시 프레임 보존1passed·0skip. 무인증 loopback/프로세스별 임시 포트 범위.
- Part7§8/§10 현재 발췌·READY 규약 최신화. 전체 파싱 자체가 손실이라는 단정,
  EOF 때 항상 알림 성공, 정확한64KiB/30초 보장으로 읽힐 설명 교정.
  실제 수신/송신 관측의 범위, 현재 unknown-before-ready 대기와 기준 즉시 거절의 차이 명시.
- 검증: 기준 READY값256개, 지정 스트림의 모든 분할 위치·양쪽 순서,
  deadline equality/큰 uint64/역행/EOF/상한/인계/이전 prefix 계약 ASan·UBSan 통과.
  실제 TCP 실습 ASan·UBSan, 새2타깃 SCRIPTED2/SDL2 CTest와 경고 없는 빌드.
  root 기존 두 구성 빌드와27/26 CTest 통과(각 Testing/Temporary/LastTest.log).
  이전 변경 없는 전체 체크포인트 타깃 재검사 없음. 인라인18 일치, Part 구조/발췌/링크,
  Markdown27, 514문제, 전체 DOM·완료저장/이전위치/단일차시/file URL 계약 통과.
  Windows/macOS 네이티브·수동GUI·WAN/부하·모든 동시 종료·인증/DB 통합 미검증.
- coverage344: covered3/partial124/unassigned217/needs-review0. 문서28/소스196.
  Part7 H2 두 개만 근거 갱신. 포워더 전체·방 동시 나가기 대응 완료로 처리하지 않음.
  정적 release **5b8847462e2250b6**,15파일 상대경로/허용목록/해시/재현 ZIP 통과.
  HTTP18767 일치: index24870 / lessons7069921 / library5463396 / app14480 / usability7039바이트.
  out/learning-jobs/104-{final-check,threaded-after,reactor-after,threaded-prefix,reactor-prefix,
  part,dom,site,http}.log. 모든 모델/검사 작업 종료, HTTP18767 유지.
  Codex 하위 에이전트·수동GUI·기록이동·커밋/푸시/배포 없음.
- 다음105 읽은 지점: server/relay.cpp forwarderLoop(212), 방향별 raw/streamBuf/prefix,
  ForwarderCompletion(220)의 종료 플래그·최종 소유자 정리, startForwardingWithPrefix(558)의
  두 worker 시작 및 부분 실패. 아직 결함으로 단정하지 않고 실패 주입/계약부터 대조할 것.
  추가 검토 후보: test_relay_meta_smoke.py _recv_frame은 parse_frames가 완성 프레임 전부를
  소비한 뒤 첫 want에서 반환하므로 같은 배치의 뒤쪽 프레임을 잃을 수 있다. 현재 주석의
  보존 약속과 대조해 재현 후 보강할 것. 104 신규 동시 프레임 검사는 별도 seen 목록으로
  모든 반환 프레임을 보존하므로 해당 helper에 의존하지 않는다.
  아직105 원고/체크포인트 없음. 판정/메모리/전송 소유 범위를 작은 누적 구현으로 설계한다.
- 검증 효율: 누적 불변 파일 바이트 확인 후 변경 타깃/필요 의존성만 검사하며,
  공통 변경·새 실패·미해결 우려 없이 이전 전체 빌드/검사를 반복하지 않는다.

## 직전 완료 기록 — 103차시

- 2026-09-29: 직전102차시는 progress. **103차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~103차시·509문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **104차시 “수락 로비: 두 사람이 준비되는 순간”** (unit-104).
- 103.json17절·5문제·인라인16(C++15/CMake1)·접힌 전체파일3.
  103-room-code는 허용한 README/DESIGN/CMake 변경 외 102 누적 파일 바이트 보존.
  25비트 코드 공간, 한 번의 충돌 확률과 생일 문제, 실패 보존, 소유권과 세대 번호,
  검색·예약 직렬화, 예외 중 자원 정리, 소켓 인계까지 구현 과정으로 연결한다.
- OpenCode DeepSeek103-directory 작업 exit0, step_start1/text1, 도구 호출 없음.
  /tmp cwd/tools deny로 승인한 명세와 선택한 생성자/generateCode 발췌만 전송.
  초안의 핸들=권한 오해, payload 소유자, 잠금 범위, 콜백/소멸 제약을 교정했다.
  find는 메타데이터 복사이며 기존 shared_ptr 별칭의 변경 가능성은 별도 계약으로 설명.
- 기준 RoomDirectory<T,Capacity>: 고정 sparse optional 슬롯, noexcept 이동/소멸,
  owner/code/generation 모두 일치해야 take, 실패 입력 보존·세대 미소비·최대32 후보.
  source 실패는 즉시 반환하고 대체 난수 없음. 내부 핸들은 인증 비밀이 아니다.
  find 스냅샷 이후 take 재검증, close는 대기 엔트리만 정리하고 사용자 drain은 별도.
  16384 참조 모델 연산, 64 두 스레드 동일 코드 경합, 160 자리/문자 검사,
  세대 소진·이동 대입 없는 payload·모든 실패 우선순위를 확인했다.
- 실제 TCP probe: host TYPE50 create → 예약 HDAAA → TYPE52 공개 → guest join → take.
  directory close 뒤에도 반환된 host 소켓 생존. TYPE41 입력으로 실제 RoundPlay 양쪽의
  정규 전체 상태 바이트 일치. round1/seed77/역할은 fixture 공급이며 READY 전체 구현은 아니다.
- 실제 server/room_code.h/.cpp: 두 relay가 같은 코드 선택/OS 난수 경로 사용.
  Linux getrandom(GRND_NONBLOCK), Windows BCryptGenRandom, 다른 POSIX /dev/urandom.
  정확히4바이트/성공 상태만 수용하고 짧은 읽기·EAGAIN·EINTR은 방 생성 실패로 처리.
  xorshift 코드 생성 제거, Windows bcrypt 연결. 경기 seed/UUID RNG 변경으로 확대하지 않음.
  수정 전 알고리즘의 숨은 draw 없는 fixture에서 7개 코드 관측의 GF(2) rank64로
  상태를 복원해 8번째 코드를 예측했다. 모든 네트워크 관측 조건에 대한 증명은 아니다.
- 실제 RoomRegistry: shutdown과 create/join 접수를 같은 mu 아래 재검사.
  Entry 할당 필드를 먼저 준비하고 noexcept 이동으로 게시, 게시 후 예외도 owner의
  fdh 및 transport 소유 동일성을 확인해 abortRoom_에서 소켓/방/lease 정리.
  guest 게시 후 roomInfo 버전이 바뀌어 읽기 루프를 시작하지 못한 경로도 정리한다.
  원시 fd나 코드 일치만으로 다른 연결의 재사용 방을 삭제하지 않는다.
- root 회귀: shutdown gate 전후, 실제 할당 실패 여러 지점, guest 게시 후 버전 변경의
  읽기 루프 포기 경로 모두 수정 전 실패/수정 후 통과. 시험용 복사본에만 관찰/스케줄 훅.
  shutdown/guest 포기/공통 난수 계약 ASan·UBSan 통과. 전역 new 실패 주입은 sanitizer 없이 검사.
  모든 방 수명 경로·모든 할당 실패를 검증했다고 확대하지 않는다.
- Part7§3/8/13, Part13§3, Part14§8 관련 현재 발췌·설명 갱신.
  25비트는 강한 인증 비밀이 아님, 후보 선택과 예약 구별, UUID 충돌 단정 교정.
  Microsoft BCryptGenRandom 및 Linux getrandom 공식 문서와 계약 대조·본문 링크.
  실수로 변경된 무관한 GAME_COMMON 발췌는 원상 복원하고 Part 검사 통과.
- 검증: 새 checkpoint2타깃 SCRIPTED2/SDL2 CTest 및 ASan·UBSan, 인라인16 일치.
  최종 root 기존 두 구성27/26 CTest 통과. 두 실제 relay 각각 무인증 방 생성/입장/
  READY/역할1·2/동일 seed loopback pytest1passed·0skip. 기존 .venv 사용.
  Part 구조/발췌/링크·Markdown27·509문제·전체 DOM 계약 통과.
  DOM에는 완료 ✓/localStorage 복원, 현재 위치와 구별한 이전 읽던 위치, 단일차시 렌더링,
  file URL/저장 실패 포함. Windows/macOS 네이티브·수동GUI·WAN/부하·인증/DB 통합 미검증.
- coverage344: covered3/partial123/unassigned218/needs-review0. 문서28/소스196.
  근거 갱신은 Part7 H2 세 개, Part13·14 각 한 개로 한정. 전체 대응 완료로 처리하지 않음.
  정적 release **55ab1893d42e0c12**,15파일 허용목록/상대경로/해시/재현 ZIP 통과.
  HTTP18767 일치: index24870 / lessons7003638 / library5455981 / app14480 / usability7039바이트.
  로그 out/learning-jobs/103-{final-root-contract,final-build,dom,part,site,http}.log 및
  두 relay final-integration 로그. 모든 모델/검사 작업 종료, HTTP18767만 유지.
  Codex 하위 에이전트·수동GUI·기록이동·커밋/푸시/배포 없음.
- 다음104 읽은 지점: server/relay.cpp queueLobbyThread(601)의 30초 마감, READY와
  QUEUE_CANCEL 소비, 준비 뒤 원시 바이트 상한/forwarder 인계. server/room.cpp의
  guest 입장 후60초, 두 참가자 준비, starter 선점과 상대 읽기 루프 종료 확인.
  hostExited/guestExited는 matchStarted 관측 뒤에만 설정하는 현재 분기를 확인했으나
  재사용/취소 경로 전체는 아직 검토하지 않았다. 버그로 단정하지 말고 계약과 재현부터 진행.
  READY 정확 길이/값과 준비 뒤 취소·버퍼 인계의 정책도 실제 클라이언트/forwarder와 대조할 것.
  아직104 원고/체크포인트 없음. 새 차시의 작은 상태 기계 초안은 DeepSeek에 맡기고 직접 검수.
- 검증 효율: 누적 불변 파일의 바이트 확인 후 변경 타깃/필요 의존성만 검사한다.
  공통 변경·새 실패·미해결 우려 없이 이전 전체 빌드/검사를 반복하지 않는다.

## 직전 완료 기록 — 102차시

- 2026-09-29: 직전101차시는 progress. **102차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~102차시·504문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **103차시 “방 코드: 생성·충돌·소유권”** (unit-103).
- 102.json16절·5문제·인라인17(C++16/CMake1)·접힌 전체파일3.
  102-match-queue는101 누적 코드 바이트 보존. 고정 optional 슬롯·실패 입력 보존·
  FIFO 등록 순서·취소/선택 직렬화·단독 대기 폴링·큐 close와 사용자 drain을 설명.
  실제 TCP3연결 중 buffered cancel2를 제거하고 Pair1,3으로 게임 입력을 처리한다.
- OpenCode DeepSeek102-queue 및102-format 작업 모두 exit0, step_start/text만 존재.
  승인한 선택 root C++/명세와 새 학습 C++2파일만 /tmp cwd/tools deny로 전송.
  초안의 enum class·mutex 대기 가능성·ID 재사용 책임·고정 슬롯의 메모리 범위 교정.
  새 contract/probe2파일은 토큰·주석 동일성을 확인해 줄바꿈 정리, 인라인6개 재발췌.
  별도 C++ 문법 검사 통과. Codex 하위 에이전트·수동GUI·기록이동·커밋/푸시/배포 없음.
- 실제 Matchmaker: stopping/상한 검사를 enqueue와 shutdown의 같은 mu 아래 수행.
  성공 bool 반환, 종료중/대기1024상한 거절은 연결 종료와 소유 lease 해제.
  player_conn은 성공 때만 queued 로그. 단독 대기에도 취소/EOF를 재검사하며,
  빈 큐 predicate wait·단독50ms wait_for·모든 pending 정리 후 생존자 앞의 두 명 인계.
  50ms는 협조적 재검사 간격이며 응답 상한이 아니다. 큐의 mutex 안 파싱/삭제 비용 명시.
- tests/learning/matchmaker_queue.cpp: 취소 중간 항목/FIFO, 종료 후 등록, 단독취소,
  1024 대기 상한 회귀. CMake matchmaker_queue/closed/lone_cancel/full4개 테스트 추가.
  수정 전 root의 late lease.expired 및 lone removed_without_second_peer 실패를 재현했고
  수정 후 통과. 실제 tetris_relay의 단독취소 EOF와 후속 두 참가자 매칭, 활성 경기
  SIGTERM pytest2passed/0skip. 프로젝트 .venv 사용, 추가 설치 없음.
- 기준 PairQueue<T,Capacity>: 이동/소멸 noexcept 제한, 닫힘/id0/중복/full 순서 검사,
  실패 T 보존·cancel 반환 소유권·이동 후 슬롯 당기기·콜백 기반 생존 검사.
  Poll과 T 소멸은 mutex 안에서 비블로킹/작업량 제한/재진입 금지. try_pair도 mu 대기 가능.
  배열 삭제 O(N), 여러 삭제 최악 O(N²). ID는 호출자가 오래된 요청과 겹치게 재사용하지 않음.
  close 후 모든 생산자/소비자 drain 전에 큐를 파괴하지 않는 계약.
- probe는 실제 TCP3연결을 cap3 수신으로 fixture가 보낸 길이만큼 모아 취소가 parser에
  남는 조건을 확정한다. 매칭 완료 후 TYPE41 입력 송신·round1/seed77/역할 fixture 공급.
  실제 RoundPlay 양쪽의 정규 전체 상태 바이트 비교, 큐 close 후 Pair 소켓 생존 확인.
  인증·MATCH_FOUND·READY 전체 프로토콜 실험으로 표현하지 않는다.
- Part7§6 반환 처리·§7 현재 header/cpp 발췌와 취소/완성프레임 소비·부분꼬리 보존/
  FIFO 정의·상한·종료·deque 참조 무효화·논블로킹 비용·UUID와 저장 멱등 조건을 교정.
  Part13§3 CMake 회귀 타깃 발췌 최신화. 101 현재 소스 링크의 void enqueue를 bool로 갱신.
  C++ deque.modifiers 원문 계약을 근거로 기존 원소 참조와 반복자 무효화를 구별.
- 검증: 새2타깃만 SCRIPTED2/SDL2 CTest·경고 없는 빌드. 변경 없는 과거 전체 타깃 재빌드 없음.
  기준32768 모델 연산·128 취소/선택 경합·단독 폴링·실제 TCP probe와 root4모드 ASan/UBSan.
  실제 root 두 기존 구성26/25 CTest. 인라인17 일치·Part/링크·Markdown27·504문제·DOM 계약 통과.
  DOM에는 완료 ✓/localStorage 복원, 이전 읽던 위치, file URL/저장 실패, 단일차시 렌더링 포함.
  Windows/macOS 네이티브·수동GUI·WAN/부하·모든 스케줄·인증/DB 전체 종료 통합 미검증.
- coverage344: covered3/partial120/unassigned221/needs-review0. 문서28/소스193.
  검토한 Part7 두 H2·Part13 한 H2만 근거 갱신. 전체 원문 대응 완료로 처리하지 않음.
  정적 release **26439f9e9331fa84**,15파일 허용목록/상대경로/해시/재현 ZIP 통과.
  HTTP18767 일치: lessons.js6930996 / library.js5438237, index/app/usability도 확인.
  로그 out/learning-jobs/102-{check,integration,snippets,format,dom,part,site,http}.
  모든 모델/검사 작업 종료. HTTP18767 제공 유지.
- 다음103 시작점: server/room.h·room.cpp generateCode_/handleCreate.
  32문자×5자리 코드와32회 충돌 재시도, mu 아래 rooms 조회/삽입을 대조할 것.
  constructor의 random_device+xorshift 예측 차단 단정, handleCreate의 mu 밖 stopping 검사와
  shutdown 사이 경합은 검토 후보이며 재현/계약 확인 후 교정할 것. 아직103 산출물 없음.
- 검증 효율: 누적 불변 파일 바이트 확인 후 변경 타깃/필요 의존성만 검사한다.
  공통 변경·새 실패·미해결 우려 없이 이전 전체 빌드/검사를 반복하지 않는다.

## 직전 완료 기록 — 101차시

- 2026-09-29: 직전100차시는 progress. **101차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~101차시·499문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **102차시 “매칭 큐: 등록과 취소를 함께 설계”** (unit-102).
- 101.json15절·5문제·인라인13(C++12/CMake1)·접힌 전체파일3.
  101-worker-lifetime은100 누적 코드 보존. 예약/상한·생성실패·본문예외·
  캡처소멸·predicate wait·notify수명·detach/join·접수중단/깨우기/drain을 설명.
  Runtime/results/group 선언 역순과 완료 이후 결과 읽기, TLS와업무정리 범위를 구별.
- OpenCode DeepSeek101-worker-material/events/draft 정상종료.
  승인한 worker_group.h와한정명세만 /tmp cwd/tools deny로전송.
  초안의무제한default삭제·중복주석축약·캡처정리와OS종료범위 명확화.
  Codex하위에이전트·기록이동·수동GUI·커밋/푸시/배포 없음.HTTP18767유지.
- 실제 server/worker_group.h: work 직접캡처소멸보다 Completion이먼저 active를
  감소하던오류를 재현후수정. unique_ptr callable을Completion 뒤의local로이동해
  callable/captures소멸후완료통지. 실패로그도finish앞으로이동.
  이름있는std::thread로 생성/ detach오류경로분리. detach실패는이미시작한작업이므로
  finish를다시부르지않고join폴백/true반환(launch블로킹가능).
  join·mutex실패/소멸자예외는fail-fast경계. TLS/OS스레드완전종료는drain범위밖.
- tests/learning/worker_lifetime.cpp: capture소멸자를gate에멈춘상태에서
  limit1의추가접수거절을검사. 수정전실제root는해당assert실패, 수정후통과.
  callable move생성실패후예약복원, int/runtime_error본문예외, move-only,
  wait후재사용/닫힌접수/limit0,12생산자가2blocked slots에경합검사.
  CMake worker_lifetime_test/CTest worker_lifetime(TIMEOUT15) 정식추가.
  matchmaker.h의잘못된단일producer주석을여러연결worker로교정.
- 기준 study_net::WorkerGroup: limit명시·copy/move삭제·count실패포화·단일mutex.
  static run은Completion 다음local owned를선언. counter예약전/정리후반납.
  failed_tasks는본문예외만집계. stop_accepting은작업취소안함.
  worker_probe: real loopback3연결/queue-create-join/TYPE41/각RoundPlaytick0.
  각worker별소켓·parser·보드/결과칸,main은wait후결과읽음.
  실제매칭/auth/READY는아니며초기설정은fixture가공급.
- Part7§4·§5: 현재header발췌/RAII정리범위/capture소멸/생성과detach실패/
  notify잠금과수명/접수중단·깨우기·drain/TLS한계/종료보장단정교정.
  Part12§8.4: 현재launch발췌와정리범위, SIGTERM시무조건즉시UAF단정교정.
  Part13§3.7: 새root회귀타깃포함CMake발췌갱신.
  C++작업초안 thread.thread.member/destr/condition_variable 확인·본문링크.
- 검증: SCRIPTED131/SDL136 CTest·경고없음·100누적보존.
  기준/root수명계약·실제3TCP게임입력 ASan/UBSan통과.
  root기존두구성빌드및22/21 CTest통과. 새수명회귀포함.
  실제 tetris_relay의두참가자MATCH_FOUND/READY후SIGTERM정상종료pytest1passed/0skip.
  기본python3에pytest가없어기존 .venv 사용(추가설치없음).
  인라인13일치·Part발췌/링크·Markdown27·499문제·전체DOM계약통과.
  OSthread생성/detach/join실패강제주입·TLS후처리·타OS네이티브·수동GUI·WAN/부하·
  인증/DB전체종료통합은미검증. loopback무인증SIGTERM을그범위로과장하지않음.
- coverage344: covered3/partial119/unassigned222/needs-review0.문서28/소스192.
  검토한Part7두H2·Part12한H2·Part13한H2만근거갱신.
  정적release **ab21643d25f5af37**,15파일 허용목록/상대경로/해시/재현ZIP통과.
  HTTP18767일치 lessons.js6863536 / library.js5427748.
  logs101-root-before/root-after/check/part/dom/shutdown/site/http. 모든검사/모델작업종료,HTTP만유지.
- 다음102 읽은지점: server/matchmaker.cpp enqueue는mu아래push하되stopping검사없음.
  shutdown뒤이미진행중인입장worker의enqueue 가능성을 재현/교정할후보.
  waitForPair는waiting>=2일때만취소폴링, waitingPlayerStillActive는QUEUE_CANCEL외
  완성프레임을버리므로미소비바이트인계와각단계허용정책도명시적으로대조할것.
  아직102원고/체크포인트없음. 발견사항은후속검토후검증하여반영.
- 검증효율: 다음차시부터복사한누적파일은바이트동일성을확인하고,변경된타깃과
  필요한의존성을두backend에서빌드/검사한다. 공통코드변경·실패·미해결우려없이
  동일한이전전체타깃을매번다시빌드/테스트하지않는다. 수행범위만정확히기록한다.

## 직전 완료 기록 — 101차시 이전

- 2026-09-29: **100차시 집필·현재 코드/원문 교정·검수 완료(progress)**.
  로컬 **1~100차시·494문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **101차시 “워커 수명: detach·예외·종료 대기”** (unit-101).
- 100.json15절·5문제·인라인14(C++13/CMake1)·접힌 전체파일3.
  100-first-admission은99 누적 코드 보존. 첫 요청·구문·고정 마감·독립 예산·
  recv/단계 경계·parser와socket 공동 인계·EOF·실제 RoundPlay 첫 틱을 설명한다.
- OpenCode DeepSeek100-admission-material/events/draft 정상 종료.
  승인한 한정 명세와 framing 발췌만 /tmp cwd/tools deny로 전송.
  초안 eof의 clock_error 무시/상태변경 수정, 크기 검사 선행, 무효 기본 request getter 제거.
  새 Codex하위에이전트·수동GUI·기록 이동·커밋/푸시/배포 없음. HTTP18767 유지.
- 기준TYPE50 exact queue/create2+len0 또는 join7+len5 ASCII A-Z0-9.
  candidate commit/실패출력보존. 읽기16/단계128B/낯선4개 허용.
  협력적 고정 경과시간, 정확 마감 우선, 역행시각 transient/no mutation.
  첫 요청에서 drain 중단, take가parser 꼬리와요청을 한번 인계.
  다음소유자는parser부터소비하고socket을이어읽음. 상태객체 복사로 소유권을 분기하지 않는 caller계약.
- 실TCP loopback의admission_probe: queue+TYPE60+TYPE61부분prefix, socket/parser인계,
  suffix+실제TYPE41입력, RoundPlay round1/seed77 peer가hostdrop+local0으로tick0진행.
  드라이버가 초기조건을 공급하며 auth/실제매칭/READY를 구현한 것으로 표현하지 않음.
  tiny blockingclientfixture, nonblockingserver입장, steady_clock, CTest15초 외부제한.
- root player_conn.cpp와room.cpp에서 parse_frames의false를 무시하던 두 분기 수정.
  first는close/return, room은break로공통정리. 정상prefix+과대길이에서
  분기실행이 계속되던 이전형태 재현/수정후차단, 정상/부분입력과residual순서보존.
  tests/learning/first_parse.cpp는 실제분기 발췌검사이며 서버전체통합검사는 아님.
  기존체크섬skip정책은 유지. player_conn.h의역할·unknown정책·game-ticket경로 교정.
- Part7§6: worker진입시5초/협력적검사/HTTP인증취소안함,
  tcp_recv_some EOF또는오류, reserve예약≠상한, canonical프레임+tail인계,
  ROOM_JOIN실제1~5범위/문자미검사, unknown무시의범위와일반화교정.
  §8 실제room파서실패guard발췌갱신. 검토한 두 H2만partial근거추가.
- 검증: SCRIPTED129/SDL134 CTest·경고없음·99누적보존.
  decoder TYPE256×size40×route256, join각문자/길이,13B모든4096분할,
  deadline/u64/역행poll·feed·eof/예산/EOF/출력보존 ASan/UBSan통과.
  추가한128B정확성공·unknown이마감연장안함·knownmalformed·다음단계badheader도통과.
  최종admission_contract타깃 재빌드/CTest통과. root게임/양relay빌드및21/20CTest통과.
  인라인14일치·Part구조/발췌/링크·Markdown27·494문제·전체DOM계약통과.
  타OS네이티브/수동GUI/운영서버auth매칭통합/WAN/부하미검증.
- coverage344: covered3/partial116/unassigned225/needs-review0. 문서28/소스191.
  정적release **d8e9fb1b4cc7a650**,15파일 허용목록·상대경로·해시·재현ZIP통과.
  HTTP18767최종바이트일치 lessons.js6813812 / library.js5416128.
  logs100-check/final-contract/boundary/part/dom/site/http. 모델·검사작업종료,HTTP만유지.
  check_learning_first_admission.py는수정전무시분기를현재분기에서재구성해임시백업의존없음.
- 다음101은 WorkerGroup의detach·예외·생성실패·shutdown/wait수명을 작은누적구현으로 연결.
  matchmaker.h의 “단일 생산자” 주석 등 호출자 수·동시성 단정은 해당범위에서 대조할 후보.
  100의시각/파서 계약이나공개55차시동결을 초기화하지 않는다.

## 직전 완료 기록 — 99차시

- 2026-09-29: 직전98차시는 progress. **99차시 집필·원문 교정·검수 완료(progress)**.
  로컬 **1~99차시·489문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **100차시 “accept와 첫 프레임: 연결의 역할 결정”** (`unit-100`).
  `99-relay-choice` 누적. 100.json/100체크포인트는 아직 없다.
- 계약 유지: 단순 구현 초안은 OpenCode DeepSeek, 선별 명세만 전송.
  Codex 하위에이전트·기록 이동·수동GUI·커밋/푸시/배포 없음. HTTP18767 유지.
- 99차시15절·5문제·인라인11(C++10/CMake1)·접힌 전체파일3.
  제어/데이터 경로, listen과 외부 접근 가능성, NAT/STUN/TURN 역할,
  연결과 socket 끝점, relay 전달과 규칙 권위, 방향 지연/RTT비대칭,
  HOL/lockstep 누락 입력, 바이트율/bit율/입출력·큐예산·RSS 구별,
  checked산술·0인자·candidatecommit 및 relay/meta 상태수명 설명.
- DeepSeek 099-cost-material.txt/events.jsonl/draft.json 정상 종료.
  자료는 비용 모델 한정 명세뿐, /tmp cwd/tools deny, 키·설정·저장소전송 없음.
  net/relay_cost.h 반환을 검토해 std::uint64_t, 네 성분(네 홉 아님),
  TCP/IP/link overhead 제외 표현으로 교정. 예측 수치는 실측값이 아님.
- 기준 RelayDirection: 두 구간+relay_wait+relay_work(us), checked 합산.
  RelayTrafficPlan: matches/frame_bytes/fps/queued_bytes_per_connection.
  connections=2N, ingress=egress=2NBF, queue_capacity=2NQ.
  표현 범위 초과는 거절/출력 전체 보존. 트래픽0인자를 먼저 처리하지만 socket/queue는 별도 검증.
  100경기/B18/F60/Q4096 => sockets200, ingress/egress각216000B/s, queue819200B.
  방향28ms/29ms 모델 왕복57ms; 실제 TCP RTT 측정 아님.
- topology_probe: Runtime+Socket/stream/Frame/RoundPlay 기존 계층 재사용.
  직접 TCP1개(A accept/B connect), relay TCP2개(양쪽 connect, R의ra/rb accept).
  하나의 제어된 driver가 작은 프레임을 동기 send/read하여 진행하며 CTest20초 외부제한.
  임의 클라이언트용 일반 포워딩 서버 아님. relay는 fixture가 아는 길이만 복사하며
  TYPE/round/input을 해석하지 않음. endpoint는 실제 FrameParser/RoundPlay 사용.
  seed77,host drop/peer0,3틱의 양쪽 정규 상태 바이트 비교와 경로간 최종상태 비교.
  미래round2 프레임도 R을 통과하지만 endpoint가 future_round로 거절하고 상태 보존.
  프레임18B*정상6개=relay각108B, 미래회차1개추가=>각126B. direct relay카운터0.
  read cap1/2/7/16 검증. 직접/중계 함수 호출을 별도 문장으로 고정해 출력순서 명확화.
- 실제 게임/서버 실행 코드는 이번에 변경하지 않음.
  Part7 §1: P2P와매칭혼동·NAT단정·STUN/TURN UDP전용·UDP틱건너뛰기·
  고정최초RTO200ms·RTT/홉수단정·동시상시worker3개·롤백/중복전송이
  결정론규칙변경필수라는 설명 교정. 비용 단위와 공개WSS/내부TCP 경계 보강.
  §2 shared_ptr<int>를 현재 NativeSocket으로 정정.
  Part10 §1: relay 재시작은 경기뿐 아니라 방/큐도 상실; DB 동기호출/프로세스분리의
  책임과 비용 설명. 도표 relay auth/verify를 현재 game-tickets/consume으로 갱신.
  RFC8489/6062/5681/6298 공식 자료 대조, 본문 근거 링크.
- 검증: SCRIPTED127/SDL132 CTest·경고 없음·98누적파일 보존.
  비용의 작은160000조합·u64경계·0인자·출력보존, 실제두경로/분할/회차·상태바이트
  ASan/UBSan 통과. 최종 명시적 호출순서도 재검증(099-final-contract.log).
  인라인11·Part구조/현재발췌/링크·Markdown27·489문제·전체DOM 계약 통과.
  타OS네이티브/수동GUI/WAN/NAT/실부하/수용량보증 미검증. root미변경이라 root CTest반복안함.
- coverage344: covered3/partial114/unassigned227/needs-review0. 검토한 H2 세 곳만 갱신.
  문서28/소스191. 정적 release **b99a2ac99b1c059b**, 15파일, 허용목록/상대경로/해시/재현ZIP 통과.
  HTTP18767 bytes일치: lessons.js6753282 / library.js5412422.
  logs099-check/final-contract/part/markdown/dom/site/http. 모든모델·검사작업 종료,HTTP만유지.
- 다음100: server/player_conn.cpp의 첫프레임deadline·초기phase 타입분기·authenticate·
  residual_stream(완성후속프레임+부분tail 보존)에서 출발. accept성공과역할/인증/경기준비
  성공을구별할것. 기존 listen_loopback/Socket/FrameParser 위에 bounded admission을 설계.
  첫모르는프레임을무시하는현재정책과기한/크기/완성된후속메시지인계의실제경계를대조할것.
  사용자계정·HTTP 전체구현은 후속단원 범위이며 가짜인증 성공으로 대체하지 말것.

## 98차시 종료 시점 기록

- 2026-09-29: **98차시 집필·실제 선택 검증 수정·검수 완료(progress)**.
  로컬 **1~98차시·484문제**, 공개 GitHub Pages **1~55차시·269문제 동결**.
  전체177차시 goal active. 다음 **99차시 “릴레이 선택: 직접 연결과 중계의 비용”** (`unit-099`).
  `98-end-negotiation` 누적. 099.json/99체크포인트는 아직 없다.
- 계약 유지: 단순 구현 초안은 OpenCode DeepSeek, 승인한 선택 자료만 전송.
  Codex 하위 에이전트·기록 이동 기능·수동 GUI·커밋/푸시/배포 없음. HTTP18767 유지.
- 98차시 15절·5문제·인라인 구현10(C++9/CMake1)·접힌 전체 파일3.
  규칙 terminal / 참여 의사 / 로컬 합의 관측 / 전송 종료 / 서버 검증 / 저장 확인을 구분.
  CS: 상태와 사건, 접수 결과와 상태, 멱등성/충돌, 안전성/진행성, 단조 시계,
  경과시간 마감과 경계 우선순위, 방향별 TCP 종료, 시드값과 round 식별자.
- DeepSeek `098-end-material.txt`/events.jsonl/draft.json exit0.
  선택한 byte_codec/framing 및 한정 명세를 /tmp cwd/tools deny로 전달.
  반환한 net/end_negotiation.h만 선별 적용하고 static_assert와 경과시간 주석 교정.
- 기준 TYPE42 payload는 exact9: nonzero round u64 LE + restart1/leave2.
  encode/decode 실패 출력 보존. EndNegotiation은 단일 main 소유, I/O/시계읽기/스레드/보상 API 없음.
  round/timeout0 거절. 라운드 준비 때 만들고 local terminal 뒤 activate하여 timer 시작.
  dormant에서 같은 round 원격 선택 한 개 선보관, 자신의 규칙 종료를 추정하지 않음.
  waiting에서 어느 한 leave면 leave, 양쪽 restart면 restart_agreed(로컬 관측).
  접수 중 같은 값 duplicate / 다른 값 conflict, 첫 값 보존. 종료 뒤에는 closed.
  old/future round와 invalid는 시계도 바꾸지 않음. main이 무응답/무효 입력 중에도 status를 폴링.
  정확 마감에서는 timeout이 먼저, 뒤로 간 시각은 상태 보존/clock_error.
  now-start 경과시간 비교; u64 시간 래핑을 정상 미래로 간주하지 않음.
  transport_ended는 dormant/waiting/restart_agreed를 transport_lost로 전환,
  leave/timed_out 사유는 보존. 새 라운드 설정 합의와 영속 결과는 별도 책임.
- end_timeline은 실제 두 RoundPlay에 seed77/drop 입력을 주어11틱 terminal 재현.
  peer 마지막 advance 전에 TYPE42를 dormant에 전달한 뒤 각자의 terminal에서 활성화.
  두 restart 관측 후 fixture가 round2 동일 설정을 명시적으로 공급(네트워크 시작 합의 아님).
  EOF 실험은 transport_lost, 승자/보상 추정 없음.
- root net/session.cpp: SendGameOverChoice는1/2만 허용.
  receive GAME_OVER_CHOICE는 exact1/값1·2 확인. 최초 remote choice를 CAS로 보존,
  동일 중복 허용, 상충 값은 첫 값을 보존하고 connectionFailed/quit.
  잘못된 길이·값은 무시. ClearGameOverChoices의 UI 결과 버퍼 초기화는 계정 보상 취소가 아님.
  실제 wire TYPE9에는 round ID가 없고, 이 수정이 전체 재시작 원자성을 제공하지는 않음.
- Part6 §8: Q가 직접 닫는 실제 경로·랭크 재매칭·비대칭 불일치 화면·시드값 재사용 한계·
  큐 수락과 상대 시작 차이 교정. RTT만큼 시차 보장/곧장 넘어가도 안전하다는 단정 삭제,
  main 주석도 교정. §8.4에서 의사/전송/판정/저장 경계를 설명.
  §12.3 exact1/domain/불변 원격 선택과 enum 기반 타입 일반화 교정.
  §9.1 덧셈 변환으로 범위 오류 해결이라는 설명을 현재 remaining 불변식으로 교정,
  basic.fundamental 표준 참조로 수정. §9.5 현재 INPUT 정확길이/전체 검증으로 교정.
  해당 H2 세 곳만 evidence 갱신. coverage344: covered3/partial111/unassigned230/needs-review0.
- 서버 no-meta 결과 경로는 ranked 생성/호출 조건과 대조해 현재 도달 경로를 확인.
  RankedGame Applied는 검증 중간 결과, relay의 저장 응답 뒤 wire 상태와 구분.
  이번에는 서버 보상 정책을 변경하지 않음. 기존 ranked_game CTest가 미종료 Incomplete를 검사.
- 검증: SCRIPTED125/SDL130 CTest와 경고 없음, 누적97 파일 보존.
  기준 상태/코덱과 실제 선택 분기·메서드 ASan/UBSan 통과.
  root 수정 전 extra-byte 접수 실패 재현, 수정 후 shape/domain/duplicate/conflict/reset 검사 통과.
  root 게임/양릴레이 빌드와21/20 CTest 통과. 인라인10 일치.
  Part·Markdown27·484문제·전체DOM 계약 검사 통과. 수동GUI/타OS/실망 검증은 미수행.
- 정적 release **e44a32fc1f34b908**, 15파일(자산14+manifest), ZIP 재현성 통과.
  HTTP18767 최종 bytes 일치: lessons.js6702801 / library.js5408851.
  문서28/소스191. logs: 098-check.log,098-dom.log,098-markdown.log,
  098-part-final.log,098-site.log. 외부모델·빌드·검사 모두 종료, HTTP만 유지.
- 다음99: Part7 §1 P2P와 relay 비용, §2 구조, Part10 §1 relay/meta 분리에서 시작.
  직접 연결/포워딩/권위 판정을 같은 개념으로 묶지 말 것. 지연은 실제 경로·큐·처리의 합이며
  relay가 항상 더 빠르거나 느리다는 단정, raw relay 자체가 검증이라는 오개념을 피할 것.
  98 기준 누적을 출발점으로 작은 실행 가능한 연결/비용 실험을 설계한 뒤 DeepSeek에 한정 위임.
- 남은 root 경계: wire round ID·같은 seed 새 사건 감지·새 설정 시작 확인,
  워커 중립입력 소유권·INPUT 중복충돌/거리·HASH 독립 정체 마감 등은 기존 기록 유지.

## 97차시 종료 시점 기록

- 2026-09-29: 직전96차시는 progress. 이번 **97차시 집필·실제 해시 보존 수정·검수도 progress**.
  로컬 **1~97차시·479문제**, 공개 GitHub Pages 1~55차시·269문제 동결.
  전체177차시 goal active. 다음 **98차시 “종료 협상: 통신 종료와 경기 판정”** (`unit-098`).
  `97-hash-observation` 누적. 098.json/98체크포인트는 아직 없다.
- 98에서는 half-close/EOF와 게임오버·판정·재대전 합의를 구별할 것.
  현재 Session GameOverChoice/SendNewSeed/ClearGameOverChoices와 main FSM,
  서버 권위 결과(MATCH_RESULT)·사용자 게임오버 표시의 의미를 대조한다.
  유효 round합의/재시작/기존 wire 무round-id 경계를 실제 종료 계약으로 확장할 때
  단순히 연결 EOF를 승패로 해석하거나 피어의 자체승리신고를 신뢰하지 말것.
- 계약 유지: 단순초안DeepSeek·선별명세/자료만전송, Codex 하위에이전트없음,
  기록이동재도입없음, 수동GUI없음, 커밋/푸시/공개배포없음. HTTP18767 유지.
- 97차시14절·5문제·인라인구현8(C++7/CMake1)·접힌완성파일3.
  개별atomic과논리트랜잭션, seq_cst 합법혼합, mutex 전체값복사,
  최신관측/이벤트보존, optional0, 고정주기창, 접수/중복충돌/앞틱누락,
  단회소비/clear/잠금밖진단/소진/시퀀스번호만으로plainpayload경합을해결못함 설명.
- DeepSeek097-mailbox-material/events/draft exit0. 상세자료구조명세만전송,
  /tmp cwd/tools deny. max매크로방어 numeric_limits 호출 교정,
  seqlock설명의동시비원자접근조건과다음비교틱/최대틱아님을교정.
- 기준 net/hash_mailbox.h: HashMailbox<Period,Capacity>, HashSample/HashComparison,
  하나의mutex 아래 local/remote optional배열+latest+uint64 next.
  Period부터시작, t0/offperiod invalid, next>u32MAX exhausted, t<next stale,
  범위밖 too_far, 같은slot동일 duplicate/상이 conflict. 실패상태보존.
  remote stored/duplicate때만latest갱신(마지막접수도착이며최대tick아님).
  latest_remote 비소비복사/실패out보존. poll은두front있을때만복사+shift+
  next전진, 불일치도소비. next_tick진단조회. clear전체리셋이나외부옛자료취소아님.
- observation_demo: phase atomic으로writer의tick갱신뒤hash갱신전reader를실행.
  합법적 seq_cst (8,111) 혼합을결정적으로재현. TSan오류없는것이논리정확성증명아님.
  이어 latest8만관측해도 창에서는4/8을모두소비하는차이를출력한다.
- 실제 net/hash_mailbox.h 템플릿은namespace/헤더가드/HashExchange<600,8>별칭을제외하면
  기준과동일하며checker가검증. Session기존hashMu_/2필드대신HashExchange소유.
  SendHash는local접수뒤송신큐에넣음, local실패는connectionFailed/quit.
  HASH분기는exact12/remote접수, stored/duplicate/stale 허용, 그외실패/quit.
  latest진단 API GetLastRemoteHash유지, 새 PollHashComparison(tick,local,remote) 소비API.
  실패out보존, hash0유효. 시작/Close/ClearInputs 7곳에서mailbox.clear.
- 실제 main의4칸local링+latest원격1개/lastSeen커서 제거. ready된 비교쌍을whilepoll로소비,
  로그/UI는잠금밖. 600틱상수는HashExchange::period사용, 역할순서결합/wire필드폭유지.
  현재기록시각과비교틱진단분리유지. 미비교600불일치가1200일치에덮이는버그를수정.
  접수validtick정책은더엄격해짐; hash규약협상/roundID는아직없음.
- 기본root CTest에hash_mailbox_test 추가(Threads링크). 실제600주기/8칸/앞불일치/
  미완성앞틱/0hash/중복/충돌/초과/clear 검증. root CTest개수이제21/20.
- 기존테스트의HASH를수신barrier로쓰던임의tick99..103/123을600단위로변경.
  scripts/check_learning_seed.py/serialization.py와session_seed/payload.cpp만
  그barrier계약에맞춤. 실제SEED역할/잘못된SEED보존/재시작·INPUT/MATCH_RESULT회귀통과.
  Pythonframing패리티의tick60예제는wire코덱검사라그대로유지(세션의미정책과다름).
  현재Python netbot폴더는코덱/expander/export보조이고HASH송신하는실네트워크클라이언트없음.
- Part6§4/6/7/12/14/16/17/18/22와부록의현재설명/발췌갱신.
  Part7/Part12의초기화발췌동기화. §16은같은mutex의값복사/보존창/소비/언어경합/
  원자성vslockfree/clear범위로재구성. 4칸링/최신원격1개/tick0 sentinel낡은설명제거.
  기존92차시는안정스니펫유지, 현재참조절만새Poll API/현재8칸으로갱신(version1.0.1).
- 이전92검사기의current root_probe는새현재소스회귀로연결, 과거before검사유지.
  95함수목록보존검사는기존함수삭제금지로완화해새public API추가를허용.
  새코드의누락을허용한것이아니며roomThread/queueThread의기존동일검사는유지.
- 검증: SCRIPTED123/SDL128 CTest·경고0·96누적파일허용변경범위확인.
  ASan/UBSan/TSan mailbox20000동시쌍+일관latest조회/2생산자/소비자/진단reader,
  별도의순서/누락대기/개수/중복충돌/0/MAX/출력보존/초기화검사통과.
  observation_demo도ASan/UBSan/TSan통과하며의도한논리혼합출력.
- 실제Session/main발췌회귀: 수정전600불일치miss assertion실패,
  수정후앞600불일치+뒤1200일치/로컬지연/한번만소비/상한/충돌/형식/clear통과.
  로그captured_tick1201/compared_tick600구별검사. 실제게임·양릴레이빌드,
  새기본테스트포함root21/20 CTest통과. 기존실제세션회귀및92checker위임통과.
  수동GUI/nativeWin/macOS/모든스케줄/실서비스임계값튜닝은미검증.
- Part현재발췌·Markdown27·8스니펫·479문제·전체DOM·정적재현ZIP통과.
  문서코드발췌교체에서끝newline차이로4곳누락을발견해완전한함수/블록으로교정;
  최종Part검사통과. 초기의불일치를성공으로간주하지않음.
- coverage344:covered3/partial110/unassigned231/needs-review0. §22API대응은일부만
  검토했으므로partial. 원문28/현재소스191. release **759eaa6318e77f2b**,15파일.
  HTTP18767 lessons.js6649310/library.js5403479 바이트디스크일치.
- 로그097-check.log(누적123/128,기본추가전root20/19),097-final-regression.log
  (최종root21/20),097-existing-session-regression.log,097-tsan.log,
  097-legacy-checker.log,097-part-final.log,097-markdown.log,097-dom.log,097-site.log.
  통합checker97-hash-observation연결. 외부모델/검사/빌드모두종료,HTTP만유지.
  자동승인거절없음. 긴도구대기함수cell938은같은핸들로재확인해정상종료했음.
- 남은경계: roundID/HASH알고리즘협상·해시없음독립마감(현재창초과실패),
  워커중립입력의페이즈/생산자인계,입력충돌중복/거리정책,u32입력틱순환,
  DNS연결취소/IOCP수명. 이번수정으로최신HASH의미비교기록유실은해결했으나
  전체게임/보안완료는아님.

### 보존: 96차시 종료 상태

- 2026-09-29: 직전95차시는 progress. 이번 **96차시 집필·검수도 progress**.
  로컬 **1~96차시·474문제**, 공개 GitHub Pages 1~55차시·269문제 동결.
  전체177차시 goal active. 다음: **97차시 “원자적 관측: HASH pair 레이스”** (`unit-097`).
  `96-round-inputs` 누적. 097.json/97체크포인트는 아직 없다.
- 97 시작 시 현재 hashMu_ 아래 tick/hash를 함께 읽는 API와 latest-only 단일 슬롯을
  구분할 것. 원자적 스냅샷은 모든 HASH 이벤트 전달 보장이 아니다. 기준 HashAudit는
  이미 유한 창/명시 실패를 사용한다. 실제 main의 비교 지연과 lastRemoteHashSeenTick
  소비 시점, latest overwrite를 대조하고 기존 완료92~93의 모델과 중복을 피할 것.
- 사용자 계약 유지: DeepSeek 단순 초안·승인한 선택 자료만 전송, Codex 하위에이전트 없음,
  기록 이동 재도입 없음, 수동GUI 없음, 커밋·푸시·공개 배포 없음. HTTP18767만 유지.
- 96차시14절·5문제·인라인 구현10(C++9/CMake1)·접힌 완성 파일5.
  backlog의 소속, 상태 전이, 로컬 생산/원격 보관/시뮬레이션 진행 조건,
  countdown 비대칭, (round,tick), 후보 교체, 늦은/미래 round, 소비된 틱,
  큐 정리와 활성 송신 수명, 실제 준비 가드를 설명. 집필 보고는 본문에서 제외.
- DeepSeek096-round-material/events/draft exit0. input_codec/byte_codec/framing
  선택 코드와 명세만 /tmp cwd/tools deny로 전송. round_gate/round_protocol 초안 검토.
  통합에서 형식 오류를 inactive와 구분하도록 RoundScope::invalid_frame 추가.
- 기준 RoundGate: waiting/countdown/playing/ended, prepare는 waiting/ended에서
  id>기존id이며 비영일 때만 countdown. start/finish 허용전이만 적용, 거절상태보존.
  capture playing만, 같은round 수신 countdown/playing허용. old/future순서분류,
  0예약/MAX이후새prepare불가. 단일 main 소유자이며 자동시계/새thread없음.
- TYPE41 RoundBatch: u64LE round +u32LE firsttick+u16LE count+mask[count],
  count1..16, payload15..30, fullframe18..33. 정확길이/틱span/마스크검증·출력후보commit.
  실제 root INPUT4와 별개인 학습용 확장. 인증/seed합의/재접속식별을 자동제공하지않음.
- 기준 RoundPlay: Gate+optional DelayedLockstep 조합. prepare는 게이트후보와게임후보
  검증후함께교체; 잘못된역할/지연/끝난보드면보존. capture는 게임후보기록+Frame생성
  성공시커밋, 실패커서/out보존. 송신접수성공과다르며 호출자는full때Frame보존+생산제한필요.
  receive는 decode→scope→receive_batch 순서. malformed별도결과,
  old/future현재창미변경, 미래수신만으로prepare안함. current에서duplicate/conflict/stale구분.
  advance는playing에서만위임; 진행결과advanced와그결과ended를함께보존, 틱소진후종료.
- round_timeline: 실제두DelayedLockstep+codec에 직접프레임전달. host먼저start,
  peercountdown에수신보관→양쪽tick0소비→peer새round준비→옛Frame거절→새tick0적용.
  실제TCP 지연시험이 아니라 전달시점을결정적으로고정한 실험임을 본문에명시.
- root 수정: Session::SendInput 맨앞 connected/ready/quit 검사. 거절시활동시각/
  watermark/큐미변경. main 생산가드에도 session.isReady 추가.
  ready검사는라운드합의/게임페이즈를대신하지않고 원자변수묶음트랜잭션도아님.
  root실제wire에round-id추가없음; 별도워커중립생산경로/입력소유권인계는미해결경계.
- Part6§5 현재SendInput 발췌·§14main발췌갱신. §14.4~14.10 설명교정:
  안정60frames/s와1틱큐 보장 제거, emplace가언제나즉시DESYNC를만든다는단정교정,
  SEED유실→PONG중단 필연관계 제거, ACK가tick무관이라는표교정,
  필터가활성/OS송신취소가아님·round가있어도상태초기화필요·1byte재사용문제 명시.
  RoundPlay 소속검사와 currentroot차이, 워커중립입력소유권한계도기재.
- coverage Part6§5해시대조유지/§14신규partial. section344:covered3/partial109/
  unassigned232/needs-review0. 원문28/소스190. 전체Part완료로승격하지않음.
- 검증: SCRIPTED121/SDL126 CTest·경고0·95누적파일변경범위검사.
  ASan/UBSan TYPE0..255/size0..39/mask0..255/count0..255/LE고정벡터/
  출력보존/0/MAXround/틱범위, 상태전이/prepare실패/두게임상태/초기수신/
  old/future/stale/duplicate/conflict/32칸포화/중립자동생성없음/u32소진통과.
  초기단위검사에서 유효count밖의 미전송 배열칸까지 원본과같아야한다는잘못된assert를
  수정했다. 실패출력보존은 여전히배열전체비교, 성공은활성전송필드비교. 제품코덱오류아님.
- old-round수신을허용하도록만든 mutation은 새round tick0 stored검사에서실패하여
  이력오염검출을확인. 최신checker에포함, 096-final-regression.log통과.
- root actualSendInput함수추출: 수정전실패→수정후8상태조합/거절sideeffect없음통과.
  mainready조건소스대조, 실제게임·양릴레이빌드 및 root20/19 CTest통과.
  새코드는단일owner; 새ThreadSanitizer검사는요구하지않음. native타OS/수동GUI미검증.
- Part발췌·Markdown27·10스니펫·474문제·전체DOM·정적재현ZIP통과.
  원고의해시메시지번호오타TYPE10→실제TYPE21로최종대조교정, 생성기최종검증.
  release **0b100c3404638801**,15파일. HTTP18767 lessons.js6608183/library.js5394919
  바이트디스크일치. 공개release는동결.
- 로그096-check.log,096-final-regression.log,096-mutation.log,096-part-final.log,
  096-markdown.log,096-dom.log,096-site.log. 통합checker96-round-inputs연결.
  외부모델/빌드/검사모두종료,로컬HTTP만유지. 이번턴자동승인거절없음.
- 남은경계: root라운드ID/seed·HASH합의확장, latestHASH이벤트손실,
  워커중립입력생산페이즈/동시소유권,충돌중복/거리정책,u32틱순환,DNS취소/IOCP수명.
  준비가드와학습round구현을 전체보안완료로해석하지말것.

### 보존: 95차시 종료 상태

- 2026-09-29: **95차시 집필·검수 완료**, 로컬 **1~95차시·469문제**.
  공개 GitHub Pages 1~55차시·269문제 동결. 전체177차시 goal active; 이번 턴은 progress.
- 다음 시작점: **96차시 “오래된 입력: 대기 중 backlog를 처리”** (`unit-096`).
  `95-backpressure` 누적. 096.json/96체크포인트는 아직 없다.
  실제 Part6§14와 Session ClearInputs/ready/중립 입력·소비 흐름을 대조할 것.
  대기 큐의 INPUT/HASH 제거는 활성 송신/이미 전송한 기록의 취소가 아니며 wire round-id가 없다.
  INPUT 거리 정책은 마지막 원격 watermark를 기준으로 하며 충돌 중복은 첫 값 유지.
  최신 HASH 단일 슬롯·라운드 식별·u32틱 순환·SEED 권한 등은 남은 경계다.
- 사용자 계약: 단순 구현 DeepSeek·승인한 선택 자료만 전송, Codex 하위에이전트 없음,
  기록 이동 재도입 없음, 수동GUI 없음. 커밋·푸시·공개 배포 없음. HTTP18767 유지.
- 95차시14절·5문제·인라인 구현7(C++6/CMake1)·접힌 완성 파일5.
  생산/소비 속도, 개수+wire 비용, 고저수위 히스테리시스, 원자적 접수,
  queued→active→complete 예산 수명, partial send, full 재시도 순번,
  통계 snapshot과 접수 예약, 계층별 완료, 배치 용량 실패를 설명한다.
  객관식은 TCP 재전송/애플리케이션 미접수 등 실제 오해를 오답으로 대조.
- DeepSeek095-flow-material/events/draft exit0. framing/frame_queue 선택 코드+명세만
  /tmp cwd/tools deny로 전달. 파서-워커 위치 오설명 교정, complete 단일 소비자 계약 명시.
- 기준 FlowQueue<8,140,105,70>: 개수는 queued+active1, 비용 size+3.
  try_pop은 비용 반환 없음, complete만 현재 active 차감; 105 이상 paused,
  완료로70 이하가 되어야 재개. ByteLimit 사전 차감 비교, close후소진,
  invalid/full 출력보존, stats는 잠금 아래 값 복사. RSS 측정/lockfree 아님.
  complete는 개별 lease ID 검증이 없으므로 단일 소비자가 정확히 한 번 호출해야 함.
- ThreadLink application_frame 표시로 일반 프레임 최종 부분송신 완료 시만 비용 반환.
  PING/PONG은 별도 고정 슬롯 예산. 수신8칸 full실패 정책 유지.
  flow_probe TYPE40/payload32, LEu32순번+0x5a28개, stored때만전진/full같은번호재시도.
  새접수250ms 정체 또는전체10초 실패. 접수 제한이지 개별 큐 체류시간 제한은 아님.
- 실제 Session: queued4096 + pendingSendBytes1MiB(queued+현재transport호출).
  pushSend에서 정지/빈값/최대frame초과/개수/바이트검사, 삽입성공뒤charge.
  pop때유지, I/O잠금밖실행, 반환성공/실패모두현재pktcharge차감.
  한순회최대64프레임/quit재확인, 송신실패connectionFailed설정.
  시작/Close6곳큐와카운터동시초기화. ClearInputs는 폐기한 queued만차감.
  WSS성공은 어댑터별128KiB 보류 큐 수락이어서 상대게임적용완료 아님.
- 실제 INPUT: 같은inMu 아래 거리조건내 신규키수를 사전계산.
  용량부족이면 map/watermark 미변경·noACK·실패. 전부들어가면적용.
  기존 거리밖skip/충돌중복첫값유지/할당예외는 이 수정의 전체원자성 보장이 아님.
  ioThread 수신프레임마다quit재확인. 수정 전 바이트접수/부분INPUT 회귀실패→수정후통과.
- Part6§4/6/12/13, Part7§11, Part12§7/8 발췌/설명검토와 evidence해시 갱신.
  4096개를68초무응답으로단정, 메모리RSS와wire비용혼동, main모든API즉시반환,
  ClearInputs만으로라운드혼입방지라는 과도한 설명 교정. 전체절완료로승격하지않음.
- 작업 중 범위 선택 오류로 session.cpp의 유사한 로비루프~ioThread 구간이 잘못
  편집된 것을 빌드 전에 발견. 095-before-session.cpp(완료94상태)에서복원하고
  ioThread 이후로앵커를한정해95변경만재적용. 함수목록 동일 및 roomThread/queueThread
  본문 동일을 checker로검증, 현재 Session 직접컴파일/게임빌드 통과.
  /tmp/impl95.py는 기존 넓은 앵커가 남아 있고 전체재실행은 중복패치하므로 실행하지말것.
- 복구 도중 자동승인검토가 사용량한도 때문에 실패해 실행이차단된 턴이 있었다.
  우회하지않았고 이후 get_usage_limits에서 ordinaryUsageAllowed true를 확인한 뒤
  정상승인경로가 다시성공하여 복구했다. 리셋크레딧 사용없음. 현재차단없음.
- 최종 CTest SCRIPTED119/SDL124·경고0·누적변경범위확인.
  ASan/UBSan 순수queue/실제worker/root실제분기 통과. TSan 순수queue와
  실제ThreadLink partialsend대역 경로통과. 100000모델연산/10000인계,
  실제TCP 즉시/50ms지연상대각500기록 순번/본문/EOF/complete/bytes0통과.
  지연reader검사는 커널 송신포화 입증이 아님. native타OS/수동GUI미검증.
- 095-final-check.log 양backend통과 뒤 인라인 대조가 오탐으로실패했다.
  여러파일을합쳐토큰화하면 파일경계가문자열/주석해석에영향줄수있어 파일별정규화로수정.
  개별실제파일과스니펫은일치. 최신095-final-regression.log의 전체회귀/7스니펫/
  root20/19 CTest통과. 이전실패로그를 지우거나 성공으로오인하지말것.
- Part검사·Markdown27·469객관식·전체DOM 계약·정적재현ZIP통과.
  원문28/소스190, coverage344: covered3/partial108/unassigned233/needs-review0.
  release **5edd2088e2e56cd4**,15파일. HTTP18767 lessons.js6556708/library.js5390171
  바이트가디스크와일치. 문항오답표현최종보완은동일schema/렌더경로,생성기에서최종검증.
- 로그095-final-check.log,095-final-regression.log,095-tsan.log,095-part-final.log,
  095-markdown.log,095-dom.log,095-site.log. 통합checker95-backpressure연결.
  외부모델/빌드/검사모두종료, 로컬HTTP만유지.

### 보존: 94차시 종료 상태

- 2026-09-29: 직전93차시는 progress. 이번 **94차시 집필·실제 PONG 교정·검수도 progress**.
  로컬 **1~94차시·464문제**, 공개 1~55차시·269문제 동결. 전체177차시 goal active.
- 다음 시작점: **95차시 “백프레셔: 느린 상대와 제한된 메모리”** (`unit-095`).
  `94-heartbeat` 누적. 095.json/95체크포인트는 아직 없다.
  현재 기준 ThreadLink는 양방향8개 Frame, 부분송신1개, 응답대기1개, 파서64바이트.
  outgoing full 반환, incoming/echo full은 실패. queued 체류 마감은 없고 개별 송신5초,
  heartbeat 전체무응답 기본3초(실험350ms). 현재의 제어 우선순위와 일반 큐 작업량,
  application admission/worker buffer/OS socket buffer를 함께 따져 백프레셔를 재구성할 것.
  실제 Session4096프레임 sendQ와 INPUT보관8192/window4096, overload 실패·silent drop
  경계는 서로 다르다. 최신 HASH 단일 슬롯 문제도 여전히 남음.
- 사용자 계약 유지: 단순 구현 DeepSeek·선택 자료 전송, Codex 하위에이전트 없음,
  기록 이동 재도입 없음, 수동GUI 없음. 커밋·푸시·공개 배포 없음. 로컬18767만 갱신.
- 94차시14절·5문제·인라인 구현9(C++8/CMake1)·접힌 전체파일5.
  장애 감지 정책과 원인 증명, main/I/O 관찰 경계, 토큰/시계, 후보검증,
  시각 경계·단일 pending·단회소비·만료우선·워커 control dispatch·half-close·keepalive 설명.
- DeepSeek094-beat-material/events/draft: exit0. 선택 framing/byte_codec 선언+정확명세만
  전달, /tmp cwd·tools deny. 불필요 조회 API 제거, Windows max매크로 보호,
  optional 인라이닝의 초기화 경고는 wire예약값0인 명시 pending토큰으로 정리.
- 기준 net/heartbeat.h: TYPE30PING/31PONG, exact8 LE u64 비영토큰. codec 실패out보존.
  Heartbeat(now,timing) 순수 singleowner, 기본1000/2000/3000ms; interval<suspect<timeout.
  status: waiting/healthy/suspect/expired/clock_error. 역행now 객체보존,
  나머지호출은 lastnow전진/만료를 갱신할 수 있음. lastgood는 시작/마지막유효확인 시각.
  정확timeout에서 expired latch, 이후PONG으로 부활안함. issue는 주기+단일pending,
  토큰1부터증가, MAX다음0으로발급중지. 전체2^64회 소진 실행은 미검증.
  pong은 status먼저→정확pending1회소비→lastgood/confirmed 갱신. 미요청/재사용불인정.
- ThreadLink constructor heartbeat=false기본, timing검증후스레드생성. 워커relative
  steady_clock ms로 status/issue/pong호출. 루프상단만료면 heartbeat_timeout(enum7).
  현재부분송신완료후 echo1슬롯→새PING→일반큐 순서. byte interleave 없음.
  control은 워커에서만소비, 확인횟수atomic공개. suspect의UI조회API는 아직없음.
  잘못된control문법 protocol_error, echo적체 receive_full. send_closed뒤PING에는응답불가,
  EOF대기에도마감적용. 하트비트무응답은 로컬지연/송신대기도 포함하며 인증아님.
- heartbeat_probe: 40/150/350ms로 약600ms교환후 TYPE32빈완료표시→finish_sending,
  상대marker+localclosed+peerEOF/complete+confirmed>=3 확인. 임의조기오류를성공처리안함.
  이것은제어경로실험이지 게임결과/SEED/규칙진행인증아님. 기존도구들은옵션off유지.
- 실제 net/pong_window.h 추가: 수신워커소유16 timestamp optional창.
  remember는 lastissued보다큰값만등록(소비/퇴출한토큰재발급불가), consume은
  now>=token, age<10000, 미소비정확토큰만1회소비. 0도가능한root timestamp값.
  Session ioThread시작에새창초기화, ready PING큐접수때등록, ready/exact8PONG만consume.
  malformed PING에 echo하지않음. wire형식유지, 기존정상timestamp에코피어와호환.
  같은밀리초/역행발급은건너뛰고다음주기재시도. 신원인증/전체어뷰징해결아님.
- 실제 main문구 ‘Opponent frozen’→‘Waiting for peer response’,
  ‘Opponent disconnected’→‘Peer response lost’. 원인확정이아닌응답관찰로표현.
  Session LinkStatus주석/Part6§11도관찰·유예·회복정책으로교정.
- Part6§6/11/12 현재code발췌갱신. ioThread PING간격과16ms중립입력의
  고정60Hz보장설명교정. main정지와I/O응답, 크래시와조용한경로단절,
  동일컴퓨터시계가아님, 토큰상관관계, grace와타이밍원인추론한계 추가.
- net/socket.cpp keepalive주석교정(동작변경없음). 같은idle/interval이같은감지시간을
  보장한다는설명제거; 현재Windows경로는TCP_KEEPCNT미설정. 공식Microsoft
  IPPROTO_TCP 옵션표(TCP_KEEPCNT Win10 1703+)와SIO_KEEPALIVE_VALS문서대조·Part링크.
  https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options
  https://learn.microsoft.com/en-us/windows/win32/winsock/sio-keepalive-vals
- 검증: SCRIPTED116/SDL121 CTest·경고0·93누적코드허용변경범위확인.
  ASan/UBSan:코덱type0..255/길이0..39/LE/MAX/0/출력보존,시간경계/역행/MAX부근,
  요청·중복·미요청·기준마감. main650ms미소비중워커응답/수신큐control없음/
  invalidtiming검사. 같은 실제워커경로 ThreadSanitizer 통과.
- 실제 두프로세스3쌍정상종료; 상대대역5가지침묵/wrong/replay/잘린PONG/PING-only
  기대protocol_error또는heartbeat_timeout. 응답만왕복시키며 게임상태일치검사아님.
- root actualPING/PONG case발췌회귀: 수정전emptyPONGassert실패, 수정후
  length/readiness/correlation/replay/future/age/창퇴출/MAX/0/재발급금지통과ASan/UBSan.
  실제게임·양릴레이빌드 및 root20/19 CTest통과. 타OS네이티브/수동GUI미검증.
- checker가실행중일때루트등록조건을보강하여메모리에남은옛소스문자열assert가실패했다.
  이는최종handler실행회귀통과뒤발생한검사버전불일치. 최신checker --skip-build 재실행에서
  모든루트/통신/스니펫/게임빌드검사통과. 기존양backend CTest완료증거는094-check.log,
  최신회귀완료증거는094-regression.log. 실패를미해결코드실패로오인하거나숨기지말것.
- Part현재발췌·Markdown27·9스니펫·464문제·전체DOM·정적재현ZIP통과.
  원문28/소스190, coverage344:covered3/partial108/unassigned233/needs-review0.
  release **bfb9dfd759eb6d7c**,15파일. HTTP18767 생성물바이트일치.
  로그094-check.log,094-regression.log,094-tsan.log,094-dom.log,
  094-part-final.log,094-markdown.log,094-site.log. 통합checker94-heartbeat연결.
- 남은경계: root최신HASH손실/라운드ID/해시협상/SEED권한/중립입력생성인계/
  u32틱순환/DNS-connect취소/IOCP수명/전체메시지rate정책. 전체보안완료아님.
  외부모델·검사·빌드모두종료, 로컬HTTP만유지. 자동승인거절없음.
- HTTP: lessons.js 6502987 bytes, library.js 5382857 bytes.

### 보존: 93차시 종료 상태

- 2026-09-29: **93차시 집필·검수 완료**, 로컬 **1~93차시·459문제**.
  공개 1~55차시·269문제 동결. 전체177차시 목표 active, 이번 턴은 progress.
- 다음 시작점: **94차시 “하트비트: 무응답과 연결 종료”** (`unit-094`).
  `93-thread-queues` 누적. 094.json/94체크포인트는 아직 없다.
  ThreadLink는 연결 이후 소켓을 워커만 사용하며 수신 idle 마감은 없다.
  main/규칙 일정·PING/PONG 응답·로컬 처리 지연과 실제 원격 무응답을 구분하는
  하트비트 정책을 설계할 것. 지금 thread_probe는 4개 고정 StateStamp 전송 실험으로
  게임 해시 검증이나 지속 GUI 통합이 아니다. 기존 hash_probe는 그대로 유지한다.
- UI 최신 요청은 이미 처리된 상태임을 확인: 목차 단색 ✓, 동일 브라우저 저장,
  현재 위치와 구분한 ‘이전 읽던 위치’. 이번 턴에 UI를 중복 수정하지 않았다.
- 사용자 계약 유지: 단순 구현 DeepSeek/선택 자료만 전송, Codex 하위 에이전트 없음,
  기록 이동 재도입 없음, 수동 화면 검사 없음, 커밋·푸시·공개 배포 없음.
- 93차시14절·5문제·실제 인라인 구현9(C++8/CMake1)·접힌 전체 파일5.
  CS: 동시성/병렬성, 데이터 경합/논리 순서, mutex의 동기화·가시성,
  값 소유권과 호출 원본 수명, 유한 큐/순환 인덱스, 정상 소진/취소/join,
  atomic relaxed의 적용 범위, 보고와 큐의 별도 게시, 최신 상태/사건 기록.
- DeepSeek093-queue-{material.txt,events.jsonl,draft.json} 완료 exit0.
  선택 framing/socket/stream 선언과 구체적 명세만 전송. cwd /tmp, tools deny.
  frame_queue.h·receive_socket.h/cpp 초안 검토: 잠금 대기 ‘briefly’ 보장 제거,
  불필요한 size/closed 조회 제거, Winsock 헤더 우선, recv 반환 auto 교정.
- FrameQueue<N>: owning Frame 배열, head/count/closed 한 mutex. 잘못된 payload는
  invalid; full/closed는 저장 안 함. pop 실패 out 보존. close는 저장 항목 소진 허용.
  try는 데이터/공간 대기를 안 할 뿐 mutex 획득 대기는 가능. lock-free 아님.
- ThreadLink: main이 연결 완료 Socket을 이동. Runtime은 ThreadLink보다 오래 생존.
  worker_ 마지막 멤버, 객체 noncopy/nonmove. 워커 소켓·파서·부분송신 독점.
  양방향8칸 큐, 논블로킹 유지, 루프당 send시도1 + parse1 또는 recv16바이트.
  기존 동기 Connection을 다른 스레드에서 close하는 계약으로 바꾸지 않음.
- send deadline은 dequeue부터5초, queue 체류 시간 제외. offset으로 suffix 재전송,
  현재 프레임 완료 전 다음 프레임 interleave 금지. idle sleep1ms는 시간 상한 아님.
  수신 full은 receive_full 실패. 송신 full은 호출자가 재시도/실패 처리.
  report는 별도 mutex optional; 소켓 reset→큐 close→보고 게시 순서이므로
  큐 closed를 보고도 report가 아직 없을 수 있음. 보고가 있어도 남은 큐 소진 필요.
- finish_sending: 큐 소진 후 shutdown_send, 상대EOF도 있어야 complete.
  request_stop/destructor는 cancelled이며 미전송 포기 가능. join은 잠금 밖에서,
  멤버 파괴 전 완료. 시스템 thread/mutex 예외 복구는 이 예제 범위 밖.
- thread_probe listen/connect: four known StateStamp(4i,11+i,29+i), DONE frames4/end0.
  accept/connect는 시작 단계의 동기식, 워커 취소/마감 범위 밖.
  실게임 schedule·SEED 협상·HashAudit는 main 소유로 연결할 경계이며 지속 경기 미통합.
- 실제 Session 동작 수정은 없음. Close 주석의 ‘블로킹 해제됨’ 단정을
  미공개 DNS/connect 작업의 완료 시간은 보장하지 않는다는 설명으로 교정.
  Part6 §6: 큐 분리=타입 규칙 강제 오류, main 동기 송신 대기, mutex/값 인계,
  최신 HASH의 손실 범위, handshake 10초가 connect도 제한한다는 오류,
  idle주파수 및 join 종료 보장 단정 교정. 원문 발췌/HTML 레퍼런스 함께 갱신.
- 누적 SCRIPTED114 / SDL119 CTest, 경고0. 92 누적 코드 무변경 확인
  (README/DESIGN/CMake는 해당 차시 확장). 실제 thread_contract ASan/UBSan/TSan 통과:
  2칸wrap/출력보존/full/close/drain/복사독립/10000개 FIFO 스레드 인계,
  idle취소/즉시소멸20회/invalid소켓/수신 길이·잘림·overflow/EOF후송신.
- 실제 ThreadLink loop + 어댑터 대역: 1바이트씩 송신, interrupted/would_block,
  두 프레임 순서, 3바이트 후오류, 5초마감 ASan/UBSan. 대역은 커널 증거 아님.
  독립 두 프로세스4쌍의 실제 소켓 FIFO 교환 통과. Windows/macOS 네이티브 미검증.
- Part현재발췌·Markdown27·9스니펫·459문제·전체 차시 DOM·정적 재현ZIP 통과.
  원문28/소스189, coverage344:covered3/partial107/unassigned234/needs-review0.
  release **77088fb114cef7a2**,15파일. HTTP18767 두 생성물 바이트 일치.
  로그093-check-final.log,093-tsan.log,093-part.log,093-dom.log,093-site.log.
  통합 checker93-thread-queues→check_learning_threads.py.
- 이전 원격HASH 최신값 덮어쓰기·라운드ID·해시규약 협상·DNS/connect 취소 등은
  그대로 열린 경계다. 이번 mutex 설명/워크플로가 실제 Session의 이벤트 보존을
  수정했다고 주장하지 말 것. 다음 과정에서 필요한 범위로 설계할 것.
- 이번 초기 빌드와 checker가 잠시 겹쳤으나 초기 빌드만 중단하고 checker 재실행.
  최종 양쪽 빌드·전체 검사는 별도093-check-final.log에 성공 기록.
  외부 모델/빌드/검사 모두 종료, 로컬HTTP만 유지. 자동 승인 거절 없음.
- HTTP 검증 크기: lessons.js 6450727 bytes, library.js 5377210 bytes.

### 보존: 92차시 종료 상태

- 2026-09-29: 직전91차시 턴은 progress. 이번92차시도 집필·실제 해시/Part 교정·검수로 progress.
  **로컬 1~92차시·454문제**, 공개 1~55차시·269문제 유지. 전체177차시 목표 미완료.
- 다음 시작점: **93차시 “스레드와 큐: 데이터 소유권”**.
  `92-hash-audit`에서 누적한다.093.json/93체크포인트는 아직 없다.
  기준 Connection은 단일소유 동기식/수신무기한블로킹. main의 다른스레드close취소계약없음.
  안전한워커소유·큐·종료인계로확장할때이계약을그대로위반하지말고명시적으로설계할것.
  현재 Session의 sendMu/sendQ,inMu,hashMu와main조회경계를대조한다.
  실제 GetLastRemoteHash는최신한쌍만보관:먼저받은600이1200으로덮이면600비교기회손실.
  92에서미래해시의조기처리표시는고쳤으나최신값저장계약은유지됨.93에서유한큐/
  누락·과부하·정리정책과함께이한계를검토할수있다.기존barrier테스트는GetLastRemoteHash사용.
- 공개55동결·기록이동재도입금지·수동GUI불필요·Codex하위에이전트금지유지.
  외부모델선택자료만/tmp cwd·도구deny.커밋/푸시/배포없음.전체goal active.
- 92:15절·5문제·실제인라인구현7(C++6/CMake1)·접힌전체파일4.
  입력번호/완료상태번호·명시적해시대상·역할/출처·XOR상쇄·wire코덱·제한비교창·
  선도착/누락·주기샘플의범위·현재라운드/진단시점을설명.검수환경보고는본문제외.
- DeepSeek092-audit-{material.txt,events.jsonl,draft.json}:exit0,hash_protocol.h/hash_audit.h초안.
  선택byte_codec.h/framing.h와API명세만전송.키/설정/전체저장소제외.
  unscoped AuditPut→enum class,관찰자변경때역할필드도바뀐다는오해주석교정,
  u32입력레코드와프레임단위용어구분.아직표준네트워킹프로토콜이아닌학습규약.
- 기준net/hash_protocol.h:StateStamp{tick,host_hash,peer_hash}:u64각3,payload24LE,TYPE21.
  tick은완료한입력쌍수:초기S0,입력0적용후S1.0..UINT32_MAX+1의4배수만유효.
  해시0/MAX포함모든bit유효.encode/decode정확타입/길이/주기/상한검증후후보적용.
- net/hash_audit.h:각출처optional<StateStamp>8칸,next0.주기4창0..28,next는poll만진행.
  record_local/remote는출처이고각snapshot필드는항상host/peer정렬.
  stored/동일duplicate/충돌conflict보존/stale/too_far/invalid/exhausted 구분.
  poll양쪽첫슬롯없으면출력/창보존false.있으면역할별equal계산·한샘플shift·next+=4.
  불일치도비교1회소비,호출자가중단결정.초기0누락시뒤샘플로건너뛰지않음.
  모든호출단일소유직렬.새라운드는새HashAudit.틱최대부근codec검사했지만
  기본next0에서10억개이상poll하는exhausted종단실행검사는하지않음.
- tools/hash_probe.cpp:Config{seed,0,D},자기초기HASH0송신→32입력capture→2개16입력묶음
  16..31/0..15송신.수신TYPE20입력/21해시분기,프레임반영후compare_ready,
  advance성공뒤4배수publish(local기록과송신에같은stamp)·다시compare_ready.
  목표32-D소비후자기HASH도모두송신됐으면halfclose,상대해시는계속읽는다.
  EOF성공은목표입력소비+모든주기샘플비교.누락/불일치/범위오류는실패.
  D2는상태0..28주기비교/소비30,29~30은wire미검사. D30은초기0만비교/소비2.
  DONE마지막hash는별도출력진단이며주기완료범위와구분.최종상태인증규약없음.
  고정작은실험,지속GUI통합/전체시간제한/자동상태복원없음.
- 실제 core/hash.h:hash_player_pair(host,peer),domain {'D','U','E','L',1}와
  두u64를명시LE로FNV누적.동일값XOR상쇄를없애지만일반적충돌/인증한계유지.
  main은session.params().role로인자정렬,CLI isHost에의존하지않음.
  wire폭유지지만구XOR와결합값이달라양클라이언트함께갱신필요.협상필드는아직없음.
- 실제main:원격hash먼저왔을때lastRemoteHashSeenTick을미리갱신하던코드이동.
  정확로컬slot확인뒤에만표시→로컬이따라잡으면동일원격값다시비교가능.
  resetHashComparison람다로ring/lastsent/lastseen/desync를함께초기화,
  두Game생성경로(새연결/재대전)모두호출.최신원격한쌍저장한계는별도유지.
  DESYNC breakdown은현재상태이므로captured_tick/compared_tick함께출력.
- root회귀:tests/learning/hash_arrival.cpp가실제main저장/비교블록을추출해검사.
  수정전선도착lastseen==0검사실패,수정후선도착대기→불일치비교→중복억제→reset통과.
  captured601/compared600stderr확인.게임/세션대역이므로전체GUI실행증거아님.
  tests/learning/hash_pair.cpp:11^11==29^29반례,새결합구분/역할순서일치,
  명시LE벡터hash(11,29)=954051100819889112.역할분기와reset두호출소스연결검사.
  before out/learning-jobs/092-before-main.cpp SHA256
  3791de74e21b6f40048d2a1f5b303667727cce1eab7ee7959606d8d3c28b2b75.
  core/hash.h·Part이전본도092-before-hash.h/092-before-part.md에보관.
- Part6§7:주기/4샘플이벽시각보장아님·선도착·최신저장한계·복구/권위구분교정.
  §16:mutex/atomic성능단정·타입크기와lock-free가능성오류교정.
  §17:역할순서결합으로재구성,XOR상쇄/독립uniform가정오류·wire호환성·초기0가치.
  §18:실제비교코드·캡처시점·단일원인단정·stderr기록보장오류교정.
  기존diagram/단축키/요약의옛XOR표현도일치갱신.전체상태복원완료로주장하지않음.
- 누적SCRIPTED111/SDL116 CTest·경고0·91기존파일변경범위확인.
  코덱/창ASan/UBSan:LE벡터·길이0..40·타입0..255·주기/범위·0/MAX해시·출력보존,
  선도착·동일/충돌·stale/too_far·역할별4조합·8샘플순서/창이동/reset.
  실제두프로세스(seed,D)=(0,2),(1,0),(77,2),(MAX,30)기대주기샘플모두일치.
  별도상대대역7경로:none/host비트/peer비트/missing4/early4mismatch/type/size검사.
  대역은호스트가만든stamp를에코하는규약검사이며독립시뮬레이션증거는앞4쌍이다.
  실제게임/양릴레이빌드·root20/19CTest. Windows/macOS네이티브·수동GUI미검증.
- 원문28/소스189,coverage344:covered3/partial106/unassigned235/needs-review0.
  Part발췌·스니펫7·Markdown27·454문제·전체차시DOM·정적재현ZIP통과.
  최종release **3be2dd9a43d9ab87**,15파일.
  HTTP18767 lessons.js6399934/library.js5373409바이트일치.
  로그092-check.log,092-regression.log,092-part-final.log,092-dom.log,092-site-final.log.
  통합검사92-hash-audit→check_learning_hash_audit.py연결.
- 남은별도:실제원격hashlatest-only/라운드ID없는stale메시지·결합규약협상,
  SEED단계/권한/운영상한,net_init전역bool,tcp_set_nonblocking오류전달,
  nativeDNS/connect취소,IOCP OVERLAPPED수명,INPUT충돌/부분저장,mainu32틱순환.
  전체보안완료아님.외부모델/빌드/검사종료,로컬HTTP만유지.자동승인거절없음.

### 91차시 완성 기록

- 2026-09-29: 직전90차시 턴은 progress. 이번91차시도 집필·실제 지연정책/Part 교정·검수로 progress.
  **로컬 1~91차시·449문제**, 공개 1~55차시·269문제 유지. 전체177차시 목표 미완료.
- 다음 시작점: **92차시 “해시 대조: 갈라진 상태 찾기”**.
  `91-input-delay`에서 누적한다.092.json/92체크포인트는 아직 없다.
  실제소켓은협상+입력교환+지연까지,최종해시는진단stdout비교이며HASH wire교환은아직없다.
  기준state_hash는Round별optional<uint64_t>,state_bytes로정확비교가능.
  host→left/peer→right고정정렬.새HASH메시지와동일틱비교/미래·과거·누락정책을작게연결한다.
  현재 Session::SendHash/GetLastRemoteHash,main hashRing·lastRemoteHashSeenTick와대조할것.
  main은600틱주기 hL^hR,마지막원격값하나와로컬링조회.같은틱/역할과초기화경계를검토한다.
  관련Part6§7/16/17/18. 단순hash일치는인증/충돌불가능/서버권위증명이아님.
- 공개55동결·기록이동재도입금지·수동GUI불필요·Codex하위에이전트금지유지.
  외부모델선택자료만 /tmp cwd·도구deny.커밋/푸시/배포없음.전체goal active.
- 91:14절·5문제·실제인라인구현7(기준C++5/현재main C++1/CMake1)·접힌전체파일4.
  지연/지터·생성/소비시계·음수상한·역할/설정·실패시번호보존·정확입력존재·
  도착공백시간선·창포화·유한꼬리/EOF·현재정책차이·실습/문제를집필했다.
- DeepSeek091-delay-{material.txt,events.jsonl,draft.json}:exit0,delayed_lockstep.h초안.
  승인된기준lockstep.h/tick_inputs.h와구체API명세만전송;키/설정/전체저장소제외.
  D<=30이면창초과를막는다는초안주석을시작범위조건/긴공백시포화가능으로교정.
- 기준net/delayed_lockstep.h:static create(Duel,Side,unsignedD,first=0),D0..30/role검증.
  ownsLockstep·localSide·delay·u64 next_capture. capture는mask축소전검증·u32소진검사,
  stored일때만생성순번증가;too_far/invalid는순번보존. receive/batch는반대Side만기록.
  local_limit=int64(next_capture)-1-D.생성은고정펄스마다소비성공과독립적으로호출.
  advance는기존exhausted/finished우선→로컬상한대기→정확한입력쌍조회/한틱소비.
  wire번호재지정없음.시간/소켓/락없고단일소유직렬호출.동적D변경API없음.
  창32포화시호출자가재시도/종료정책소유.생성끝/EOF/u32끝에서D꼬리자동배출없음.
- tools/delay_timeline.cpp:고정arrival max[0,1,2,2,2,5,6,7],각펄스로컬하나생성.
  next D0=[1,2,3,3,3,6,7,8],D2=[0,0,1,2,3,4,5,6].
  공백2펄스는D2기보유입력소비로메움.더긴공백에서는실제다음입력이없어대기검사.
  인위적논리시간선이며실환경RTT/GUI체감지연측정아님.
- tools/delay_probe.cpp:listen PORT SEED DELAY/connect PORT.협상Config{seed,0,D}.
  두쪽32입력capture후TYPE20두묶음16..31→0..15송신,halfclose후수신.
  EOF에서next=32-D인지검사,정상DONE pending=D.입력0누락은WAIT0→FAILED.
  두프레임작은통신진단이며paced지속GUI경기아님.기존90파일은CMake/README/DESIGN외동일.
- 실제src/main.cpp: safeTick=min(lastLocalSent,lastRemote)-D에서
  **min(lastLocalSent-D,lastRemote)**로수정.기존은상대최대번호가멈출때D예약입력을
  사용하지못함.로컬생성시계로소비상한을열고수신된정확한쌍이있는지별도확인유지.
  net/session.h공식주석동기화.입력번호/마스크wire는변경없고실행시점정책이달라짐.
- tests/learning/input_delay.cpp는실제main의상한계산3줄+소비루프발췌를읽어검사.
  수정전D2=[0,0,1,1,1,4,5,6]로회귀실패,수정후기준D2시간선일치.
  initialD2=-3/D255=-256 및u32상단부호연산검사.게임/Session은대역,전체GUI검사아님.
  before out/learning-jobs/091-main-before.cpp SHA256
  a0476b8c0f35f8dfd64e373507a3ccb0b05067f6389e979356844cec99808eeb.
  로그091-regression.log.원문이전본091-part-before.md도out에보관.
- Part4§11:FPS다른경우동일틱상태/벽시각진행구분·새공식·localmap[]대신쌍조회·
  매반복gameOver·INPUT payload정수시간으로정확성교정.
  Part6§5/10:새공식/숫자·기존정책반례·D0/D2표·지연/전체체감한계·음수초기상한.
  §11/요약의같은공식도갱신.heartbeat틱0누락원인을max번호가아닌정확조회실패로교정.
  Part4section13,Part6section07/12기존부분대응해시갱신;다른절전체완료주장없음.
- 누적 SCRIPTED109/SDL114 CTest·경고0.검사delay_contract ASan/UBSan:
  D0..30첫소비/31..255설정거절,invalidmask·생성순번보존,창포화/회복,remotebatch원자성,
  host/peer같은소비틱상태바이트직접실행일치,D0/2/30유한꼬리,긴공백/u32끝/종료우선.
  실제두프로세스(seed,D)=(0,2),(1,0),(77,2),(MAX,30)의역할별해시·소비수·꼬리일치.
  구멍/동일중복/충돌/범위/입력없음EOF 5경로통과.
  실제게임/양릴레이빌드·root20/19CTest. Windows/macOS네이티브미검증·수동GUI없음.
- 원문28/소스189,coverage344:covered3/partial102/unassigned239/needs-review0.
  Part발췌·실제스니펫7·Markdown27·449문제·전체차시DOM참조/저장/복사/탐색통과.
  최종메타데이터후library/coverage/lessons재생성·정적검사재실행.
  최종release **b4ac84619d18bd98**,15파일·재현ZIP.
  HTTP18767 lessons.js6345886/library.js5378249바이트일치.
  로그091-check.log,091-regression.log,091-part.log,091-dom.log,091-site-final.log.
  통합체크포인트디스패치91-input-delay→check_learning_delay.py연결.
- 기존남은점검:실제SEED단계/상대권한·운영상한·버전협상,net_init전역bool수명,
  tcp_set_nonblocking오류전달,nativeDNS/connect취소,IOCP OVERLAPPED수명,
  실제INPUT충돌/부분저장정책,main/Session의장기u32틱순환.전체보안완료아님.
  외부모델/빌드/검사모두종료,로컬HTTP만유지.자동승인거절없음.

### 90차시 완성 기록

- 2026-09-29: 90차시 집필·실제 main/Part 수정·검수 완료로 progress.
  **로컬 1~90차시·444문제**, 공개 1~55차시·269문제 유지. 전체177차시 목표 미완료.
- 다음 시작점: **91차시 “inputDelay와 safeTick: 지연을 흡수”**.
  `90-input-exchange`에서 누적한다.091.json/91체크포인트는 아직 없다.
  TickInputs 32칸/host-peer 입력 쌍/Lockstep 순차 소비가 준비됐다.
  현재 진단은 Config{seed,0,0}이며 추가 지연/카운트다운을 실행하지 않는다.
  현재 main의 min(lastLocalSent,maxRemoteTick)-inputDelay 소비 상한과 대조한다.
  번호 재지정과 소비 상한 정책을 혼동하지 말고, 최대 관찰 번호와 연속 입력 보유를 구분한다.
  이번 Part6§10 교정: D틱이 곧 총 체감지연/무정지 보장은 아님. 가변 도착 시간선으로
  실제 정지 동작을 검사해야 한다. 유한 진단에서 마지막 D개 미소비/종료 조건도 명시할 것.
- 공개55 동결·기록 이동 재도입 금지·수동GUI불필요·Codex하위에이전트금지 유지.
  외부모델은 승인된 선택자료만 /tmp cwd·도구deny로 전달한다. 커밋/푸시/배포 없음.
  이 턴 최초 사이드 UI 재작업 예고는 상태를 다시 읽은 뒤 철회: 이미 단색✓와
  이전 읽던 위치 수정 및 공개 완료임을 확인했고 기존 강의 집필을 이어 갔다.
- 90:14절·5문제·인라인 실제 구현8(C++7/CMake1)·접힌 전체파일4.
  누락/중립·논리적 틱·제한 창·최대번호/연속성·후보 원자성·조회/소비·canonical역할·
  종료틱/EOF·실제 main과 기준 차이를 설명한다. 검수환경 보고는본문에없다.
- DeepSeek090-lockstep-{material.txt,events.jsonl,draft.json}:exit0,두헤더초안.
  선택한simulation/input_mask.h,duel.h,intent.h와API명세만전송.이벤트step_start/text.
  소진 뒤 잔여 기록이 있다는 부정확한 주석, 입력 레코드와 네트워크 프레임 혼동 교정.
  통합 코드의 처음 API이름·state_bytes 접근자 오류는 본 빌드 전에 수정했다.
- net/tick_inputs.h:Side host/peer,Put stored/duplicate/conflict/stale/too_far/invalid/exhausted.
  optional<uint8_t>배열각32,next_ u64.마스크축소전검증,zero실제입력,정확슬롯조회.
  peek출력보존,consume두입력필요·한칸shift·끝칸reset.마지막u32틱뒤소진,wrap없음.
  put_batch전체후보검증/성공시적용,count1..32·nullptr·번호넘침·모든mask검사.
  같은pending만duplicate;소비한번호는stale.모든호출단일소유자직렬.
- net/lockstep.h:host→Duel.left/peer→right고정.번호소진/보드종료/입력누락은소비안함.
  두마스크해석후Duel후보tick성공시에만상태공개+한쌍consume.이번틱에서종료돼도
  advanced로기록하고다음호출finished.벽시계/추가지연/동시호출없음.
- tools/input_probe.cpp:협상후TYPE20(INPUT payload코덱재사용;기존echo1/2와다름).
  자신의0..11입력을저장하고6..11→0..5순서로두묶음전송,halfclose후수신계속.
  첫미래묶음에서WAIT next=0,전체입력후next=12.정상경계EOF+12틱이성공조건.
  duplicatepending허용;conflict/stale/범위/문법/누락EOF는실패.실패시전체경기rollback아님.
  작은고정2프레임진단이며지속게임은송수신interleave필요.수신무기한블로킹한계유지.
  GUI게임누적파일유지,새진단이기존순수Duel실행.네트워크GUI전체통합은아직아님.
- 실제 net/input_pair.h + src/main.cpp:로컬누락을0으로대체하던경로삭제.
  local/remote두조회성공시에만출력반영,상대조회는nonconsuming,출력변수별도.
  로컬값은callback전에복사하여iterator를callback넘어보유하지않음.
  catch-up while매반복두gameOver검사추가,한틱에서끝나면다음입력제출중지.
- 실제 main발췌fixture:수정전 missing=3 finished=3(exit1),수정후 missing=0 finished=1(exit0).
  before out/learning-jobs/090-main-before.cpp SHA256
  b8db17ba7f54b97b14d5786f1213f9aeba704bae76db5f8d3867e5779251787e.
  main전체GUI실행이아니라실제소비루프+게임/조회대역으로조건을검사했다.
  지속검사 tests/learning/{input_pair.cpp,lockstep_loop.cpp};후자는main블록직접추출.
  046.json의현재소스탐색symbol만새while조건으로갱신,안정스니펫변경없음.
- Part6§5:현재루프발췌·입력존재와중립·매반복종료가드·helper참고연결.
  §10:수신슬롯재지정주장교정,송신요청/원격수신구분,RTT→정확틱차이단정삭제,
  D=2면33ms총체감/무정지보장삭제,네트워크별적합성표를틱시간환산표로교체.
  초기maxRemoteTick0은입력존재증거아님,초기식min(-1,0)-2정정.
  두절부분대응기록,지연정책세부실행은91범위.
- 검사SCRIPTED106/SDL111 CTest·경고0·89기존파일변경범위확인.
  ASan/UBSan:mask0..511·누락/중립·창양끝·동일/충돌·묶음실패전체보존·u32소진·
  상태바이트불변/역할정렬·종료틱,현재helper조합/출력보존,실제main양쪽종료.
  실제두프로세스4시드(0/1/77/MAX)12틱동일역할해시일치·WAIT0,
  추가9경로(구멍/동일중복/충돌/잘못된타입/마스크/범위/입력없음/stale/잘림)통과.
  실제게임/양릴레이빌드·root20/19CTest. Windows/macOS네이티브미검증,수동GUI없음.
- 원문28/소스189,coverage344:covered3/partial102/unassigned239/needs-review0.
  Part발췌·8스니펫·Markdown27·444문제·전체차시DOM참조/저장/복사/탐색통과.
  최종metadata와주석뒤재생성,정적release **db56f20b30016404**,15파일·재현ZIP.
  HTTP18767 lessons.js6296870/library.js5376616바이트일치.
  로그090-check.log(두빌드),090-check-final.log(추가main검사/최종root),
  090-regression.log,090-part-final.log,090-dom-final.log,090-site-final.log.
- 기존남은별도점검:실제SEED단계/상대권한·운영상한·버전협상,
  net_init전역bool/프로세스수명,tcp_set_nonblocking오류전달,nativeDNS/connect취소,
  IOCP OVERLAPPED수명.실제INPUT재송충돌은첫기록유지이며기준의원자정책과다름.
  main/Session의장기u32틱순환도별도.전체보안완료를주장하지않는다.
  자동승인거절없음.외부모델/검사모두종료,로컬HTTP만유지.전체목표active.

### 89차시 완성 기록

- 2026-09-29: 직전88차시 턴은 progress, 이번89차시도 집필·실제 SEED/Part 수정·검수로 progress.
  **로컬 1~89차시·439문제**, 공개 1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **90차시 “입력 교환: 미래 틱을 기다리는 이유”**.
  `89-seed-handshake`에서 누적한다.090.json/90체크포인트는 아직 없다.
  HELLO10→OFFER11→ACK12로 공통 Config를 합의하고 Round 초기 상태 해시를 비교한다.
  countdown/input_delay는 합의만 하며 아직 실행하지 않는다. 새 Connection당 한 번인
  순차 협상이다. 입력 프레임 타입1/2 echo와 현재 게임 MsgType은 별도 규약이다.
  합의한 시드로 만든 양쪽 Round에 틱별 local/remote 입력을 보관·확보 후 소비하는
  작은 lockstep 경로를 만든다. 현재 SendInput/main simTick/localTickNext/inputDelay,
  Part6§5/10과 비교한다. UI·해시교차검증·대규모서버 전체를 선반영하지 않는다.
- 공개55 동결·기록 이동 재도입 금지·수동GUI불필요·Codex하위에이전트금지 유지.
  외부모델은 승인된 선택자료만 /tmp cwd·standalone·도구deny로 전달한다.
  전체 목표 complete 아님. 커밋/푸시/배포 없음.
- 89:14절·5문제·실제 인라인 구현9(C++8/CMake1)·접힌전체파일5.
  공통설정/개별역할·버전/규칙ID·순차상태·후보전체검증·제안과ACK전체일치·로컬/원격
  준비시점·카운트다운과벽시계·소유인계·기존Round초기화·현재프로토콜차이를 설명한다.
  검수환경보고는본문제외. 카운트다운/입력지연실행과실제게임입력동기화는아직없다.
- DeepSeek089-seed-{material.txt,events.jsonl,draft.json}:exit0,두헤더초안,
  이벤트 step_start/text만. API와선택학습헤더만 /tmp전송,키/설정/저장소전체제외.
  모델초안의sendhelper실패close책임주석교정. 진단통합코드의optional state_hash를
  정수로직접변환하던첫빌드오류는성공검사후역참조로수정하고전체검증했다.
- net/seed_protocol.h:Config{seed:u64,countdown_ticks:u32,input_delay:u8}.
  version:u16=1/rules:u32=1. HELLO10 payload6, OFFER11/ACK12 payload20=
  version2/rules4/seed8/countdown4/delay1/receiver_role1. receiver_role은Peer2.
  countdown<=600/delay<=30은학습정책이며현재게임운영제한으로적용한것아님.
  seed0은wire그대로,시뮬레이션이기본값으로정규화. encode/decode모든실패출력보존.
- net/seed_handshake.h:negotiate_host/peer순차절차,새로운배타Connection에서한번.
  host로컬제안검사→HELLO검사→OFFER송신→ACK형식/모든Config비교→출력반영.
  peer HELLO송신→OFFER검사→ACK송신→후보반영. local invalid연결/출력보존,
  network/protocol failure는close/출력보존. proposed/agreed alias허용(스냅샷).
  각송신2초협력예산,수신무기한블로킹/전체협상시간제한없음. EOF는실패.
  성공연결은계속열려있고후속프레임은호출자소유. post-ready중복/재대전FSM은없다.
- Connection::adopt(Socket&)추가. 연결된블로킹후보전제,active/invalid면후보보존,
  성공에만소켓이동/파서·방향상태초기화. 원시번호검사만으로연결/모드증명하지않는다.
- seed_probe listen PORT SEED/connect PORT:host는실제accept후adopt,
  두쪽합의후Round::create_seeded→optional state_hash검사→READY필드출력.
  role만host/peer로다름. 시드0/1/77/UINT64_MAX에서config와초기해시일치확인.
  초기해시는충돌가능진단신호이며정식인증/바이너리동일성증명이아님을본문명시.
- 실제net/session.cpp:초기acceptThread와SendNewSeed의wire role을로컬역할반대로전송.
  이전에는로컬Host를그대로보내상대도Host가될수있었음. 로컬역할은유지.
  SEED는정확14바이트및role1/2검사후에만seed/start_tick/delay/role/ready갱신.
  잘못된role기본값대체·여분바이트허용제거,정상재대전SEED반복허용유지.
  역할의송신의미가바뀌므로직결양쪽을함께갱신해야함.구버전의도자동판별불가.
- Part6§4/8/12:현재발췌·수신자역할·카운트다운은공통벽시계아님·현재ready가
  엄격한HELLO버전/설정ACK검증을뜻하지않음·SEED실패의전체상태보존교정.
  §12.6의덧셈이면언더플로방어라는오류를헤더검사후남은크기비교/덧셈오버플로주의로수정.
  enum임의기본값대체·fuzz가모든문법방어를증명한다는표현도적용범위로교정.
- 검사SCRIPTED104/SDL109 CTest·경고0·88기존파일변경범위확인.
  코덱ASan/UBSan:고정LE벡터·길이0~33·역할0~255·정책양끝/초과·0/MAX시드·출력보존.
  별도수명ASan:adopt거절보존/성공이동/busy보존,invalidlocal alias보존,닫힌연결실패.
- 실제두프로세스4시드초기상태해시/필드일치. host7/peer7잘못된단계/타입/버전/규칙/
  역할/값/길이/EOF에서FAILED unchanged=1 active=0. 전역완료시각이나전체시간상한은미검증.
- 실제Session ASan/UBSan:host초기/재대전rolePeer수신·로컬Host유지,client잘못된
  role/짧은/여분SEED후원래설정과ready보존,정상후속SEED허용. HASH장벽으로처리완료확인.
  수정전원본으로host초기수신자role검사실패,client설정보존CHECK실패확인.
  before파일out/learning-jobs/089-session-before.cpp,
  SHA256 8943b113e5227864164e14d2a9b387553522edb601693658f8a55bbdb1695d5d.
  rootHost공개API가요청포트만받아검사에서임시포트를반환후기동하므로포트선점경합은
  환경상남음(현재모든실행성공). 실제게임/양릴레이빌드·root20/19CTest통과.
  Windows/macOS네이티브미검증,수동GUI없음.
- 원문28/소스188,coverage344:covered3/partial100/unassigned241/needs-review0.
  Part6§4/12부분대응갱신,§8은재대전SEED역할만부분대응추가. 전체게임오버FSM완료아님.
  Part발췌·9스니펫·Markdown27·439문제·전체차시DOM참조/저장/복사/탐색통과.
  마지막준비시점표현/validation후library/coverage/lessons재생성,site최종재검사.
- 최종local release **f114f039428fc667**,15파일·재현가능ZIP.
  HTTP18767 lessons.js6241651/library.js5374318바이트일치.
  로그089-root.log/089-check-final.log/089-dom.log/089-site-final.log.
  최초089-build.log는optional해시변환오류로그이며성공근거는최종check로그다.
  check_learning_seed.py 및 통합체크포인트디스패치에89연결.
- 남은별도점검:실제SEED의경기단계/상대권한·운영상한·HELLO버전협상,
  net_init전역bool/프로세스수명,tcp_set_nonblocking void오류전달,native DNS/connect
  취소/시간제한,IOCP완료회수뒤OVERLAPPED수명. 문법검증을전체보안완료로해석하지않는다.
  자동승인거절없음. 외부모델/빌드/검사종료,로컬HTTP만유지.

### 88차시 완성 기록

- 2026-09-29: 직전87차시 턴은 progress, 이번88차시도 집필·실제 Session/Part 수정·검수로 progress.
  **로컬 1~88차시·434문제**, 공개 1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **89차시 “시드 협상: 두 게임의 시작을 맞추기”**.
  `88-connection-lifetime`에서 누적한다.089.json/89체크포인트는 아직 없다.
  Connection은 동기 loopback client로만 시작하며 소켓/파서/방향 플래그를 소유한다.
  수락측 소켓 인계나 양쪽 역할 협상은 아직 추가하지 않았다. current seedParams,
  HELLO/HELLO_ACK/SEED/MATCH_FOUND와 Part6§4를 대조해 동일 시작 조건의 확정을
  작은 프로토콜로 구현한다. TYPE1/2 echo wire와 실제 게임 wire를 구분한다.
  누적 게임/렌더러/오디오를 유지하고 lockstep 입력/해시 전체를 선반영하지 않는다.
- 공개55 동결·기록 이동 재도입 금지·수동GUI불필요·Codex하위에이전트금지 유지.
  외부모델은 승인된 선택자료만 /tmp cwd·standalone·도구deny로 전달한다.
  전체 목표 complete 아님. 커밋/푸시/배포 없음.
- 88:14절·5문제·실제 인라인 구현7(C++6/CMake1)·접힌전체파일4.
  핸들 소유/양방향 종료/요청 완료·후보 공개·멱등성·파서 먼저 배출·EOF/잘린프레임·
  모드 복귀 실패의 accepted 보존·명시적 재시작·thread종료/join·shutdown/참조해제.
  작성자 환경보고는 본문에 넣지 않고 review/validation에만 기록했다.
- DeepSeek088-connection-{material.txt,events.jsonl,draft.json}:exit0,두파일초안,
  이벤트 step_start/text만. API와선택학습헤더만 /tmp에서 전송. 키/설정/저장소전체 제외.
  Connection::ReadReport 누락수식·부분실패는위치불명이아닌접두사수락·실패가close한뒤
  재시작가능·deadline협력적한계·Runtime생성주체표현을 교정했다.
- 기준 net/connection.h/.cpp:Socket/FrameParser/send_closed/peer_eof 단일소유,
  비복사·비이동·직렬호출. Runtime은외부에서더오래생존. start는active면상태보존busy,
  connect후성공후보만이동. close는핸들reset→파서/방향초기화,반복가능.
  active는핸들소유만뜻하며두방향이종료돼도close전까지true다.
- send_frame은인코딩→논블로킹설정→send_bounded→블로킹복귀. 로컬호출오류는
  invalid_request로연결보존, OS/미완료/복귀실패는연결정리. 수락한길이는보고에보존.
  finish_sending은성공을기억해중복호출무OS성공, 수신방향유지. inactive는false/error0.
  next_frame은파서배출후최대16read. 완전경계EOF는캐시하고송신유지,꼬리EOF는truncated,
  parser오류/progressappend실패는protocol_error,OS오류는io_error후close. out은frame때만변경.
  연결수립/수신은블로킹무예산,다른스레드close취소불가,자동재접속/요청재실행없음.
- connection_probe PORT추가:tick42,count1,left 요청→2초송신→송신halfclose→
  TYPE2/필드/정확히한응답/EOF검증→close. 기존framing_probe서버와왕복한다.
  게임wire와별도진단. 기존87파일은README/DESIGN/CMake외동일.
- 실제Session hasUnjoinedWorkers():ath→qth→rth→th 순서단락평가.
  앞3워커는th를publish하므로joinable일때th를읽지않는다. Host/Connect/QueueJoin/
  RoomCreate/RoomJoin의상태리셋앞에서통일검사. 한소유자의start/Close직렬호출전제명시.
  원래Host는listening만검사,Connect무가드,QueueJoin은rth누락,Room은th를먼저읽음.
  실행완료/취소플래그만으로joinable스레드를덮어쓰지못하게하고Close뒤재시작허용.
  QueueJoin true는워커기동이며비동기연결실패는hasFailed로전달한다는헤더교정.
- Part6§4:다섯진입점/가드순서·소유자호출직렬화·동기Connect UI취소오해·thread생성후
  즉시CPU실행보장오해·native DNS/connect와Close join의무기한대기가능성 교정.
  Part7§11:QueueJoin/Cancel/Confirm/Decline 및 RoomCreate/Join 전체발췌범위보존갱신,
  QueueCancel은종료요청이고join/해제는Close라는주석동기화.
  Part12§7:shutdown이accept를EOF로깨운다는오류·reset이항상마지막참조라는오류·
  Close에서모든상태초기화한다는오류를수정. 수신/입력/틱초기화는새시작에도존재한다.
- 기준SCRIPTED101/SDL106 CTest·경고0. ASan/UBSan실제loopback검사:
  시작실패/상태보존busy·두프레임후EOF·EOF캐시후역방향응답·반복halfclose/close·
  송신종료후송신거절·꼬리EOF/길이0후새파서재연결·invalidframe연결보존·만료송신close.
- 실제Session8경로(진행5/실패한queue/room3)×새시작5를거절,반복Close/재연결2회검사.
  현재코드네이티브및ASan/UBSan통과. 수정전보관소스로8경로모두 !start assertion실패.
  프로세스std::terminate를관찰했다는뜻아님;미정리thread덮어쓰기의잠재결과는소스계약분석.
  before파일 out/learning-jobs/088-session-before.cpp,
  SHA256 6e4cd05159874f11064a5fe3f5334eee1422102eb74357961f7663821a6b61c1.
- connection_probe 실제echo4read상한·요청EOF후응답·누락/종류/값/중복/꼬리/길이0
  오류6종거절. 실제게임/양릴레이빌드·root20/19CTest통과.
  Windows/macOS네이티브미검증. 임의동시start/Close·네트워크단절시각의일반적종료상한
  보장이나TSan전체데이터경합검사를주장하지않는다.
- 원문28/소스188,coverage344:covered3/partial99/unassigned242/needs-review0.
  Part6§4/Part7§11/Part12§7부분대응추가. 전체경기프로토콜/서버동시성검토완료아님.
  Part발췌·7스니펫·Markdown27·434문제·전체차시DOM참조/저장/복사/탐색통과.
  마지막취소/초기화주석과validation후library/coverage/lessons재생성,site최종재검사.
- 최종local release **4412c1123798cf29**,15파일·재현가능ZIP.
  HTTP18767 lessons.js6189548/library.js5371108바이트일치.
  로그088-build.log/088-root.log/088-check-final.log/088-dom.log/088-site-final.log.
  check_learning_connection.py 및 통합 체크포인트 디스패치에88 연결.
- 남은별도점검:net_init전역bool/프로세스수명,tcp_set_nonblocking void오류전달,
  native DNS/connect 취소/시간제한,IOCP소멸자의완료회수뒤OVERLAPPED수명.
  자동승인거절없음. 외부모델/빌드/검사종료,로컬HTTP만유지.

### 87차시 완성 기록

- 2026-09-29: 직전86차시 턴은 progress, 이번87차시도 집필·실제 코드/Part 교정·검수로 progress.
  **로컬 1~87차시·429문제**, 공개 1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **88차시 “연결 수명: 시작·종료·재연결”**.
  `87-partial-send`에서 누적한다.088.json/88체크포인트는 아직 없다.
  소켓 RAII·Runtime·shutdown_send·부분 송신의 외부 효과·고정 deadline을 배운 상태다.
  연결 시작 실패/정상 종료/half-close/강제 정리/재연결 시 초기화할 상태를 작은 소유 객체로
  구체화한다. 실제 Session 시작/Close/worker수명·net_init/net_shutdown과 Part6§4,
  Part12 fd재사용 경합을 대조한다. 후속 lockstep/다중연결/IOCP전체를 선반영하지 않는다.
- 공개55 동결·기록 이동 재도입 금지·수동GUI불필요·Codex하위에이전트금지 유지.
  외부모델은 승인된 선택자료만 /tmp cwd·standalone·도구deny로 전달한다.
  전체 목표 complete 아님. 커밋/푸시/배포 없음.
- 87:14절·5문제·인라인 실제 구현7(C++6/CMake1)·접힌전체파일5.
  로컬수락/전달/상대처리·단일시도/전체작업·accepted 불변식·동기 차용·WouldBlock·
  무진행/전체deadline·협력적취소·모드변경 전제·실패후중복송신·WSS큐수락을 설명한다.
  호출부 스니펫과 CMake 연결을 본문에 추가하고 README/DESIGN을87내용으로 교체했다.
- DeepSeek087-send-{material.txt,events.jsonl,draft.json}:exit0,3파일초안,
  이벤트 step_start/text만. 선택API/spec만 /tmp에서 전달; 설정/키/저장소전체 전송 없음.
  알수없는enum 거절·WouldBlock뒤 fresh now검사·기존 io_chunk_size 재사용으로 교정.
- 기준 net/send_budget.h:SendAttempt(progress/would_block/interrupted/error)와
  SendReport(complete/timed_out/cancelled/error/invalid_result/invalid_request,accepted,error).
  size0은 콜백/포인터산술 없이 완료, 양수null은 invalid_request. progress는1..remaining.
  0/초과 count·불일치오류·unknown상태 거절. 부분 실패도 수락된 접두사를 보존한다.
  전체deadline 고정, 각시도 취소→시간 확인; blocked뒤 시계를 다시 읽고 min(남은시간,1ms)
  대기 요청. 마지막시도에서 완료하면 complete 우선. 신속한 비블로킹콜백 전제/협력적 한계 명시.
- net/send_socket.h/.cpp:모드변경결과 반환·POSIX다른flags보존·한번의try_send.
  POSIX EINTR는상위재시도, Winsock WSAEINTR는오류. 양수요청의0반환은합성오류,
  stale errno사용없음. Linux MSG_NOSIGNAL/macOS생성때SO_NOSIGPIPE.
  send_bounded는실제시계/대기/독립중단atomic_bool(relaxed)을연결;모드는변경하지않음.
- serialization_probe만 송신모드true성공확인→2초예산→보고검사→모드false성공확인.
  실패시소켓RAII정리,전체재송신없음. connect와블로킹응답수신은이예산밖이다.
  기본게임/렌더러/오디오/기존프레이밍계약은유지한다.
- 실제 net/socket.cpp tcp_send_all(native):무진행timer reset에서고정전체5초deadline으로변경.
  작은진행으로무한연장하던경로를막고 EINTR재시도도같은마감을검사한다.
  bool API와WSS분기는유지. 헤더에논블로킹전제·부분실패·계층별성공·스케줄링한계명시.
- Part6§1 현재발췌/시간제한, §13INPUT단독4.2KB추정전제/공유메모리상한아님/
  진행중INPUT임의폐기불가 교정. Part14§9전체deadline/공유프로세스자원/성공의범위 교정.
  Part12§8.2함수/설명교체. §8.1에도정확길이·knownmask·래핑없는틱구간과emplace보존반영.
  원문 대응344:covered3/partial96/unassigned245/needs-review0,문서28/소스188.
- 검사SCRIPTED100/SDL105 CTest·경고0·86누적파일변경범위확인. 정책ASan/UBSan:
  접두사진행/오류/취소/시간초과/인터럽트반복/invalid결과/0요청/마지막완료우선순위.
  실제loopback16MiB·작은송신버퍼·50ms예산으로역압유발,shutdown뒤상대가읽은
  접두사길이/내용정확일치(관찰127973바이트,이숫자는고정기대값아님).모드복원/사전취소검사.
- 실제tcp_send_all send대역:1.5초마다1바이트수락,전체5000ms·accepted4에서false확인.
  수정전HEAD실행회귀는이번에는하지않음. 실제root-stream 인터럽트/부분진행검사도통과.
  Winsock대역은실제87어댑터의핸들폭·int상한·ioctl실패·WSAEWOULDBLOCK/WSAEINTR/0검사.
  Windows/macOS 네이티브빌드나SDK증거아님. atomic동시취소경합스트레스검사도아님.
- typed echo4수신상한/7잘못된응답/CLI통과. 실제게임/양릴레이빌드·root20/19CTest통과.
  첫체크는병렬빌드완료전typed프로브파일없어서실패,완료후087-check-final.log는전부통과.
  새Windows검사는별도087-windows.log로통과하고메인스크립트에도연결했다.
- 통합 check_learning_checkpoints.py에빠진85/86/87을각전용검사로연결했다.
  Part발췌·실제스니펫7·Markdown27·429문제·전체차시DOM참조/저장/복사/탐색통과.
  완료✓브라우저저장과이전읽던위치가현재hash를피하는기존동작도소스/DOM검사로확인.
  validation만최종추가후library/coverage/lessons재생성,site재검사.
- 최종local release **ad853737ee58a51c**,15파일·재현가능ZIP.
  HTTP18767 lessons.js6137215/library.js5366592바이트일치.
  로그087-build.log/087-check-final.log/087-windows.log/087-dom.log/087-site-final.log.
- 남은별도점검:net_init전역bool/수명,tcp_set_nonblocking void오류전달,IOCP소멸자의
  제한된완료회수후OVERLAPPED수명. 전체운영보안완료아님.
  자동승인거절없음. 외부모델/빌드/검사종료,로컬HTTP만유지.

### 86차시 완성 기록

- 2026-09-29: 직전85차시 완성 턴은 progress, 이번86차시도 집필·실제 코드/Part 교정·검수로 progress.
  **로컬1~86차시·424문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **87차시 “부분 송신: 끝까지 보내기의 실패 조건”**.
  `86-serialization`에서 누적한다.087.json/87체크포인트는 아직 없다.
  기준StudySocket은블로킹·StreamResult progress/eof/error,실제진행량으로전진한다.
  framing_probe/typeecho와serialization_probe(기존서버에typedpayload왕복)가있다.
  단순성공루프를넘어부분진행뒤실패·재시도범위·이미수락된접두사·상대응답의차이,
  nonblocking WouldBlock/보류송신·시간상한/취소조건을실제tcp_send_all/send_some와대조한다.
  88연결수명전체를선반영하지말고누적게임/렌더러/오디오분리를유지한다.
- 공개55 동결·기록 이동 재도입 금지·수동GUI불필요·Codex하위에이전트금지 유지.
  외부모델은승인된선택자료만standalone·도구deny로전송한다.다음호출은/tmp cwd사용.
  전체 목표 complete 아님. 커밋/푸시/배포 없음.
- 86:14절·5문제·실제스니펫7·접힌전체파일4.객체/wire크기·필드폭·LE자리·
  차용커서/남은범위·필드실패vs메시지후보·변환전검사·정확한count·마스크/틱구간·
  고정벡터vsroundtrip·연결정책분리.작성자환경보고는본문제외.
- DeepSeek086-codec-{material.txt,events.jsonl,draft.json}:exit0,2헤더초안,API/spec만.
  event step_start/text만,도구실행없음.이번실행은자료생성과같은repo cwd에서
  standalone·도구deny로수행(AGENTS없음);후속은기존/tmp방식유지.
  max매크로내성·구조/의미검증주석·후보실패보존설명교정.
- 기준net/byte_codec.h:ByteReader/Writer가포인터/extent를차용,position<=extent.
  u8/u16/u32/u64를완전필드크기확인뒤LE조립,실패시커서/출력/저장소보존.
  null0은빈범위,양수null은invalid.호출자실제저장소크기/수명전제,실패는sticky아님.
- 기준net/input_codec.h:InputBatch{first_tick:u32,masks[16],count:size_t}.
  count1..16→u16축소,remaining==count,knownmask0x1F,래핑없는구간.
  Frame후보/배치후보로전체성공후출력갱신.외부TYPE1/2진단이며게임INPUT4와구분.
- serialization_probe:기존framing_probe서버에first_tick0x01020304,count3,masks01/00/10
  송신,동일payload를값으로복원검증후halfclose/EOF.틀린문법/종류/내용/꼬리거절.
  샘플큰틱은바이트순서용이며경기정책검증아님.블로킹loopback/한연결/인증·시간제한없음.
- 실제net/input_message.h추가:InputBatchView는기존payload차용.
  INPUT헤더6·payload≤4096·count>0·남은길이정확일치·count-1<=UINT32_MAX-first·
  모든입력비트를확인한뒤out갱신.현재구문최대4090,서버ranked의더작은개수정책별도.
  Session은검증후에만입력큐/틱상태를바꾸고기존거리/큐상한도적용.
  기존from=0xFFFFFFFE,count4는뒷틱0/1로래핑해거리검사를통과할수있었음.
  늦은unknownmask나여분바이트도이전코드가허용.새코드는배치전체드롭,연결즉시종료는아님.
- 실제net/framing.cpp le_read_i32추가,Session MATCH_RESULT3필드에사용.
  C++17unsigned범위밖signed변환의구현정의의존제거.음수는 -1-(UINT32_MAX-raw).
  N4659 conv.integral 원문확인,wire비트/범위유지.각readhelper폭의caller전제명시.
- tests/input_message_test.cpp를CMake/CTest추가.원시decode상태보존/경계와signed끝점검사.
  tests/learning/session_payload.cpp는실제SessionpublicAPI+후행HASH123/456장벽으로
  앞INPUT처리완료후큐를확인한다.정상/래핑/unknown/여분/0count/결과signed6경로.
- 수정전HEAD Session 소스를현재소켓/API에링크해같은회귀의wrap/unknown/trailing3종
  실패(exit1)확인.소스out/learning-checkpoints/86-serialization-check/session-before.cpp
  SHA256 3f9110bd7df7480914bea30e96e6ea64616c49f3ae3cb8aad0d356d3e4a13e3d.
- Part6§2.8필드폭/전체payload검증추가,LEhelper/handleFrame발췌와§12INPUT교정.
  Part12§8.1검증순서·거리검사단독으로래핑을막는다는오해교정,발췌갱신.
  ByteReader는기준예제이며root전체rawreader가이타입으로교체된것은아님.
- check_learning_serialization.py최종:SCRIPTED99/SDL104CTest·경고0·85누적파일동일.
  고정LE벡터·u16전체65536값·u64최상위/양끝·필드/메시지실패보존·null/0·
  크기/개수/늦은비트오류·틱구간 ASan/UBSan.실제INPUT전마스크256/최대payload/signed경계.
  실제typed echo4수신상한·잘못된응답7종·CLI6종,실제Session6경로통과.
  실제게임/양릴레이빌드·root20/19CTest통과.Windows/macOS네이티브미검증·수동GUI없음.
- 원문28/소스188,coverage344:covered3/partial94/unassigned247/needs-review0.
  Part6프레이밍부분대응갱신·Part6악성프레임/Part12DoS절에INPUT부분대응추가.
  다른메시지/전체운영보안/나머지Part검수완료라는뜻아님.
- Part발췌·스니펫7·Markdown27·424문제·전체차시DOM참조/저장/복사/탐색통과.
  최종참조캡션/보기문구·validation메타데이터후library/lessons재생성,최종site재검사.
  local release **f56db208bae70d6c**,15파일·재현가능ZIP.
  HTTP18767 lessons.js6081374/library.js5364566바이트일치.
  로그086-build.log/086-check-final.log/086-dom.log/086-site-final.log.
- 남은별도점검:net_init전역bool/수명,tcp_set_nonblocking void오류전달,IOCP소멸자의
  제한된완료회수후OVERLAPPED수명.체크섬/LEN0복구정책유지,전체보안완료아님.
  자동승인거절없음.외부작업/빌드/검사종료,로컬HTTP만유지.

### 85차시 완성 기록

- 2026-09-29: 직전84차시 완성 턴은 progress, 이번85차시도 집필·실제 코드/Part 교정·검수로 progress.
  **로컬1~85차시·419문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **86차시 “직렬화: 크기·엔디언·범위 검증”**.
  `85-framing`에서 누적한다.086.json/86체크포인트는 아직 없다.
  현재 학습 wire는 LEN:u16LE + TYPE:u8 + PAYLOAD(최대32), 체크섬 없음.
  게임 wire와 호환되지 않는 통신 진단이다. 단순 길이 접두사의 byte조립을 배웠으므로,
  다음에는 필드별 폭·엔디언·읽기 전 범위·정수 변환/유효값을 실제 메시지와 연결한다.
  Part6 직렬화 및 현재 net/framing helpers/Session payload검사와 대조하되87부분송신
  전체내용을 선반영하지 않는다. 게임/렌더러/오디오 누적 상태를 유지한다.
- 공개55 동결·기록 이동 재도입 금지·수동GUI불필요·Codex하위에이전트금지 유지.
  단순 구현은 승인된 선택 자료만 DeepSeek /tmp cwd·standalone·도구deny로 전송하고 검토.
  전체 목표 complete 아님. 커밋/푸시/배포 없음.
- 85:14절·5문제·실제스니펫7·접힌전체파일4. LEN정의·자료형별size·접두사소비·
  파서3결과·부분헤더/본문·다중프레임·고정실패·소유복사·공간증명·EOF/의미완료분리.
- DeepSeek085-framing-{material.txt,events.jsonl,draft.json}:exit0,2파일초안,선택API/spec만.
  append가 하위buffer실패를무시하던오류·없는runtime헤더·std::min매크로내성·
  Frame꼬리0/보류바이트주석의범위교정. 사용자설정/키/저장소전체전송없음.
- ByteBuffer::consume_front는초과거절/나머지앞으로이동/used갱신.84체크포인트변경없음.
  FrameParser:fixed64, LEN1..33/전체3..35, need_more/frame/error,오류고정/연결단위생성.
  완성분drain후tail≤34/read≤16→append후≤50. 출력은완성후갱신,포인터로반환하지않음.
- framing_probe:listen PORT READ_CAP/connect PORT CHUNK,범위1..16/엄격CLI.
  요청ABC·빈·00FF2A→응답3개내용/순서검증→클라이언트송신종료→서버EOF/송신종료.
  응답은요청EOF전송신,추가/누락/잘린응답거절.블로킹loopback한연결,인증/시간제한없음.
- 실제net/framing.cpp:offset≤size불변식으로remaining=size-offset비교,wire정책불변.
  기존LEN0/잘못된checksum은완성분소비후스킵,unknownTYPE출력;과대LEN만false.
  parse_frames는out누적·오류전정상prefix유지,호출자정책과구분해헤더계약명시.
- 실제Session::parseReceived공통경로추가.기존4곳이parse_frames(false)를무시했다.
  io/room/queue/lobby모두반환확인,실패배치frames폐기,ready/connectedfalse·quittrue·
  sockMu하shutdown·connectionFailedtrue.룸실패상태설정.워커에서Close/join안함.
  기존wire성공동작·앞선호출에서처리된프레임은되돌리지않음.
- tests/framing_test.cpp실제parser회귀를CMake/CTest에추가.
  tests/learning/session_framing.cpp는실제SessionpublicAPI로네경로진입.
  tests/learning/framing_parity_probe.cpp는실제C++생성/파싱을Python과직접비교.
- Part6:체크섬범위/헤더연속저장·unsigned불변식·동적할당·고정벡터vs실제교차비교교정.
  Part6/7Session발췌갱신,Part8예외가종료강제한다는단정·검사범위교정.
  Python미러docstring의lockstep복구암시삭제·고정벡터검사주석정확화.
- check_learning_framing.py:SCRIPTED97/SDL102CTest·경고0·기존누적파일동일검사.
  기준128분할/출력보존/최대프레임/초과소비/null/상한 ASan/UBSan.
  실제root32768분할/빈/최대/제로길이/checksum/unknownTYPE ASan/UBSan.
  실제두프로세스16조합·EOF전응답·최대/바이너리/빈·잘못된요청5/응답6·CLI9.
  실제Session4경로:상대연결유지중2바이트과대LEN으로실패/연결종료확인.
  C++/Python32880직접비교·unknownTYPE(C++출력/Python드롭)차이명시검사.
  저장소.venv Python고정벡터검사·실제게임/양릴레이빌드·root19/18CTest통과.
- Windows/macOS네이티브미검증.85새알고리즘은OS호출추가없으며83/84계약위에구성.
  수동GUI없음.이전대역검사를Windows네이티브증거로확대하지말것.
- 남은별도점검:net_init전역bool/수명,tcp_set_nonblocking void오류전달,IOCP소멸자제한된
  완료회수후OVERLAPPED수명.체크섬/LEN0의게임진행복구정책은이번에변경하지않음.
  전체보안완료아님. Part6의33ms지연도입등전체감수완료아님.
- 원문28/소스185,coverage344:covered3/partial92/unassigned249/needs-review0.
  Part발췌·스니펫7·Markdown27·419문제·전체차시DOM참조/저장/복사/탐색통과.
  validation메타데이터최종화후library/lessons재생성(본문/렌더러변경없음),최종site재검사.
- 최종local release **01c9c3bfac0ab08f**,15파일·재현가능ZIP.
  HTTP18767 lessons.js6026918/library.js5348772바이트일치.
  로그085-build.log/085-check.log/085-dom.log/085-site-final.log.
  자동승인거절없음.외부작업/빌드/검사종료,로컬HTTP만유지.

### 84차시 완성 기록

- 2026-09-29: 직전83차시 완성 턴은 progress, 이번84차시도 집필·실제 코드/Part 교정·검수로 progress.
  **로컬1~84차시·414문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **85차시 “프레이밍: 길이 접두사와 부분 수신”**.
  `84-tcp-stream`에서 누적한다. 085.json/85체크포인트는 아직 없다.
  현재는 블로킹 EOF 기반 한 묶음의 최대64바이트 요청/echo. ByteBuffer에는 append만 있고
  소비/프레임 파서는 없다. Part6 §2와 실제net/framing을 대조하며 부분헤더·부분본문·
  연속프레임·길이상한·완성된프레임만소비하는계약으로 확장한다.86직렬화와87송신실패
  전체내용을한꺼번에선반영하지말고학습순서를유지한다. 게임통합전통신진단이라는위치명시.
- 공개55 동결·기록 이동 재도입 금지·수동GUI불필요·Codex하위에이전트금지 유지.
  단순 구현은 승인된 선택 자료만 DeepSeek /tmp cwd·standalone·도구deny로 전송하고 검토.
  전체 목표 complete 아님. 커밋/푸시/배포 없음.
- 84:13절·5문제·실제스니펫8·접힌전체파일5. 메시지/호출/세그먼트, 용량/진행량/사용량,
  포인터/남은길이·범위제한후변환·누적불변식·실패상태보존·EOF/반쪽종료·검사범위설명.
- DeepSeek084-stream-{material.txt,events.jsonl,draft.json}:exit0,4파일초안,step_start/text만.
  Windows전용int길이설명·max매크로내성·스레드원자성표현·송신로그누적위치·오류코드·CLI교정.
- 기준net/stream.h/.cpp:StreamStatus progress/eof/error,StreamResult count/error.
  양수요청만허용,유효한블로킹Socket과버퍼차용. 요청길이INT_MAX제한후변환.
  POSIX EINTR만재시도,Windows취소는오류보고. 비어있지않은send0은합성오류,오래된errno안읽음.
  shutdown_send는SHUT_WR/SD_SEND로송신만종료,Socket소유권·수신방향유지.
- ByteBuffer<Capacity>:std::array+used, count>Capacity-used검사후전체복사. 실패시내용/size보존.
  append(nullptr,0)허용,양수null거절. 스레드원자성과무관.고정64는진단상한이지프로토콜상한아님.
- stream_probe:listen PORT READ_CAP(1..16),connect PORT MODE(one/two/bytes).
  같은ABCDEF를6/3+3/1씩제출하되send_block은실제진행만큼전진.클라이언트송신종료후응답수신,
  서버요청EOF뒤누적echo+송신종료.최종VERIFIED6.분할모양비결정적·한연결·loopback·인증/시간제한없음.
  누적게임/렌더러/오디오/한바이트진단유지.실패입력/64초과는오류종료.
- 실제net/io_size.h:Windows송신size_t→int를INT_MAX제한후변환.두송신함수사용.
  tcp_send_some은후속오류/0반환때도앞선진행량out_sent보존.실제호출자들은실패시연결종료유지.
  tcp_recv_some은POSIX EINTR재시도,WouldBlock만true무진행,EOF/나머지오류false.
- 공식Winsock표확인:WSAEINTR는호출취소.현재send_some/send_all/recv_some과기준stream에서
  자동재시작제거. WSAEINPROGRESS를정상데이터없음으로분류하던것도제거.
  83Socket도같은교정,83강의v1.0.1.83/84 net/socket.h/.cpp바이트동일·수정후누적검사재실행.
- Part6/12송수신발췌·Part14주석갱신. true=상대생존단정제거,바이트/메시지경계표현교정,
  부분진행·정수범위·half-close·수신상한/누적상한/시간상한구분추가.33ms지연도입등전체Part감수완료아님.
- check_learning_tcp_stream.py 최종:SCRIPTED95/SDL100CTest·경고0·소스누적비교,
  기존1바이트프로세스회귀·실제3×4통신조합12·빈/바이너리/64/65바이트·잘못된응답3·CLI오류9종.
  전체32분할·추가실패보존·null/0/size_t최댓값·반쪽종료응답 ASan/UBSan.
  실제socket_stream.cpp syscall대역 ASan/UBSan·root18/17CTest·실제게임/양릴레이빌드·골든유지.
- check_learning_tcp_stream_windows.py:실제root송수신3함수발췌와기준stream.cpp를API대역컴파일.
  64비트핸들·큰길이제한·취소1회보고·WSAEINPROGRESS/WouldBlock확인.거대버퍼역참조없음.
  Windows SDK/네이티브·macOS미검증.수동GUI없음.이대역검사를네이티브증거로쓰지말것.
- 남은별도점검:net_init전역bool/수명,tcp_set_nonblocking void오류전달,IOCP소멸자제한된
  완료회수후OVERLAPPED수명. Windows송신길이축소는이번교정완료.전체보안완료아님.
- 원문28/소스183,coverage344:covered3/partial92/unassigned249/needs-review0.
  Part발췌·스니펫8·Markdown27·414문제·전체차시DOM참조/저장/복사/탐색최종통과.
- 최종local release **67471d674df63b4b**,15파일·재현가능ZIP.
  HTTP18767 lessons.js5966562/library.js5334033바이트일치.
  로그084-check-final.log/084-windows.log/084-dom-final.log/084-site-final.log.
  자동승인거절없음.외부작업/빌드/검사종료,로컬HTTP만유지.

### 83차시 완성 기록

- 2026-09-29: 직전82차시 완성 턴은 progress, 이번83차시도 집필·실제 소켓/Part 교정·검수로 progress.
  **로컬1~83차시·409문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **84차시 “TCP 스트림: 메시지 경계가 없는 이유”**.
  `83-sockets`에서 누적한다. 084.json/84체크포인트는 아직 없다.
  83은 game 파일을 그대로 둔 독립 socket_probe의 블로킹 loopback1바이트 왕복이다.
  Runtime→Socket 수명·move-only·주소/포트·accept 별도 핸들·EOF·오류 보존을 다뤘다.
  84에서는 write/read 호출 경계가 보존되지 않는 이유와 누적/부분 진행을 작은 실제 실험으로 다룬다.
  다음85의 길이 접두사 완성 프레이밍을 선반영하지 말고 게임 통합 위치를 계속 명확히 한다.
- 공개55 동결·기록 이동 재도입 금지·수동 GUI 불필요·Codex 하위 에이전트 금지 유지.
  DeepSeek는 승인한 선택 자료만 /tmp cwd·standalone·도구deny로 호출하고 직접 검토한다.
  전체 목표 complete가 아니며 이번에도 커밋/푸시/배포 없음.
- 83:13절·5문제·접힌 전체파일4개·실제 구현 발췌13개.
  프로세스 주소 공간·핸들/끝점/연결·리스너/수락 소켓·move/RAII·Winsock 성공 참조·
  네트워크 바이트 순서·오류 즉시 보존·인터럽트의 호출별 처리·SIGPIPE·recv길이0/내용0·
  send수락/응답·입력 전체 파싱·검사의 준비 신호를 설명했다.
- DeepSeek083-socket-{material.txt,events.jsonl,draft.json}: exit0,3파일 초안,step_start/text만.
  미정의 Native·POSIX음수·close 전 무효화·backlog 총접속자 단정·NRVO·send0오류 교정.
- 기준 net/socket.h/.cpp,tools/socket_probe.cpp,tests/socket_contract.cpp와 CMake 추가.
  Runtime 먼저/Socket 나중 선언, 소멸 반대순서. 단일 스레드 소유·복사 금지.
  listen127.0.0.1:0→실제포트 출력→accept한 번→listener정리→1바이트 echo.
  Windows배타바인드, LinuxMSG_NOSIGNAL, macOSSO_NOSIGPIPE. 인증/제한시간/게임통합없음.
  EOF false+eoftrue+error0,실행오류1/명령오류2. 82게임·렌더러·오디오 파일 바이트 유지.
- 실제 net/native_socket.h:POSIX int/-1,Windows uintptr_t/모든비트1.
  TcpSocket 공유소유자·생성/수락/송수신·Reactor인자·IOCP맵/세트/재무장목록·서버Conn 전파.
  실제SOCKET 크기/무효값 static_assert. 핸들0유효·int축소제거.
- 실제 make_owned:무효핸들 저장소+제어블록 먼저 준비한 뒤 실제핸들전달.
  bad_alloc 두지점에서 close1회,shared_ptr 실패deleter의 중복close회피.
  Windows SO_EXCLUSIVEADDRUSE/POSIXSO_REUSEADDR 분기·옵션실패정리.
  accept/connect 논블로킹설정실패는정리후실패,acceptOk는소유권성공후보고.
- Part6/12/14:발췌갱신·accept와handshake순서·핸들축소정당화삭제·주소재사용OS차이·
  준비성→recv데이터성공단정교정·StreamTransport명칭수정. 원문은여전히부분대응.
- scripts/check_learning_sockets.py:SCRIPTED93/SDL98CTest·경고0·누적파일비교,
  실제두프로세스42왕복·raw0/128/255·EOF·접속거절·잘못된CLI9종,
  move/목적지교체/descriptor0/닫힌상대송신 ASan/UBSan.
  실제socket.cpp 두할당실패·공유소유자·바인드옵션실패·accept/connect모드실패 주입.
  실제릴레이/Reactor릴레이빌드·root18CTest·sim_hash_dump골든,실제게임빌드·root17CTest.
- Windows 공개헤더64비트핸들/0/sentinel/Reactor시그니처 정적검사. 네이티브Windows/macOS미검증.
  수동GUI/스크린샷없음. 이 타입검사는Windows 소켓 런타임/IOCP 실행 증거가 아니다.
- 후속점검메모: 기존 net_init 전역bool/수명, tcp_set_nonblocking void의 오류전달,
  Windows send size_t→int길이, IOCP 소멸자 제한된완료회수 후 OVERLAPPED 수명은별도검토필요.
  IOCP 종료 UAF 가능성은 이번에 재현·수정하지 않았으므로 확정된 해결로 기록하지 않는다.
- 원문28/소스182,coverage344:covered3/partial91/unassigned250/needs-review0.
  Part발췌·구현스니펫13·Markdown27사례·409문제·DOM전체차시/참조/저장/복사통과.
- 최종local release **4ef0662164317152**,15파일·재현가능ZIP.
  HTTP18767 lessons.js5902348/library.js5329260바이트일치.
  로그083-check.log/083-root-build.log/083-root-game-test.log/083-dom.log/083-site-final.log.
  자동승인거절없음. DeepSeek/빌드/검사모두종료,로컬HTTP만유지.

### 82차시 완성 기록

- 2026-09-29: 직전81차시 완성 턴은 progress, 이번82차시도 정책 구현·실제 코드/원문 교정·검수로 progress.
  **로컬 1~82차시·404문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **83차시 “소켓: 프로세스 사이의 바이트 통로”**, 새 네트워크 단원 시작.
  083.json/83체크포인트는 아직 없다. `82-audio-failure`의 누적 게임을 출발점으로 삼고,
  root net의 소켓 생성/수명/플랫폼 오류·Part6 설명을 대조한다. TCP 스트림의 메시지 경계는
  84차시에서 다루므로83은 프로세스·연결·주소/포트·소켓 자원·양끝의 역할을 실제 작은 통신으로
  구성할 것. 독립 진단을 쓰면 누적 게임과 합치는 지점/아직 연결하지 않은 범위를 명확히 한다.
  Linux 서버·Windows 이전·웹/네이티브 클라이언트 맥락을 유지하되 한 차시에 완성 네트워킹을 복사하지 않는다.
- 공개55 동결·학습 기록 이동 재도입 금지·수동 GUI 불필요·Codex 하위 에이전트 금지 유지.
  단순 구현은 승인된 선택 자료만 OpenCode/DeepSeek에 보내 직접 검토. 전체목표 complete 아님.
- 82:13절·5문제·접힌 전체파일3개·짧은 실제 정책/메인 발췌9개.
  기능 상태/요청 결과, 선택 기능/내부 불변식/로더 실패, 상태기계, 템플릿 의존성 주입,
  if constexpr의 버려지는 가지, 부분 정리·C++ 예외 경계·고정 실패 보고 저장소,
  한 번의 사건 소비·pending/seen 분리·고정 상한·공유init/COM의 다른 의무·실패 주입을 설명.
  동일 입력/dt에서의 규칙 독립성과 실제 프레임 CPU/장치 지연을 구별한다.
- `presentation/sound_policy.h`: BasicSession<Device,Supported>가 Device를 소유.
  unprepared에서 prepare1회. 미지원disabled(통지없음), 성공ready, 실패unavailable.
  준비실패는 Device.close 후 상태고정, 반복prepare는 장치를 재호출하지 않음. 새Session이 새시도.
  CueFactory 함수포인터의 기본은 make_cue.4자리중 nullopt→clip, 설치거절→install,
  open거절→open, bad_alloc→memory, 기타예외→unexpected. nullfactory는 open없이clip실패.
  Device기본생성/play/stop/close는 비예외 계약; 정책이 OS/메모리 오류 전반을 복구하지 않는다.
- send는 종류검사를 먼저하고 잘못된값 invalid_kind. 정상종류라도 준비전/무음/실패상태는
  skipped+장치무호출. ready에서 play거절은 failed·상태ready 유지, 다른 새사건은 시도 가능.
  started는 API수락이며 청취 보증 아님. ready는 준비 이력이며 장치 건강 모니터가 아님.
  stop은 ready재생만 중단·PCM/상태보존. play(bool)는 기존probe호환 래퍼.
- Failure7종 seen+pending 고정배열. 최초종류만pending, take_notice는 enum순서로pending만
  비우고 seen은 유지. 시간순 이력/발생횟수/시간창요청제한 아님. 각종류 통지최대1회.
  main의 report_audio_notices가 worker밖에서 stderr출력. NONE빌드의 거짓장치경고 제거.
- drain(Batch&,Sink&)는 take 후 send를1회, 성공/건너뜀/실패 모두소비. DeliveryReport는
  한Batch최대24라 누적overflow없음. Sink의 send는 비예외·열거한결과 계약.
  malformed FrameReport는 Batch::from 거절→main오류종료 유지; 선택기능실패로 삼키지 않음.
- sound_session.h는 실제SDL/XAudio/NoAudioDevice의 타입별칭만 선택. NoAudio에 메서드없음.
  DeepSeek의 미지원Device에도 play를 요구하던분기를 if constexpr로교정했다.
  Device할당/I/O가 없다는 설명과 close가 단지best-effort라는설명도 실제수명계약으로교정.
- DeepSeek082-failure-{material.txt,events.jsonl,draft.json} exit0,2헤더 초안.
  승인된 cue/PCM/Batch 발췌+정책명세, standalone/도구deny.이벤트step_start/text·도구호출0.
  이번 실행cwd는저장소였음.후속외부초안은 /tmp cwd를명시해 주변컨텍스트유입을최소화할 것.
- root/학습공통 core/once_flags.h 바이트동일. Flags<N>은유효인덱스의첫take만true,
  고정bool배열·reset·무할당·카운터없음.메인스레드전용이며스레드동기화수단아님.
- 실제audio/audio.cpp: SFX생성/제출시작, BGM생성/제출시작 네단계최초실패만stderr진단.
  첫audio_init수명에reset,이미공유중인추가init은reset안함.후속새재생은계속시도하고실패정리유지.
  이가드는모든로그나음원로드오류를제한하지않고,같은단계의나중HRESULT도해당수명에는생략.
- check_learning_audio_failure_root.py: legacy_audio_failure_logs.inc 고정before함수,
  실제Windows전체백엔드+MP3로더+API대역 ASan/UBSan.2수명1200실패요청에before1200줄,
  after8줄(4단계×2수명).공유init·실패후성공요청·PCM해제·COM균형유지.
  실제Windows소스를대역헤더로 -Wall/-Wextra/-Wpedantic 추가컴파일,경고0.네이티브증거아님.
- tests/game_wrapper_test.cpp에실제Game init/all-load/play실패3모드×24seed×240tick추가.
  SimGame상태해시동일·사건플래그소비·공유자원정리검사.17280tick ASan/UBSan통과.
- 학습audio_failure_contract:모든4자리factory nullopt/bad_alloc/기타예외·설치실패·open실패,
  반복준비제한·무음빌드·잘못된종류·1000재생실패의최초통지·stop보존·불변식거절검사.
  실제학습Game+정책에서3모드×24seed×180frame=12960,같은dt/입력의해시와잔여시간,
  실패해도Batch소비·다시drain0·건수합/상한비교.상태해시비교를실행시간동일성으로해석하지않음.
- check_learning_audio_failure.py 최종exit0:Release SCRIPTED91/SDL96CTest·누적게임·경고0,
  SDL/NONE·SCRIPTED/SDL구성·XAUDIO2비Windows거절,정책/XAudio/Session ASan/UBSan,
  실제main XAudio선택대역컴파일,root17CTest·sim_hash_dump골든유지.통합체크포인트분기추가.
- Part5 §10재검토/§10.5추가,관련init/play발췌최신화.절대크래시방지/자동장치복구/콘솔없으면
  자연무시단정과옛파일로그수정.선택기능요구·불변식실패·로그표시경로·장치복구미구현범위명시.
  SDL장치사건/Microsoft OnCriticalError공식계약대조.실제src/audio/platform에복구콜백없음확인.
  네이티브Windows·스피커·장치제거/복구미검증.사용자요청대로수동GUI/스크린샷없음.
- 원문28/소스181,coverage344:covered3/partial90/unassigned251/needs-review0.
  Part발췌·정책스니펫9개·Markdown27사례·404문제·DOM전체차시/탐색/저장/참조/복사통과.
- 최종local release **46812cf73be285ed**,15파일·재현가능ZIP.
  메타데이터최종갱신후library가오래됐다는export검사가1회실패;library/lessons둘다재생성후통과.
  HTTP18767 lessons.js5833770/library.js5324750바이트일치.공개release708d2e519c17c011유지.
  로그082-build.log/082-root.log/082-dom.log/082-site-final.log.커밋/푸시/배포없음.
  자동승인거절없음.외부작업/모든검사종료,로컬HTTP만유지.

### 81차시 완성 기록

- 2026-09-29: 직전 80차시 완성 턴은 progress, 이번 81차시도 구현·원문/실제 코드 교정·검수로 progress.
  **로컬 1~81차시·399문제**, 공개 1~55차시·269문제. 전체 177차시 목표는 미완료다.
- 다음 시작점: **82차시 “오디오 실패: 게임 전체 실패와 구분”**. `81-xaudio2`에서 누적한다.
  082.json/82체크포인트는 아직 없다. Session.prepare는 bool, main은 한 번 준비하고
  실패 시 소리 없이 게임을 계속한다. play 실패는 사건 자동 재전송 없이 소비한다.
  실제 root는 실패한 init도 shutdown과 짝지으며 장치 참조 횟수와 COM 성공 횟수를 분리한다.
  Part5 §10/§13, audio/audio.h·양 백엔드·Game 초기화 가드를 대조해 실패 분류,
  오류 전파·부분 정리·선택 기능·재시도 범위를 다룰 것. 무한 재시도/동일 로그 반복을 피하고
  오류가 규칙/해시/입력 소비를 바꾸지 않는지 검사한다. 장치 변경·복구는 아직 구현하지 않았다.
- 공개55 동결·학습 기록 이동 재도입 금지·수동 GUI 불필요·Codex 하위 에이전트 금지 유지.
  단순 구현은 승인한 선택 자료만 OpenCode/DeepSeek로 전달해 검토한다. 전체 목표 complete 아님.
- 81: 13절·5문제·접힌 전체 파일5개, 짧은 실제 XAudio 발췌9개.
  같은 제어 계약과 동일 DSP 출력의 차이, Source→Mastering 그래프, COM apartment와
  호출 스레드의 횟수, 부분 초기화/멱등 종료, 프레임·표본·바이트, 설명서와 PCM 대여,
  HRESULT·성공 상태·포화 실패, OS 동기화, 빌드/링크/런타임, API 대역 한계를 설명한다.
  본문은 학습 내용이며 집필 환경 보고는 review/validation에만 기록했다.
- `audio/xaudio_player.h/.cpp`: open/replace/play/stop/unload/close 공통 제어 면.
  SDL state/cursor_frames 진단 API와 출력 바이트 동일성은 공통 계약이 아니다.
  메인 스레드 직렬 제어·같은 스레드 소멸. noncopy/nonmove,9PCM/9Source 자리 소유.
  open은 형식 검사→COM→엔진→Mastering. S_OK/S_FALSE는 이번 성공1회 해제 의무,
  RPC_E_CHANGED_MODE는 호스트 모델을 유지하며 엔진 생성 시도; 성공 보장은 아님.
- replace는 형식/한도를 선검사하고 같은 owner의 모든 보이스를 파괴한 뒤 PCM 이동.
  play는 nullptr/queue0 우선, 포화일 때 oldest를 DestroyVoice 후 재생성.
  Create/Submit/Start 모두 확인, 실패 시 Source 정리, 성공 후 owner/order 기록.
  Stop/Flush만으로 읽기 종료를 추정하지 않는다. stop은 PCM을 보존하며 모든 Source 파괴.
  close는 Source→PCM→Mastering→engine Release→이번 COM 해제; 반복/부분 상태 안전.
- Session이 STUDY_AUDIO_XAUDIO2 또는 SDL 구현을 선택. 규칙/사건/main의 동작 코드는80과 동일.
  CMake STUDY_AUDIO=AUTO/SDL/XAUDIO2/NONE을 창 플랫폼 선택과 분리.
  AUTO는 SCRIPTED→NONE, 그 외 Windows→XAUDIO2, SDL→SDL. 비Windows XAUDIO2 조기 거절.
  누적게임은 SDL 창+XAudio2 조합; WIN32 플랫폼은 여전히 초기 input_demo 전용.
  SDL2 CMake 패키지 우선/pkg-config 대안, 네이티브 xaudio2/ole32 링크·장치 probe는 별도.
- SDL_MAIN_HANDLED를 SDL 소비자에 전달하고 플랫폼/Player 및 직접 초기화하는46진단main에서
  SDL_SetMainReady를 먼저 호출. int main()과 SDL_main(int,char**) 재정의 충돌을 예방한다.
  Windows용 진입점 설계/소스 교정이며 네이티브 링크 성공 증거로 표현하지 않는다.
- DeepSeek `081-xaudio-{material.txt,events.jsonl,draft.json}` exit0. /tmp cwd, standalone/도구deny,
  선택 PCM/order/Player 선언/Session 명세만 전송. out-of-line 소멸자가 헤더를 숨긴다는 설명,
  콜백이 없으므로 동기화 불필요라는 설명을 교정하고 DestroyVoice 완료 경계를 명시했다.
- `tests/xaudio_fake/`는 ABI가 아닌 행동 대역. 설명서는 복사하고 PCM 주소는 보존,
  파괴 직전 첫/끝 바이트 읽기, 그래프별 Source→Master→engine 순서와 COM 의무를 검사.
  실제 XAudioPlayer 전체와 Session을 ASan/UBSan으로 연결. 실패 모든 단계·2개독립그래프·
  잘못된 형식 교체 시 입력/기존재생 보존·idle재사용·같은 PCM다중대여·포화실패를 검사.
- 실제 root `audio/audio.cpp`: BGM Submit/Start HRESULT 검사, 실패 Source파괴/current0,
  마지막 요청은 off→on 재시도용 보존. NOMINMAX 추가, COM 주석과 HRESULT 출력 타입 교정.
  `audio/audio.h`의 동기화/마지막 요청 재시도 설명도 맞췄다. Linux 규칙 구현 변경 없음.
- `check_learning_xaudio_root.py`: 실제 Windows audio.cpp 전체+공통 MP3로더를 대역에 연결.
  고정 legacy_xaudio_music.inc로 수정 전 잘못된 성공 상태 재현, 수정 후 실패 정리·재시도·
  SFX풀·다중대여 unload·공유init·COM 균형을 ASan/UBSan 검사. NOMINMAX 제거 시
  std::numeric_limits<int>::max() 매크로 충돌 컴파일 실패 재현, 가드 버전 전체 컴파일.
- Part5 §1/3/8/10 교정: 호출 스레드 COM/현대2.8+ 생성·Source별 Destroy·BGM 실패·
  SDL/OS 동기화 책임·OS 런타임과 게임 배포 의존성·로더 실패는 API 가드 이전일 수 있음.
  변경 발췌와 관련7절 부분 대응 갱신. 전체 원문 완료라고 주장하지 않았다.
- `check_learning_xaudio.py` 최종 exit0: Release SCRIPTED90/SDL95 CTest·누적게임·경고0.
  SDL/NONE·SCRIPTED/SDL 선택 검사, invalid/XAUDIO2 비Windows 거절,
  실제main의 XAudio Session 선택을 대역 헤더로 컴파일. root17CTest·sim_hash_dump골든 유지.
  수정된 음원형식/포화실패 테스트도 최종 ASan에 포함. scripts/check_learning_checkpoints.py에74~81분기 연결.
- Windows SDK/크로스컴파일러/Wine 환경 없음. 실제 Windows/macOS·스피커·물리 지연 미검증.
  Microsoft COM/XAudio2·SDL 공식 계약 대조. 사용자 요청대로 수동 GUI/스크린샷 없음.
- 원문28/소스180, coverage344: covered3/partial90/unassigned251/needs-review0.
  Part발췌·Markdown27사례·399문제·DOM 전체차시/단일마운트/탐색/저장/참조/복사 통과.
- 최종 local release **bd40e3e89a2aab92**,15파일·재현가능ZIP.
  HTTP18767 lessons.js5782323/library.js5316042바이트 일치·81포함.
  로그081-build-final.log/081-root.log/081-dom.log/081-site-final.log.
  공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 자동 승인 거절 없음.
  외부 작업/모든 검사 종료, 로컬 HTTP만 유지.

### 80차시 완성 기록

- 2026-09-29: 직전 79차시 완성 턴은 progress, 이번 80차시도 구현·실제 백엔드/Part 교정·검수로 progress.
  **로컬 1~80차시·394문제**, 공개 1~55차시·269문제. 전체 177차시 목표는 미완료다.
- 다음 시작점: **81차시 “XAudio2: 같은 계약의 Windows 경로”**. `80-voice-pool`에서 누적한다.
  081.json/81체크포인트는 아직 없다. 현재 학습 장치 Player는 SDL 전용,
  Session은 STUDY_AUDIO_SDL에서만 실재생하며 SCRIPTED/Windows 구성은 false/no-op.
  root `audio/audio.cpp`의 현재 API/COM/보이스 형식·PCM 대여·HRESULT·종료와
  `audio/audio.h`, CMake의 플랫폼 선택을 검토하여 누적 학습 경로에 Windows 구현을 연결할 것.
  원문 Part5 §1~3/§7/§11 및 Part13 플랫폼 빌드를 대조한다. 실제 Windows SDK/컴파일러와
  네이티브 실행 가능 여부는 다음 턴 현재 환경으로 확인할 것. 대역을 네이티브로 주장하지 않는다.
  root BGM start_music_voice는 SubmitSourceBuffer/Start 반환 검사를 아직 추가하지 않았으며,
  80은 SFX 경로를 수정했다. COM 스레드·BGM 오류 처리·형식 계약을 함께 확인할 것.
- 공개55 동결·학습 기록 이동 재도입 금지·수동 GUI 불필요·Codex 하위 에이전트 금지 유지.
  승인된 선택 코드만 OpenCode/DeepSeek에 전달한다. 전체 목표 complete 아님.
- 80: 14절·5문제·접힌 전체 파일4개. 자원 vs 재생 슬롯, 공유 PCM/독립 커서,
  풀 예산 vs 사건 종류 수, 시작 순열과 LRU 차이, 고정 배열/카운터 넘침 회피,
  요청 끝의 완료 판정, active와 참조 분리, 모든 대여 종료, 세션 핸들,
  비동기 Stop/Flush/Destroy 경계와 실패 후 상태를 설명한다.
- 공통 `audio/voice_order.h`: VoiceOrder<N>이 0..N-1 순열을 유지.
  성공한 시작 번호를 끝으로 이동, 나머지 상대 순서 유지. 빈자리 우선이며
  모두 활성일 때만 oldest를 선점 후보로 쓴다. 시간/증가 카운터/heap 없음.
  root와80체크포인트 helper 바이트 동일. root SFX는8, 학습 VoicePool은9.
- `audio/voice_pool.h`: Mixer + owners_[voice] + 시작 순서.
  start(owner,clip)는 빈 보이스부터 선택, 모두 바쁘면 oldest. Mixer 검증 성공 후에만
  owner와순서를 기록. owner는 PCM 소유 자리 태그이며 kNoOwner=maxsize_t는 거절.
  같은 owner는 stop_owner까지 동일·생존·불변 PCM을 빌려야 한다. 호출자가 모든 접근 직렬화.
  stop_owner는 완료된 보이스의 잔여 참조까지 모두 분리. stop_all/configure 성공도 연결 초기화.
  playing(owner)는 하나라도 활성, cursor(owner)는 아직 남아 있는 가장 최근 재생의 커서.
  owner_of/반환 보이스 번호는 일시적 진단 위치이며 세대가 있는 영구 신분이 아니다.
  Pool은 복사/이동을 막은 제어자이며 PCM을 소유하지 않는다.
- Player의 Mixer를 VoicePool로 바꾸고 play(음원slot)가 새 보이스를 자동 선택하게 했다.
  기존 시그니처 유지, 반복 play는 이제 중첩된다. replace/unload는 stop_owner로 해당 PCM의
  모든 연결을 끊고 retired를 잠금 밖에서 해제. close/stop의 전체 종료 계약 유지.
  Session의 네 PCM 자리·main·규칙·사건 투영은 유지한다. 네 음원 ×4410frames×2ch×2B=70560B,
  최대9보이스 ×피크2000=18000. PCM복사는 늘지 않고 재생 상태가 추가된다.
  cursor/state의 의미는 현재 풀 관찰값이며 모든 보이스를 잃으면 cursor0/loaded ready.
- DeepSeek `080-pool-{material.txt,events.jsonl,draft.json}` exit0.
  /tmp cwd, standalone/도구 deny, 선택 Mixer/Voice/PCM만 전달. 이동 금지에 대한 과장된
  설명을 정책으로 명시하고 owner의 동일 PCM 계약을 보강한 뒤 직접 적용했다.
- root SDL/Windows SFX: 항상0번 교체하던 정책을 실제 시작 순서 기반으로 수정.
  SDL은 마지막 샘플이 요청 마지막 프레임과 일치할 때 즉시 active=false.
  빈 슬롯인데 바쁘다고 여겨 다른 소리를 자르던 경계 오류를 교정했다.
  SDL init/shutdown에 순서 reset, Windows 첫 성공 init에 reset. shared init에서는 유지.
- root Windows 포화 교체: Stop/Flush 후 handle을 덮기 전에 이전 읽기 종료가 확정되지
  않을 수 있다. DestroyVoice로 대여 종료 후 재생성하고 추적 형식/핸들을 정리한다.
  SubmitSourceBuffer와 Start의 HRESULT도 검사, 실패 시 보이스 파괴·추적값 초기화.
  성공한 시작만 순서에 반영한다. 생성 실패가 이미 끊은 이전 소리를 복원하지 않음,
  DestroyVoice의 메인 스레드 대기 가능성도 설명한다. 이것을 실시간 지연 보장으로 쓰지 않는다.
- audio.h에 8보이스 포화 정책과 init 세션 내 정수 핸들 수명을 명시.
  언로드 인덱스는 같은 세션에서 재사용하지 않지만 shutdown 이후 숫자는 재사용될 수 있다.
  API에 세대 검사를 새로 구현한 것으로 주장하지 않는다.
- Microsoft FlushSourceBuffers/DestroyVoice 공식 문서 확인·Part/본문에 직접 링크.
  테스트 대역은 Stop/Flush가 아직 읽기를 종료하지 않은 합법적 지연 스케줄을 모델링한다.
  실제 Windows에서 UAF를 관찰했다거나 ABI·스피커·지연을 검증했다고 주장하지 않는다.
- `check_learning_voice_pool.py` exit0: Release SCRIPTED88/SDL93 CTest·누적게임·경고0.
  Voice/Player/callback/Mixer/Pool/사건/cue/Session ASan·UBSan·float-cast-overflow 통과.
  Pool 5000회 별도 deque 기반 모델과 선택/출력/활성/연결 대조, invalid owner/format/configure,
  같은 PCM9보이스 연결 해제 후 실제 PCM 파괴/출력, 정확한 완료 경계 재사용 확인.
  마지막 테스트도 PCM 지역 소유자 종료 전 stop_all을 명시하고 해당 target/ASan 재검증.
  실제 Player callback에서 같은 PCM 두 보이스 합4000 및 두 연결 unload 후 무음,
  retired 잠금 밖 해제와 기존 콜백 heap guard 유지 확인.
- `check_learning_voice_pool_root.py` exit0. 고정 before fixture
  `tests/learning/legacy_sdl_pool.inc`, `legacy_windows_pool.inc` 보관.
  실제 SDL 전체소스의 수정 전 고정0 선점/정확한 끝 active 지연 재현,
  수정 후 oldest/끝 재사용/BGM+8SFX 동일 PCM 전부 분리/무효 핸들 무음 검사.
  Windows 실제 FormatMatches/unload/play 함수 본문에 대역 연결, 이전 추적 소실 가능성,
  수정 후 FIFO·create/submit/start 실패·다중 참조 종료 검사. 해제 전 PCM 주소를 보관하도록
  대역 검사를 강화하고 Windows before/after만 ASan/UBSan 재검증(080-windows-final.log).
  root게임17CTest·sim_hash_dump 골든 유지. 네이티브 Windows/macOS·실제 스피커 미검증.
- Part5 최신 발췌와 §7 재집필. 8이 수학적 최대/최적이라는 단정, 0번이 대략 FIFO라는 설명,
  풀 확대가 반드시 콜백 할당이라는 설명, 포화가 드물고 잘림을 인지하기 어렵다는 단정 제거.
  공통 디코더인데 백엔드별 부분읽기/fseek 정책이 다르다는 낡은 설명도 교정.
  관련 세 기존 검토 절의 해시/부분 대응과 풀 절의80부분 대응 갱신. 전체 원문 완료 아님.
- 원문28/소스180, coverage344: covered3/partial90/unassigned251/needs-review0.
  Part 발췌·Markdown27사례·394객관식·DOM 전체차시 렌더링/단일마운트/탐색/기록/참조/복사 통과.
- 최종 local release **b56856b81946aae5**,15파일·재현가능ZIP.
  HTTP18767 lessons.js5677737/library.js5311524바이트 일치·80포함.
  로그080-check.log/080-root.log/080-windows-final.log/080-pool-final.log/080-dom.log/080-site-final.log.
  공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 자동 승인 거절 없음.
  외부 작업/모든 검사 종료, 로컬 HTTP만 유지.

### 79차시 완성 기록

- 2026-09-29: 직전 78차시 완성 턴은 progress, 이번 79차시도 구현·실제 코드/Part 교정·검수로 progress.
  **로컬 1~79차시·389문제**, 공개 1~55차시·269문제. 전체 177차시 목표는 미완료다.
- 다음 시작점: **80차시 “풀과 참조 수명: 재생 중인 자원”**. `79-sound-events`에서 누적한다.
  080.json/80체크포인트는 아직 없다. 현재 Player는 최대 9개의 소유 PCM 슬롯을 갖고,
  각 슬롯은 Mixer의 같은 번호 보이스와 1:1이다. Session은 그중 0~3을 종류별로 사용하며
  같은 종류는 재시작한다. 자동 빈자리 선정·가로채기와 한 음원의 여러 보이스 대여는 없다.
  root SDL MAX_SFX_VOICES/audio_play_sound/audio_unload_sound와 Windows SFX 풀,
  Part5의 풀/자원 수명을 대조할 것. 재생 중 unload·잘못된/재사용 핸들·풀 포화 정책을
  실제 출력/수명 검사로 설명한다. 이전 불변식과 현재 콘텐츠 정책을 구별할 것.
- 공개 55 동결·학습 기록 이동 기능 재도입 금지·수동 GUI 검사 불필요·Codex 하위 에이전트 금지 유지.
  단순 구현은 승인된 선택 자료만 OpenCode/DeepSeek에 전송하고 직접 검수한다.
  전체 목표 complete가 아니며 다음 턴부터 이어 간다.
- 79: 14절·5문제·접힌 전체 파일 5개. 사건 vs 상태, bool 병합/정보량, 틱별 관찰값,
  확정 후 표현, 0 후보/0 거리 경계, 고정 용량 증명, 이동 후 소비 위치,
  요청 1회 vs 실제 들림, 종류별 슬롯 정책, 소유 PCM의 선택적 해제까지 연결한다.
  본문에 집필 환경 결과를 넣지 않고 review/validation에 실행 증거를 분리했다.
- `presentation/sound_events.h`: 최대 max_ticks*4=24 Kind의 이동 전용 Batch.
  report.ticks 범위 검사, stopped/invalid 무시. 틱별 rotate(kick>=0), drop(distance>=0),
  clear(cleared>0), garbage(inserted>0) 순서. take는 반환 전 커서 증가,
  이동은 size/next를 보존하고 원본을 비운다. 같은 FrameReport를 재투영하면 새 요청이
  생기므로 전역 exactly-once라고 주장하지 않는다. main에서 성공한 advance당 한 번 투영.
- `audio/player.h/.cpp`: optional<Pcm16> 배열, replace/play/unload/state/cursor의 기본 슬롯 0.
  기존 단일 음원 도구 API 유지. replace/unload는 해당 Mixer 슬롯만 분리하고 retired를
  잠금 밖에서 해제한다. unload의 전역 pause 제거로 다른 슬롯 진행 유지.
  stop은 전체 pause/stop_all, close는 콜백 종료 후 모든 PCM 해제. 제어 API는 main 직렬.
- `audio/cue_pcm.h`: 44100Hz/stereo/4410frames, 주기100/200/64/320 삼각파.
  앞뒤441frames 선형 포락선, 시작/끝0, 절댓값<=2000. 네 종류 합 상한8000.
  준비 때만 할당하며 사건마다 생성/디코딩하지 않는다. 실제 MP3를 쓰려면 장치 형식을 맞춘다.
- `presentation/sound_session.h`: 한 번 준비해 네 슬롯 설치, 실패 시 부분 장치 정리,
  반복 prepare 성공은 no-op. 초안의 중복 prepared/ready를 ready 하나로 정리.
  callback userdata는 Session 자체가 아닌 내장 Player 주소라는 주석을 교정했다.
  같은 종류 재시작은 요청을 모두 처리해도 모든 음원이 끝까지 들림을 보장하지 않는다.
  틱→샘플 시각 예약/스레드간 큐/자동 풀 정책을 추가한 것으로 설명하지 않는다.
- 누적 src/main.cpp에 Session 준비·AppReport.frame 투영/소비 연결.
  메뉴/종료/게임 재생성 때 stop, 최종 게임오버 틱의 frame은 소비.
  장치 실패 안내는 준비 때 한 번, 규칙 진행 유지. SDL tetris에 study_playback 연결.
  SCRIPTED Session은 false/no-op. Round/FrameRunner/Game/Application/Mixer/Voice는
  78과 바이트 동일. 기존 파일 중 CMake/README/DESIGN/player.h/.cpp/main/callback검사만 변경.
- DeepSeek `079-sound-{material.txt,events.jsonl,draft.json}` 및
  `079-cues-{material.txt,events.jsonl,draft.json}` 최종 exit0. 승인된 선택 C++만 제공.
  첫 호출은 repo cwd였으나 standalone/도구 deny로 요청된 첨부 코드 작업만 수행,
  두 번째는 /tmp cwd. 저장소 전체/키/계정정보 전송 없음. 외부 작업 모두 종료.
- root `Game::ConsumeSoundEvents` 추가: SubmitInput/Tick/MoveBlockDown 각각 직후 호출.
  네 bool을 std::exchange로 먼저 분리하고 rotate/drop/clear/garbage 요청.
  하드 드롭 안에서 발생한 가비지/클리어가 다음 Tick을 기다리던 경로 수정.
  직접 down 호출도 소비한다. root bool은 한 sim 호출 안의 같은 종류를 병합하며,
  공개 game.sim 직접 변경은 래퍼 소비를 우회한다는 한계 명시.
- Part4/5: 최신 함수/헤더 발췌·소비 표·다이어그램·부록 갱신.
  소비자 둘이면 플래그 둘이 필수라는 단정→별도 플래그 또는 불변 결과/각 커서.
  두 함수에 나눠 소비해도 지연 사실상0이라는 설명 삭제, 요청과 실제 출력 구별,
  풀8 근거를 세 소리 발생만으로 단정하던 설명 교정.
- `check_learning_sound_events.py` exit0: SCRIPTED87/초기SDL91 CTest,
  최종 장치 실패 검사 추가 후 configure/build 및 **SDL92 CTest** 별도 통과.
  경고 없음. 자기 이동 검사의 의도적 컴파일러 경고를 일반 transfer 함수로 정리한 뒤 재검증.
  Voice/playback/callback/mixing/sound_events/cue/session ASan·UBSan·float-cast-overflow 통과.
  사건24개 순서·빈/범위초과·0 경계·이동/반복 소비·실제 pending 입력·반복 view,
  30seed×120frame 해시/phase 동등성 검사. 두 실제 고정이 있는 6틱 보고서도 보존 확인.
- 실제 Player callback 대역을 수동 구동해 두 슬롯1000+2000=3000,
  0번 교체 후2300, 해제 후1번2000/커서4 유지, 잘못된 형식·슬롯 거절,
  retired 잠금 밖 해제·C++ 할당/해제 guard 유지 확인. 실제 스피커/지연 검사는 아니다.
- `check_learning_sound_events_root.py` exit0. 고정 fixture
  `tests/learning/legacy_game_sound_dispatch.inc`로 수정 전 실제 하드 드롭의 가비지 소비 지연 재현.
  수정 후 ASan/UBSan에서 직접 down·반복 draw/입력·초기화 실패 시 규칙 해시·소유권 검사 통과.
  root 누적 게임 빌드17CTest·sim_hash_dump 골든 유지. Windows/macOS 네이티브 미검증.
- Part 발췌, Markdown27사례, 389객관식 스키마/정답 선택·해설, DOM 전체 차시 단일 마운트/
  탐색/완료·답안 저장/참조/복사 검사 통과. 기존 문항의 answer는 정답 옵션을 바꿔 쓴 문장도
  허용하므로 전체 문자열 동일성은 요구하지 않고 79문항만 동일성 확인.
- 원문28/현재소스179, coverage344: covered3/partial89/unassigned252/needs-review0.
  Part5 이벤트 절에79부분 대응 추가. 변경된 Part4/5 다섯 기존 검토 절의 해시를 실제 대조 후
  갱신했고 원문 전체 완료로 확대하지 않았다.
- 최종 local release **520346e033c9fd82**,15파일·재현 가능한 ZIP.
  HTTP18767 lessons.js5630484/library.js5305685바이트 일치·79포함.
  로그 `079-check.log`, `079-root.log`, `079-device-final.log`, `079-dom.log`, `079-site-final.log`.
  공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 자동 승인 거절 없음.
  모든 외부/검사 작업 종료, 로컬 HTTP만 유지.

### 78차시 완성 기록

- 2026-09-29: 직전77완성턴은 progress, 이번78도 구현·root/Part 교정·검수로 progress.
  **로컬1~78차시·384문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **79차시 “규칙 이벤트: 한 번만 소리를 내기”**. 78-mixing에서 누적한다.
  079.json/79체크포인트는아직없다. 현재main/게임규칙은77과그대로,Player는한소유음원을
  Mixer0번슬롯으로재생한다. Mixer는명시적9슬롯까지가능하나자동빈자리/가로채기정책은80범위.
  학습simulation/round.h의latesttick결과(회전/드롭/라인/가비지관련)와src/game.cpp Update의
  rotateSoundEvent/dropSoundEvent/clearSoundEvent/garbageSoundEvent 소비후false를대조할것.
  catch-up여러틱/같은tick반복조회/여러실제event병합·게임실패시규칙불변을설계하고실제연결한다.
  공개55동결·학습기록이동재도입금지·수동GUI불필요 유지. 목표전체complete아님.
- 78:13절·5문제·접힌전체파일5개. 채널별동시각합산·표현vs계산범위·중간클리핑손실·
  게인진폭vs청감·NaN비교·보이스별절삭·넓은누산증명·고정블록·음소거커서·헤드룸설명.
- 공통 audio/mix_s16.h를root와78기준코드에동일반영(완성시바이트일치검사).
  kMaxVoices9/kBlockFrames256. nonfinitegain은0,finite는[0,1].
  scaled_sample은float곱후int32로0방향절삭,finish_sample은합을최종int16클램프.
  9*32768=294912 상한을static_assert. 순서독립은고정된정수기여량/오버플로없는합에한정.
  float누산의일반적결합법칙이나다른양자화정책과같다고주장하지않음.
- 학습 audio/mixer.h: 기존Voice9슬롯+gain,기본형식invalid,configure검증후stop_all과형식설정,
  start는slot/nonempty/rate/channels검사후에만교체. 명시슬롯,auto-pool없음.
  PCM은호출자소유/연결중생존불변,호출자가동시접근조정. gain0도Voice.render로커서진행.
  render는출력전체0채움→최대256frames씩512int16scratch+512int32sum→최종memcpy.
  두배열3KiB는전체스택측정아님. mono/stereo같은형식만받고부분프레임꼬리0/미정렬허용.
- Player의Voice필드를Mixer로교체,open시검증된형식configure,play는0번start,
  state/cursor도0번슬롯조회,callback은Mixer.render,replace/unload/close는stop_all.
  소유음원하나인공개API·retired잠금밖해제·주소안정성·콜백락계약유지.
  체크포인트의기존파일중CMake/README/DESIGN/player.h/.cpp외모두77과바이트동일.
- mix_probe:30000+30000-30000=30000,legacy순차클램프2767을실제출력비교.
  DeepSeek078-mixing-{material.txt,events.jsonl,draft.json}선택자료만전송,최종exit0.
  후보가Voice를mixer.h에복제한부분제거/기존voice.hinclude로교정하고중복형식분기간소화.
- root SDL mix_voice가int32*누산기에기여,callback이최대256프레임고정배열2KiB로분할,
  BGM1+SFX8모두합산후finish_sample/memcpy로출력. len<=0/채널1..2가드와전체0채움.
  MAX_SFX_VOICES+1<=공통상한static_assert. 한음원출력동일,보이스수에따른중간손실제거.
  SDL/XAudio2volume setter 모두normalize_gain사용. NaN/무한대는무음정책.
  Windows의실제DSP와SFX볼륨적용시점까지비트동일/동일행동이라고주장하지않음.
- scripts/check_learning_mixing.py exit0:Release SCRIPTED84/SDL88CTest·누적게임·경고0.
  모든S16값×gain0/.5/1·6순열·9보이스양/음극단·formatreject/slotbound/mutecursor,
  모노/스테레오×요청1/255/256/257/513×4회독립int64oracle·가드·꼬리0·미정렬검사.
  Voice/playback/callback/mixing ASanUBSan+float-cast-overflow 통과.
  실제Player콜백회귀로새Mixer단일음원바이트/잠금밖해제/C++heap계약유지확인.
- check_learning_mixing_root.py:실제SDL전체소스의callback을직접구동.
  tests/learning/legacy_mix_s16.inc는고정된수정전함수만보관한회귀fixture.
  before6순열30000/2767차이·NaN정수변환UB를sanitizer비정상종료로재현;
  after모든순열30000·513프레임chunk/미정렬/꼬리0·비유한gain0·mute진행검사.
  Windows실제volume setter본문을MockVoice에연결해동일유한정책확인,네이티브ABI미검증.
  기존root77callback/76수명회귀·actualgame빌드17CTest·sim_hash_dump골든유지.
  로그078-check.log/078-root.log. 실제스피커·Windows/macOS실행·지연/청감미검증.
- Part5/11 current발췌/volume정책/믹싱도식갱신. 양자화위치·정수승격vs출력범위·
  좁은순차클램프의순서의존·RMS/피크조건·항상청감무시가능단정교정.
  AudioStream을장치출력대안으로취급하던설명→형식변환역할,장치공급은callback/Queue선택.
  렌더링과오디오가축만다른동일구조라는단정도책임분해비교로교정.
  SDL_MixAudioFormat/Microsoftvolume/C++conv.fpint공식근거확인,관련본문링크.
- 원문28/소스179,coverage344:covered3/partial88/unassigned253/needs-review0.
  Part5SDL/설정,Part11오디오에78부분대응추가. Part5§1비유문구검토해시만갱신,
  기존76부분대응범위를확대하지않음. 원문오디오전체완료로표시하지않음.
- Part발췌·Markdown27사례·384객관식·DOM단일마운트/탐색/기록/참조/복사통과.
  원문마지막수정과겹친첫정적검사실패후확정자료로재검증exit0.
  최종local release **f211a66936aaf9de**,15파일·재현가능ZIP.
  HTTP18767 lessons.js5573498/library.js5303672바이트일치·lesson78포함.
  로그078-dom.log/078-site-final.log. 최종생성물--check통과.
- 공개release708d2e519c17c011유지. 커밋/푸시/배포없음. 외부작업과최종검사모두종료,
  로컬HTTP만실행중. 기존sandbox문제로승인된실행사용,자동승인거절없음.

### 77차시 완성 기록

- 2026-09-29: 직전76완성턴은 progress, 이번77도 구현·코드/Part 교정·검수로 progress.
  **로컬1~77차시·379문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **78차시 “믹싱: 더하기·게인·클리핑”**. 77-callback에서 누적한다.
  078.json/78체크포인트는 아직 없다. 단일보이스SDL재생까지있고main/규칙은유지.
  audio/sdl_audio.cpp mix_voice/볼륨setter, XAudio2 volume, Part5 §8.4/9를검토한다.
  root gain NaN→float-to-int 문제·순차포화합산의순서의존·넓은누산후최종클램프를검토하고
  실제입력수치/경계/비정상gain으로검사할것. 믹싱주제와79이벤트/80풀/81XAudio2를구분.
  공개55동결·학습기록이동재도입금지·수동GUI검사불필요 유지. 목표전체complete아님.
- 77:13절·5문제·접힌전체파일3개. 버퍼기간/실제지연·바이트/프레임·시작지연/대기/작업·
  우선순위역전·숨은할당과해제·소유권교환·스코프정리·검사의증명범위를설명한다.
  2048bytes/4=512frames/44100≈11.61ms,1024frames≈23.22ms는음원기간이며지연상한아님.
  2049byte경계입력은512프레임+꼬리1byte0으로검사하며정상SDL요청일반화없음.
- 77-playback가아닌 **77-callback** 체크포인트. 76파일중CMake/README/DESIGN/player.cpp외
  기존파일바이트동일. Voice와main/규칙/렌더링/MP3/Player공개API는유지했다.
  Player::replace/unload 바깥optional<Pcm16>retired,잠금안Voice.stop→retired.swap(clip_)→
  replace시새후보emplace(move),잠금해제후retired파괴. 실패검사는변경전.
  Pcm16 noexcept이동은배열복사/할당없이소유권인계. 여전히SDLdevice lock구조이며
  제어APImain직렬,lockfree/hardrealtime보장아님. 해제작업은같은main에서수행한다.
- root SDL audio_unload_sound도모든해당BGM/SFX연결끊기→PCM빈vector교환→validfalse를
  s_mu안에서수행하고지역retired파괴는잠금밖. 소유·동시접근계약유지.
  audio_load_sound의바깥s_sounds.push_back재할당은여전히잠금안이며본문범위명시.
- DeepSeek out/learning-jobs/077-callback-{material.txt,events.jsonl,draft.json},최종exit0.
  첫/tmp작업경로오류로자료파일없음CLIexit1,자료작성후새호출정상종료. 승인된선택자료만전송.
  후보의없는Pcm16::allocate/data를실제make_reference_tone/samples로교정,
  INTERFACE타깃을-lstudy_pcm으로링크하는설명제거,가상의littleendian가정제거.
  진단카운터thread_local·nullfree계수제외·준비를측정구간밖·CHECK실패즉시종료로교정.
- tests/callback_contract.cpp는실제player.cpp를대역SDL호출과함께포함한다.
  realSDL장치는계속pause,등록된실제callback을직접90회호출해2049byte/가드/음원전체/꼬리0검사.
  SDLworker실제스케줄링검사는기존playback_contract가별도담당한다.
  일반operatornew/delete와잠금깊이관찰로callback C++heap0,
  replace/unload에서실제해제양수+잠금안해제0. 불변76player.cpp를같은검사에연결해
  LEGACY_EXPECT_LOCKED_FREE에서기존잠금안해제양수확인.
  카운터는Cmalloc/SDL내부/물리지연/WCET증명아님. readonly기준PCM바이트비교로엔디언가정없음.
  최적화GCC가malloc기반대체new를인라인하여mismatch경고를내므로진단경계noinline
  (GNU/Clang/MSVC조건부)을추가. 경고억제플래그없이최종경고0빌드통과.
- scripts/check_learning_callback.py 최종exit0:Release SCRIPTED82/SDL86CTest·누적tetris·경고0,
  Voice/playback/callback ASanUBSan,legacy해제위치대조,루트SDL새진단과76수명회귀,
  실제root게임빌드·17CTest·sim_hash_dump골든유지. 로그077-check-final.log.
  진단stderr도파일로보관하도록스크립트출력수집보강,callback-sanitized단독재실행exit0.
- check_learning_callback_root.py는실제root전체SDL소스에서lock_guard경계만TrackedLock으로
  관찰,실제PCM사운드40회콜백출력·무음꼬리·C++heap0·언로드before잠금안free/after밖free확인.
  SDLdummy실제디코더/장치사용,ASanUBSan. 76regression의Windows계약mock도재통과.
  nativeWindows/macOS·스피커·실제지연/스케줄링상한은미검증이며본문작업보고없음.
- Part5want.samples를고정호출간격/지연하한/최대23ms로단정한설명교정.
  new없으면시스템콜없음·콜백할당없으면glitch없음·생성자로드면경합해결·reserve한줄이면해결
  설명을준비/공급/잠금/할당자/측정범위로교정. 언로드현재발췌와공유상태표동기화.
  남아있던항상체감+3dB문구도이미교정된§8.4의조건부설명과맞춤.
  공식SDL2AudioSpec/Open/Lock대조,관련본문링크. 원문coverage344:
  covered3/partial87/unassigned254/needs-review0;Part5SDL/오류절의검토해시갱신.
- 원문28/소스178. Part발췌·Markdown27사례·379객관식·DOM단일마운트/탐색/기록/참조/복사통과.
  최종local release **b37a89e66f66546c**,15파일·재현가능ZIP.
  HTTP18767 lessons.js5523978/library.js5297569바이트일치·lesson77존재.
  로그077-dom.log/077-site-final.log. 수정후최종스니펫과선택지로정적export재검증.
- 공개release708d2e519c17c011유지. 커밋/푸시/배포없음. 최종외부작업/검사모두종료0,
  로컬HTTP만실행중. 기존sandbox mountinfo문제로승인된실행사용,자동승인거절없음.

### 76차시 완성 기록

- 2026-09-29: 직전75완성턴은 progress, 이번76도 누적 구현·루트/Part 교정·검수로 progress.
  **로컬1~76차시·374문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **77차시 “SDL 콜백: 실시간 경로에서 피할 작업”**. 76-playback에서 누적한다.
  077.json/77체크포인트는 아직 없다. main/게임규칙은 유지하고 SDL 단일보이스도구까지구현했다.
  콜백의 요청단위/실행시간/할당·파일IO·디코딩/잠금대기·제어명령교환을 현재소스·Part5와
  비교하고, 최소 보강을 구현·검수한다. 78믹싱/79이벤트/80풀/81XAudio2 전체를 미리 넣지 않는다.
  root의 volume NaN 입력 정책은78게인/클리핑 검토 때 확인할 항목(현재 UI 정상범위와별개).
  공개55동결·local진행·학습기록가져오기/내보내기재도입금지·수동GUI불필요 유지.
- 76:15절·5문제·접힌전체파일6개. 세객체소유/대여·프레임커서·바이트복사·주소안정성·
  상태도출·준비후확정·실패정리·콜백배타경계·읽기종료후해제·멱등설정·제출vs청취를설명.
- 체크포인트 Voice는const Pcm16를빌리고독립프레임cursor를관리한다. 출력전체0채움후
  완전한프레임만복사,음원끝/부분프레임꼬리0,memcpy로출력정렬전제추가없음.
  원본은연결중생존/불변/이동금지. finished도stop전까지포인터는연결되어있다.
- Player는장치/optional PCM/Voice소유,userdata=this주소고정을위해복사/이동삭제.
  open은실패로컬정리후성공만멤버확정,allowed_changes0·AUDIO_S16SYS·512frame요청.
  제어API끼리는main직렬호출,DeviceLock은콜백과의접근조정. arbitrarycaller동시호출미지원.
  replace는형식검사후stop/emplace(move),실패시기존음원/후보유지.
  stop은PCM보존/cursor0,unload는장치보존,close는잠금없이SDL_CloseAudioDevice종료대기후
  Voice/PCM/자기SDL참조정리. PCM파괴의할당자작업으로잠금시간상한은보장하지않음.
- playback_probe는디코드→open→replace→play→submittedframes관찰→정리.
  rotate.mp3는16128frames; finished는출력버퍼로복사완료,청취완료아님.
  deadline은ceil(기간ms)+2000,실제장치스케줄링상한보장은아님.
  75의기존파일들(main/규칙/데이터준비등)은CMake/README/DESIGN제외바이트동일.
- DeepSeek076-playback 이벤트/초안/선택자료 out/learning-jobs/076-playback-*.
  최종작업종료0. 없는audio_pcm헤더/load_clip누락·원본대여주석·고정5초대기·finished출력교정.
  Voice검사중size_t빼기표현을signed연산으로교정. 외부에키/설정/전체저장소전송없음.
- root SDL s_audioOwned추가:Init성공뒤보유,deviceopen실패시Quit후false,shutdown은보유시만Quit.
  초기화시도s_refCount와SDL의실제소유참조를분리,다른모듈참조를두번반환하던문제수정.
  두backend sentinel저장소할당을OS자원획득전try로이동해bad_alloc/length_error반환false.
  SDL마지막shutdown에서voices/BGM/currentMusic도초기화해재시작stalehandle제거.
  SDLmusicEnabled같은값재적용은return,양수볼륨변경때곡이되감기는문제수정.
  audio.h에제어호출main직렬계약명시. root API 실패init도shutdown짝필요계약유지.
- root Windows언로드:Stop/Flush+최대100회Sleep폴링후PCM해제를DestroyVoice→slotnull/formatreset→
  PCM해제로변경. 제한시간만으로읽기종료를보장하지못하는문제. 풀재생성정책은유지.
- Part5현재init/shutdown/설정/unload발췌동기화. SDL시도참조vs소유참조,Windows종료보장설명.
  자원재사용이면gapless라는단정·unique_ptr내부대입순서잘못된설명·컨테이너교체만으로
  참조/동시성해결설명교정. 현재SDL/XAudio2지원설명과실제48k/44.1k에셋설명갱신.
  공식SDL2 Open/Lock/Close와Microsoft DestroyVoice계약확인,본문관련자리링크.
- 최종 scripts/check_learning_playback.py 종료0:
  Release SCRIPTED82/SDL85CTest·누적tetris·경고0. Voice전체요청크기행렬/독립커서/정렬/꼬리0,
  SDLdummy실제콜백/16128frames/틀린형식거절/12openplayclose/언로드/외부참조생존,ASanUBSan.
  root실제SDL실패open/할당실패/sharedinit/동일enabled/음악stalehandle수정전후대역재현.
  root실제Windowsunload본문+API계약mock에서100ms후에도읽기가능vsDestroy후해제검사.
  root실제tetris빌드·17CTest·sim_hash_dump골든유지. 로그076-check-final.log.
  초기추가회귀의assets/rotate.mp3경로오타를Sounds/rotate.mp3로교정후최종전체재실행.
  네이티브Windows/XAudio2/macOS·실제스피커·수동GUI는미실행. 작성환경보고본문에넣지않음.
- 원문coverage344:covered3/partial87/unassigned254/needs-review0. Part5추가5절부분대응,
  SDL절과오류절변경해시검토갱신. 원문28/소스178. 76전체오디오완료로표시하지않음.
- Part발췌·Markdown27사례·374객관식·DOM단일마운트/탐색/답안기록/참조/복사통과.
  최종local release **21f4b820f1e539e8**,15파일·재현가능ZIP. 최종생성물--check전부통과.
  HTTP18767 lessons.js5474130/library.js5293682바이트일치·lesson76포함.
  로그076-dom.log/076-site-final.log. 마지막Part에셋문구수정후정적export재검증완료.
- 공개release708d2e519c17c011유지,커밋/푸시/배포없음. 최종외부작업/검사모두종료,
  로컬HTTP만실행중. mountinfo기본sandbox실패후승인된실행사용;자동승인거절없음.
  작은차시완료로전체goal을complete처리하지않는다.

### 75차시 완성 기록

- 2026-09-29: 직전74완성턴은 progress, 이번75도 누적 구현·루트/Part 교정·검수로 progress.
  **로컬1~75차시·369문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **76차시 “재생 수명: 장치·버퍼·보이스”**. 75-mp3에서 누적한다.
  076.json/76체크포인트는 아직 없다. 현재 학습은 CPU 디코딩과mp3_probe뿐이며main/규칙은유지.
  audio/audio.h·SDL/XAudio2초기화/종료/재생/언로드·src/game.cpp참조카운트·Part5를읽고,
  학습 장치/저장소/재생인스턴스의최소계약을추가한다. 콜백/믹싱전부는77/78편성과분리.
  공개55동결·local진행·학습기록가져오기/내보내기재도입금지 유지.
- 75:16절·5문제·접힌전체파일6개. 압축vsPCM/MP3프레임·실제읽은수량·바이트예산·
  빌린입력/소유출력·성공init가드·단일헤더번역단위·증분읽기vs재생스트리밍을설명.
  실제rotate.mp3를assets에보관:48000Hz2ch16128frames32256samples64512bytes0.336초.
  드코더의56개잘린접두성공사례로디코딩성공과파일전체무결성차이를명시.
- root/학습 공통 audio/mp3_decode.h/.cpp 동일. DR_MP3_IMPLEMENTATION은cpp한곳에만정의.
  Limits기본파일16MiB/PCM64MiB,Result는error+channels/rate+소유vector<int16>.
  load는FILE RAII·fseek/ftell·예산후할당·정확한fread·뒤EOF/ferror검사. 로컬seek가능파일용,
  같은길이동시덮어쓰기등의스냅샷보장없음. input은decode가끝날때까지유지.
  decode는init_memory성공후uninit Guard,형식검사,4096샘플배열/채널수로프레임요청,
  got*channels만누적. added>maxSamples-currentSamples이면추가전후보전체버림.
  bad_alloc/length_error→allocation_failed. initfalse/0프레임은decode_failed이며
  라이브러리내부실패원인/EOFvs손상전체를구분하는엄격검증기가아님.
  logicalbytes예산이며capacity/재할당/디코더내부/SDL변환/모든핸들합계한도는아님.
- 학습load_clip은PCM16MiB로낮춰Pcm16계약과연결하고vector이동. mp3_probe는메타데이터출력.
  CMake study_mp3 STATIC에cpp하나,dr_mp3 vendored헤더동일복사/라이선스유지.
  이전체크포인트변경없음,75main/규칙/렌더링/설정/PCM모듈은74와동일.
- root두백엔드가공통load사용. SDL일치포맷은vector이동,변환기는unique_ptr로정리,
  Windows는바이트배열복사/WAVEFORMATEX. API크기/프레임배수/Get길이검사는유지,
  정수핸들범위검사추가. 실제resize/push_back구간전체를try로감싸할당실패핸들0.
  CMake게임공통source에mp3_decode.cpp추가. dr타입MakeWaveFormat인자는std::uint32_t.
  74root검사스크립트는새공통모듈이있으면75root검사로현재코드검사연결.
- DeepSeek075-decode/075-backends종료0. 후보optional==0/namespace/stdexcept누락교정,
  noexcept load만catch하고후속할당은밖에두던후보try범위교정,실패clear가capacity를남기는것을
  빈Result반환으로보강. 원고reference가notes에들어간3절을생성기실패로발견후교정,
  최종새번들로DOM/정적검사재실행. 4096-4000수치오타도96으로교정.
- Part5전체편의API는선행총량스캔후단일할당이라는설명을실제블록/증분realloc로교정.
  전체디코드면동기화/실시간오류0·효과음무조건우월·스트리밍은콜백디코딩필수단정제거.
  헤더가드는서로다른번역단위중복정의도완화한다는오개념교정. 현재로더발췌/오류표/흐름도/
  공통파일계약·WAVEFORMATEX와파일fmt메모리동일단정교정. Part10/13CMake발췌갱신.
  실제BGM은44100Hz2ch5292000frames21168000bytes120초로30MB설명교정.
- scripts/check_learning_mp3.py 종료0:Release SCRIPTED81/SDL82CTest·누적tetris·경고0.
  편의API와rotate전체샘플동등·입력/출력정확경계·오류후빈저장소·없는/빈/텍스트파일·
  확장자와바이트분리·200잡음입력·56디코딩가능잘린접두,ASan/UBSan통과.
  실제SDL백엔드+공통디코더:7할당실패/9성공주입·Get오류/짧은읽기/정렬거절·
  FILE descriptor/변환기정리·rotate/clear/music3실제에셋·dummy장치.
  root파일wrapper의seek/부분읽기/후행바이트대역주입에서io/FILE정리확인.
  root실제tetris빌드·17CTest·sim_hash_dump골든유지.
- 후속 check_learning_mp3_root.py 최종종료0:위검사와함께실제Windows로더본문/
  MakeWaveFormat을Linux대역API타입으로컴파일,3실제에셋의PCM바이트/형식동일과
  작은대역API상한거절확인. Windows네이티브ABI/DLL/장치검증아님.
  수동GUI·실제스피커·Windows/macOS네이티브실행미검증. 모든실험범위를기록하고본문작업보고없음.
- 공식dr_libs저장소·vendored구현·CSIRO MPEG보고서대조,관련설명옆링크.
  원문coverage344:covered3/partial82/unassigned259/needs-review0. Part5대응6절부분,
  오디오전체를완료로올리지않았다. 원문28/소스178.
- 최종local release **a8c80f3f0270fa7d**,15파일·재현가능ZIP. Part발췌·Markdown27사례·
  369객관식·DOM단일마운트/탐색/기록/참조/복사·정적묶음검사통과.
  HTTP18767최종lessons.js5420586/library.js5289784바이트일치·lesson75존재확인.
- 공개release708d2e519c17c011 유지. 커밋/푸시/배포없음. 외부작업/최종검사모두종료0,
  로컬HTTP만실행중. 작은차시완료로 전체goal을complete처리하지 않는다.

### 74차시 완성 기록

- 2026-09-29: 직전73완성턴은 progress, 이번74도 누적 구현·루트/Part 교정·검수로 progress.
  **로컬1~74차시·364문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **75차시 “MP3 디코드: 압축 파일에서 샘플로”**. 74-pcm에서 누적한다.
  075.json/75체크포인트는 아직 없다. audio/audio.cpp·sdl_audio.cpp·dr_mp3와Part5를 읽고,
  파일바이트→디코더→Pcm16 소유권/오류 경계를 작게 구현한다. 74는CPU모듈/pcm_probe만추가,
  main/규칙/렌더러/설정은73과동일하며 학습 장치재생은없다. 공개55동결·local계속.
- 74:15절·5문제·접힌전체파일5개. 샘플링/양자화·signed16·채널/PCM프레임/스칼라샘플·
  인터리브·시간/메모리/bitrate·Nyquist/aliasing·레이트변경vs리샘플링을수치예로설명.
  학습record가져오기/내보내기는복구하지않는다. 일반파형도구이며학습기록기능과무관.
- audio/pcm_layout.h는root와학습동일. 채널1/2,rate8000..192000은앱지원정책.
  size_t바이트예산/(channels*2)로uint64frames를검사한뒤캐스트/곱셈; 할당없음.
  Pcm16::make는이미전달받은vector의완성프레임·16MiB보관예산검사/포맷과함께소유.
  counts는실제배열기반조회; moved-from에오래된캐시없음. at은frame/channel각각검사.
  대입은값매개변수후swap으로복사할당실패시포맷만바뀌는부분대입방지.
- make_reference_tone:44100Hz2채널44100frames,정수삼각파주기100,phase0/25/50/75/100에
  0/6400/0/-6400/0. L/R복제·기본주파수441Hz·88200samples·176400bytes·1초.
  CMake study_pcm INTERFACE,pcm_probe·pcm_contract추가. 스피커재생/WAV출력없음.
- 루트 audio_load_sound 두백엔드는널/빈경로거절,공통layout호출후복사·캐스트.
  SDL은INT_MAX바이트한도;변환출력완성프레임확인·Get실제반환길이일치검사.
  XAudio2는XAUDIO2_MAX_BUFFER_BYTES;fseek오류검사추가. MakeWaveFormat은검증뒤사용.
  Part5현재전체로더발췌/단위표/크기경계보강. XAudio2는PCM만지원한다는단정과
  모노복제는항상체감+3dB라반드시보정한다는단정을지원경로/전력지표/청감조건으로교정.
- 남은실제루트검토점:크기검사는전체디코딩뒤이며압축파일읽기/디코더내부할당을제한하지않는다.
  fread후vector예외·디코더malloc후vector복사예외에서자원정리경계도75에서검토한다.
  이턴의범위검사로전체오디오자원어뷰징방어가완성됐다고쓰지않았다.
- DeepSeek074-pcm/074-outline종료0. 최초첨부경로오류뒤승인자료를올바른경로로재실행.
  초안1100Hz→-100Hz예시를sin주기+100Hz로교정,Nyquist경계/양자화축구별,
  정규화대칭·리샘플링항상새배열·API차이는vector타입때문이라는단정제거.
  공개mutable초안은실제private생성경계로통일,이동후Layout캐시/복사부분대입보강,
  파형생성의할당실패가능성을주석에반영,모든정답a초안대신5사례/선택순서재작성.
- 최종 scripts/check_learning_pcm.py 종료0:Release SCRIPTED79/SDL80CTest·누적tetris·경고0.
  16MiB/반프레임/서로다른좌우/32비트모형/API예산/UINT64_MAX경계·44100파형·ASan/UBSan.
  실제전역new실패로복사대입후원래rate/channel/샘플유지확인.
  실제SDL로더함수+실제변환기로Get오류/짧은읽기·출력정렬·직접복사/거대길이검사.
  Get반환검사를제거한반례는잘못된핸들을등록함을확인(전체이전버전실행으로표현하지않음).
  XAudio로더/MakeWaveFormat은Linux대역타입과작은API한도로검사;Windows네이티브아님.
  전체SDL백엔드+실제rotate.mp3+dummy장치init/play/unload/shutdown통과.
  root실제tetris빌드·17CTest·sim_hash_dump골든유지. GUI/스피커/타OS네이티브미검증.
- Microsoft WAVEFORMATEX/XAUDIO2_BUFFER·SDL2 Put/Get·MIT OCW Sampling공식자료대조,
  학습본문관련설명옆링크. 사이트작업보고와미검증OS는본문에넣지않았다.
- 최종local release **45bec90c29433e7f**,15파일·재현가능ZIP. 원문28/소스176,
  coverage344:covered3/partial78/unassigned263/needs-review0. PCM관련Part5두절부분대응,
  디코더/장치/콜백/믹서전체를완료로올리지않았다. Part발췌·Markdown27사례·364객관식·
  DOM단일마운트/탐색/기록/참조/복사·정적ZIP검사통과. HTTP18767최종번들바이트일치.
- 공개release708d2e519c17c011 유지. 커밋/푸시/배포없음. 외부작업/최종검사모두종료0,
  로컬HTTP만실행중. 작은차시완료로 전체goal을complete처리하지 않는다.

### 73차시 완성 기록

- 2026-09-29: 직전72완성턴은 progress, 이번73도 누적 구현·루트/Part 교정·검수로 progress.
  **로컬1~73차시·359문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **74차시 “PCM과 샘플: 소리의 데이터 표현”**. 이미지·글자·UI 묶음은73까지,
  소리 단원 시작. 73-settings에서 누적한다. root audio/audio.h·audio/audio.cpp(XAudio2)·
  audio/sdl_audio.cpp·Part5·Part11오디오절을 읽고 PCM표현/샘플·채널·프레임·시간/바이트수의
  최소 기능을 정한다. 장치·디코딩·믹서 전체를 한 차시에 넣지 말고 편성안 의존관계를 확인.
  074.json/74체크포인트는 아직 없다. 별도공개요청없이local진행, 현재public55동결유지.
  학습사이트 답안 가져오기/내보내기는 삭제요청 우선이며 다시 만들지 않는다.
- 73:15절·5문제·접힌 전체파일9개. settings/config.h는version1필수·선택키기본값·
  문서전체후보검증(duplicate/unknown/invalid/unsupported거절), 전체4096/물리줄256바이트,
  CRLF/EOF/#주석/ASCIItrim·NUL거절, 소유하는캐릭터ID·고정순서encode를 구현했다.
- store.h는loaded/missing/invalid/io_error와값을분리, Session은loaded/missing만저장허용.
  invalid/io_error도실행중기본값으로조작하되원본보호. 실제Action변경때만저장,실패후메모리값유지.
  main은study-settings.cfg(작업디렉터리)Session사용. 보호해제는파일수정/이름보관후재시작.
  자동백업/삭제/외부변경감지/다중작성자병합없음. 현재게이트는학습에만있고루트와구별.
- 학습 settings/private_file.cpp는루트meta/private_file.cpp writer와함수바이트동일(네임스페이스외).
  같은디렉터리배타적임시파일·쓰기/fsync/close/rename/dirfsync, Windows권한/Flush/MoveFileEx경로.
  교체전실패old보존, 교체후dirfsync실패new가시+false를구별. 신뢰하는로컬경로조건이며
  적대적인디렉터리경쟁/전원차단/동시편집전체보장을주장하지않는다.
- root main: 숫자접미사/ERANGE/NUL뒤문자거절; long의OS크기차이를피해최종strtoll/longlong사용.
  optionalbool로구형bgm/sfx잘못된값이37→100으로변하던오류수정. 전체16KiB읽기/255물리줄/
  NUL줄거절로fgets조각을새키로해석하던오류수정. 유효한줄만적용하는관대한정책유지.
  save_settings는먼저직렬화후기존private_file writer재사용; 사용자경로부모생성/저장진단유지.
  루트UItoast/손상파일savegate는추가하지않았고오류stderr보고. Part11현재발췌/정책설명갱신.
- DeepSeek073-settings/073-outline종료0. 파서/store구현과구성후보직접검토.
  store save의encode예외도catch안에옮기고Session책임명시. 초안의없는hana·잘못된오류선행순서,
  규칙/RNG상태는절대직렬화불가·동일setter없는예시·쓰기횟수가손상방지라는인과·
  영속성확인이없다는단정·외부전송작업보고를교정했다. 예제캐릭터는실제rook.
- 검증 scripts/check_learning_settings.py: Release SCRIPTED77/SDL78CTest·누적tetris·경고0.
  최초SCRIPTED CMake에서없는tetris타깃연결실패를if(TARGET tetris)로수정후최종통합통과.
  파싱경계/후보롤백/소유ID/4설정roundtrip·실제파일생성/교체/재시작/보호/수동백업복구/
  저장실패후UI값·UBSan/float-cast-overflow. 누적포인터/36위젯/DPI/2755407hit픽셀/
  921600캐릭터픽셀/4915200효과픽셀회귀. root17CTest·sim_hash_dump골든유지.
- check_learning_settings_root.py: 수정전prefix/overflow/NULint/legacy/긴물리줄/NUL줄각각반례,
  수정후통과. RLIMIT_FSIZE0+SIGXFSZ무시로쓰기실패시before는대상비움/after는old보존확인.
  check_learning_settings_faults.py: filefsync/rename실패old보존,dirfsync실패new완성파일+false,
  임시파일정리. 최종strtoll변경뒤rootprobe/fault·실제tetris다시빌드·17CTest/골든재실행0.
  root객체mtime와추출probe에최종strtoll반영확인. 수동GUI/타OS네이티브/전원차단미검증.
- Linux rename/fsync와Microsoft MoveFileExW공식문서대조, 강의의API설명옆링크포함.
  POSIX원문URL은403이라본문근거에사용하지않고Linux man-pages계약으로범위를한정했다.
- 최종local release **5ed1c09496918dfc**,15파일·재현가능ZIP. 원문28/소스175,
  coverage344:covered3/partial76/unassigned265/needs-review0. Part11영속절은부분대응이며
  모니터프리셋/사용자경로전체/마이그레이션/시작시플랫폼적용은남음. Part발췌·Markdown27사례·
  359객관식·DOM탐색/기록/참조/복사·정적ZIP통과. HTTP18767의73번들/library바이트일치.
- 공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 외부작업/최종검사 모두종료0,
  로컬HTTP만실행중. 작은차시완료로 전체goal을complete처리하지 않는다.

### 72차시 완성 기록

- 2026-09-29: 직전71완성턴은 progress, 이번72도 누적 구현·Part 보강·검수로 progress.
  **로컬1~72차시·354문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **73차시 “설정 저장: 기본값·검증·복구”**. 72-idle-animation에서 시작.
  현재 Preferences는 장식 bool와 알려진 캐릭터 ID의 정적 view를 실행 중에 보관한다.
  위상/자원핸들/규칙상태를 설정 파일에 섞지 않고, 현재 src/main.cpp의 load/save_settings와
  Part11을 대조하여 기본값·문법/범위·알려지지않은ID·부분오류·저장실패/복구 계약을 정한다.
  학습사이트의 답안 가져오기/내보내기는 사용자 요청으로 삭제된 기능이며 되살리지 않는다.
  073.json/73체크포인트는 아직 없다. 선택 자료만 DeepSeek, 공개 배포 없이 로컬 진행.
- 72:14절·5문제·접힌 전체파일8개. presentation/idle_animation.h에 주기4초 Clock와
  Animation을 추가. 호출당최대0.1초, 초과시간폐기·음수/비유한값거절·비활성no-op.
  생성자 로컬XorShift64Star두출력→상위53비트/2^53→위상오프셋두개. RNG는생성후소멸,
  sample은시간/난수불변조회. 이진53자리이상double전제를static_assert로명시했다.
- main활성조건은메뉴+장식표시+유효layout. 게임/최소화/설정off는시계정지. 비활성sample은
  alpha1/1·angle30의정적인기본값, 별도표시조건으로선택장식숨김. 복귀는보관위상에이번dt추가.
  테두리알파(.65~1)/일러스트알파(.85~1)/비대화형장식회전만변화. 버튼위치/라벨/hit계약유지.
  art_view에기본opacity1추가, 비유한/[0,1]밖거절. 규칙·위젯·캐릭터표는71과동일.
- root코드는새로수정하지않았다. 현재아바타는platform_get_time의현재위상에서재개,
  상점shopSpin은활성dt누적으로멈춘각도보관. 플랫폼dt상한0.1이면한번빼기wrap은유효하다.
  Part15에이차이와시간폐기·공유RNG소비가블록순서를바꾸는구체원인을보강했다.
  루트SDL의window/drawable1:1잔여제약은유지.71의시각UB수정을중복작업하지않았다.
- DeepSeek072-animation/072-outline종료0. 승인선택자료만사용, 초안은직접검토.
  Clock순수성·sample(false)=숨김·floor오차·플랫폼간수학alpha보장·이미지hitbox·
  학습에없는상점화면사례·클램프면점프없음·비활성검사의잘못된인과를교정했다.
  시간누적반올림/정수양자화·명령조회·정적표시/정지·같은시드/별도상태를구별했다.
- 검증 scripts/check_learning_idle_animation.py 최종종료0: Release SCRIPTED76/SDL77CTest·경고0.
  시간입력/정지/폐기/64000주기반복·이진시간분할·독립사분주기alpha와90도·4096표본프레임·
  반복조회불변. 실제규칙진행600프레임에효과호출수/활성만변경하여hash/phase/기존lock효과RNG
  비교, 실제hash변화와lock발생assert. UBSan/float-cast-overflow통과.
  SDL offscreen/llvmpipe4915200알파/범위픽셀(내부RGB허용오차1), 정적값/회전제출/무효opacity.
  누적캐릭터921600픽셀·포인터2755407표본·36위젯/DPI·실제SDL큐회귀. root17CTest·골든유지.
  rootpresentation기존수정전UB/수정후회귀도통과. 수동GUI·타OS/타GPU비트동일성미검증.
- 최종local release **4b01314b496258b6**,15파일·재현가능ZIP. 원문28/소스175,
  coverage344:covered3/partial75/unassigned266/needs-review0. Part15표현절은72까지부분대응,
  테마파서/보드배치전체는남아있다. Part발췌·Markdown27사례·354객관식·DOM탐색/기록/참조/
  코드복사·정적ZIP통과. HTTP18767의72번들/library실제바이트일치.
- 공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 외부작업/최종검사 모두종료0,
  로컬HTTP만실행중. 작은차시완료로 전체goal을complete처리하지 않는다.

### 71차시 완성 기록

- 2026-09-29: 직전70완성턴은 progress, 이번71도 누적 구현·루트/문서 교정·검수로 progress.
  **로컬1~71차시·349문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **72차시 “대기 애니메이션: 표현용 시계와 난수”**. 71-character-art에서 시작.
  이름·아이콘·일러스트는 캐릭터 표/ArtSet으로 연결됐고 선택은 ID로 유지한다. 표현용 시간과
  규칙 틱/난수의 분리를 실제 작은 대기 효과로 누적한다. root presentation의 비유한 시각과
  큰 값의 각도 overflow는 이번71에서 이미 보강했으므로 같은 수정·설명을 반복하지 않는다.
  072.json/72체크포인트는 아직 없다. 선택 자료만 DeepSeek, 공개 배포 없이 로컬 진행.
- 71:14절·5문제·접힌 전체파일10개. 콘텐츠의 ID·현재 인덱스·표시 이름·경로·GPU핸들을 분리.
  player/rook의 이름·아이콘·일러스트를 같은 표에서 조회한다. 기존 assets/icons의 PNG를 재사용.
  Preferences는 호출자 문자열 대신 정적 리터럴 view를 저장하며 알려진 ID만 허용한다.
  선택은 실행 중 유지되고 파일 저장은 없다. 게임 simulation 트리는70과 바이트동일.
- ArtSet은 ImageStore 핸들을 빌리고 init 동안 정확히 같은 경로의 성공/실패0을 캐시한다.
  파일 실패는 역할별 fallback으로 성공 처리; 후보 완성 뒤 ready. 예외 때 외부 GPU 자원은
  rollback하지 않고 ImageStore 소유로 남는다. main 세션 종료에서 정리한다.
  이름은 공통5개+카탈로그2개 라벨;16 Unicode스칼라/폭/글꼴 제한을 본문에 명시했다.
- image_fit::contain은 int64 교차곱·양수 정수 내림·중앙 배치·좌표범위를 검사한다.
  정수 양자화로 원본 비율과 차이가 남고 얇은 변이0이면 생략한다. art_view는 무효핸들/
  GPU오류를false, 배치생략을true로 처리한다. 메뉴일러스트와 게임아이콘에 같은 선택을 연결.
- root src/image_fit.h를 추가하고 presentation avatar/portrait에서 공유. 잘못된 size/x의
  signedoverflow와 NaN→uchar 변환UB를 수정전 함수로 재현했다. 테두리 전 배치검사,
  isfinite와 주기 fmod 후 각도계산으로 보강. Part15의 현재발췌·내림/양자화/수명 설명 반영.
  모든GUI/서버보안 감사는 아니다. 루트SDL의 window/drawable1:1잔여제약은 유지한다.
- 검증 scripts/check_learning_character_art.py: Release SCRIPTED75/SDL76CTest·경고0.
  실패8조합·경로중복제거·임시문자열선택·예외게시·589824정수fit(75696생략)·UBSan/
  float-cast-overflow. 표재정렬 두빌드에서 rook인덱스1→0/역할핸들유지, 중복ID컴파일거절.
  실제SDL offscreen/llvmpipe PNG·독립역할폴백·921600픽셀가로/세로/생략·오래된핸들거절.
  누적포인터큐·36위젯/DPI사례·2755407픽셀/클릭회귀. root17CTest·sim_hash_dump골든유지.
  최초GL테스트빌드의session_icons헤더누락을 고친 뒤 최종통합검사종료0.
  수동GUI·네이티브Windows/macOS미검증.
- DeepSeek071-content/071-outline 종료0. 초안의 캐릭터ID가규칙판단기준이라는 오개념,
  catalog가문자열소유·모든파일실패ready=false·정확종횡비·rawpointer/stalehandle혼동 교정.
  안정ID/비소유수명/실패캐시/게시범위/정수양자화 중심으로 직접 재구성했다.
- 최종local release **f3c88f08f4088802**,15파일·재현가능ZIP. 원문28/소스175,
  coverage344:covered3/partial75/unassigned266/needs-review0. Part15세절은 부분대응이며
  정책/모델/보상·애니메이션전체까지 완료한 것은 아니다. Part발췌·Markdown27사례·349객관식·
  DOM단일차시/탐색/저장/참조/복사·정적ZIP통과. HTTP18767의71번들/라이브러리바이트일치.
- 공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 외부작업/최종검사 모두종료0,
  로컬HTTP만실행중. 작은차시완료로 전체goal을complete처리하지 않는다.

### 70차시 완성 기록

- 2026-09-29: 직전69완성턴은 progress, 이번70도 누적 구현·루트/문서 교정·검수로 progress.
  **로컬1~70차시·344문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **71차시 “아이콘·일러스트: 콘텐츠와 규칙 경계”**. 70-pointer-mapping에서 시작.
  이미지 디코딩/핸들/배지·폰트·즉시모드 위젯은 이미 있다. 69의 두 아이콘 선택을 되풀이하지
  말고 콘텐츠 식별자·데이터·자원/폴백과 게임 규칙의 경계를 누적 캐릭터 표현에 연결한다.
  현재 assets/src 메뉴·아이콘/아트 설정을 먼저 읽고 구체적인 기능을 정한다.
  071.json/71체크포인트는 아직 없다. 선택 자료만 DeepSeek, 공개 배포 없이 로컬 진행.
- 70:14절·5문제·접힌 전체파일9개. FrameMapping이 window/drawable/logical6값을 보관하고
  make_layout을 한 번 호출해 그림/입력에 같은 결과를 전달한다. 첫 호출·크기/DPI변경·복구
  프레임은 포인터 취소, 유효한 layout은 그리기에 사용한다. 키보드는 별도 경로다.
  SDL SIZE_CHANGED/RESIZED도 raw pointer를 취소해 같은 펌프 A→B→A를 처리한다.
  규칙·설정·위젯·letterbox 변환 공식은 유지한다.
- 실제 반례: raw(240,180)은640×480에서장식(120,90),1200×617에서시작(19.85,70.02)이다.
  FrameMapping으로 새 배치에서 옛 press를 실행하지 않는다. OS원자조회/사건별배치이력을
  보장하는 설계는 아니다. 첫 프레임과 변경 프레임의 클릭을 버리는 정책을 본문에 명시했다.
- root Win32 button down/up6분기가 좌표를 갱신하지 않던 것을 수정했다. 이동사건없이클릭이
  들어오는 반례를 수정전소스로 재현하고 update_mouse_position을 move/down/up에 적용했다.
  부호좌표·capture회귀 검증. Windows네이티브 빌드/창 검증은 수행하지 않았다.
- Part2 현재 window_proc/좌표helper 발췌와 설명 갱신. Part11의 drawable/window혼용,
  전체화면에서만여백·중앙이면위아래여백같음·lround면오차없음 단정을 교정했다.
  **루트SDL은 여전히 window/drawable1:1전제**다. 별도drawable조회와 크기변경취소는 학습의
  계약이며 현재루트가 동일한HiDPI범위를지원한다고 쓰지 않는다. 고밀도지원확장의 잔여 제약.
- 검증 scripts/check_learning_pointer_mapping.py: Release SCRIPTED74/SDL75CTest·경고0.
  독립viewport표·정수유리식204834hit비교·경계제외0·내부점왕복·정확경계·여섯치수단독변화,
  최소화/복구/키보드독립·UBSan/float-cast-overflow. 같은펌프크기왕복과뒤press취소SDL실제큐.
  실제offscreen/llvmpipe GL 픽셀중심2755407개·내부43568·경계제외0에서hit일치(통제5크기쌍).
  69렌더36조합·7라벨/atlas수명회귀, root포인터검사·17CTest·sim_hash_dump골든유지.
  마지막6치수검사 보강 후 양쪽 계약타깃과 sanitizer 재실행. 수동GUI/실제모니터DPI/타OS미검증.
- DeepSeek070-mapping/outline종료0. Dimensions를 private로 옮기고 장황한주석축소.
  원고의잘못된 SDL_GetDrawableSize API, 정/역방향혼동, 프레임값으로모든클릭재생가능,
  새position이면취소한다는오해를교정했다. CS는단위·아핀역순·정수배치·시간기준·oracle로연결.
- 최종local release **4f87041cef8d6659**,15파일·재현가능ZIP. 원문28/소스174,
  coverage344:covered3/partial72/unassigned269/needs-review0. Part발췌·Markdown27사례·
  344객관식·DOM단일차시/탐색/저장/참조/복사·정적ZIP재현성 통과. HTTP18767의70번들바이트일치.
- 공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 외부작업/빌드/검사 모두종료0,
  로컬HTTP만실행중. 작은차시완료로 전체goal을complete처리하지 않는다.

### 69차시 완성 기록

- 2026-09-29: 직전68완성턴은 progress, 이번69도 누적 구현·루트/문서 교정·검수로 progress.
  **로컬1~69차시·339문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **70차시 “마우스 역매핑: 그림과 클릭을 일치”**. 69-widget-state에서 시작한다.
  68에서 window/drawable/logical 포인터 매핑을 이미 만들었으므로 단순 반복을 피하고,
  root platform/mouse_coordinates.h·letterbox·크기 변경 시 같은 배치 사용 계약을 대조하여
  실제 그림/클릭 불일치 사례와 독립 왕복/경계 검증을 중심으로 누적 구현을 설계한다.
  070.json/70체크포인트는 아직 없다. 공개 배포 없이 로컬 진행, 선택 자료만 DeepSeek 사용.
- 69:14절·5문제·접힌 전체파일7개. 장식 표시 checkbox와 기존 두 아이콘 선택기를 누적 메뉴에
  추가했다. Preferences는 run_session 수명, Focus는 메뉴 재진입 시 start로 초기화한다.
  checkbox는 현재 값+반전 요청, selector는 방향/활성 상태를 반환한다. evaluate는 Action
  하나를 반환, main은 한 번 apply하고 최신 값으로 그린다. 규칙/자원/설정의 소유를 분리한다.
- 마우스의 유효한 위젯 press 우선, 선택기 중앙/비활성 끝점도 프레임 소비. 포인터 사용 불가는
  키보드를 막지 않으며 키보드 취소는 전체 메뉴 명령을 취소한다. 양방향 키는 상쇄한다.
  렌더 함수는 설정 변경을 하지 않는다. Labels는 일곱 라벨을 묶어 준비하며 atlas reset 전
  참조를 정리한다. 선택은 실행 중 유지되며 파일 저장은 이번 구현 범위가 아니다.
- 실제 루트 수정: checkbox의 size+gap+measure 정수 overflow 재현 후 int64 검사·무효 크기 거절.
  selector 화살표 중첩을 재현 후 h>6/w>2h·파생 좌표 검사. 호출자가 allowPrevious/allowNext를
  전달해 사용할 수 없는 끝 화살표를 숨기고 입력도 막는다. main의 Window설정 행에 적용.
  Part3 GUI와 Part11 위젯/설정 호출 발췌를 갱신하고 ‘즉시모드면 동기화 버그 없음’ 단정을 교정.
- 검증 scripts/check_learning_widgets.py: Release SCRIPTED73/SDL74CTest·경고0,
  독립 키/포커스/입력 채널 표768사례·UBSan/float-cast-overflow·설정 수명/규칙 해시 분리.
  SDL 실제 큐 pointer회귀, 루트 위젯 수정전 UB/겹침·수정후16764좌표/활성 조합·UBSan.
  실제 SDL offscreen/llvmpipe 단일 버퍼 시험 컨텍스트에서36설정/포커스/DPI조합·일곱 라벨·
  reset/revision 가드·GL오류없음. 실제 앱 이중 버퍼 요구 유지; 수동GUI/타OS네이티브 미검증.
  root17CTest·sim_hash_dump골든 유지. 체크포인트 기존 파일은 허용한 통합 변경 외 동일.
- DeepSeek069-widgets/069-outline 완료. 무상태 일반화·모든 마우스 입력 우선·모든 화면 전이에서
  focus 초기화·기본값 반환만으로 안전한 렌더 보장 표현을 교정했다. 멱등성/실제 변경 여부/
  공유 atlas와 라벨 준비 범위를 보강하고 문제를 시나리오로 재작성했다.
- 최종local release **1ab3d76c00696716**,15파일·재현가능 ZIP. 원문28/소스174,
  coverage344:covered3/partial72/unassigned269/needs-review0. Part발췌·Markdown·339객관식·
  DOM단일차시/이동/저장/참조/코드복사·정적ZIP재현성 통과. HTTP18767의 lessons.js 바이트 일치.
- 완료✓·브라우저저장·이전 읽던 위치 이동은 기존 반영 상태를 확인했고 DOM회귀로 재검증했다.
  공개release708d2e519c17c011 유지. 커밋/푸시/배포 없음. 외부작업/빌드/검사 모두종료0,
  로컬HTTP만실행중. 작은 차시 완료로 전체 goal을 complete 처리하지 않는다.

### 68차시 완성 기록

- 직전67차시 완성 턴은 progress였다. 이번68차시도 누적 UI·루트 오류 수정·원고·검수로 progress.
  **로컬1~68차시·334문제**, 공개1~55차시·269문제. 전체177차시 목표는 미완료다.
- 다음 시작점: **69차시 “버튼·선택기: 상태 소유 위치”**. 68-immediate-ui에서 시작한다.
  메뉴의 단일 시작 버튼은 이미 구현했다. 단순 버튼/edge/좌표 내용을 반복하지 않고,
  체크 값·선택 인덱스를 호출자가 보관하고 위젯의 의도를 적용하는 흐름을 누적 UI에 추가한다.
  src/gui.cpp의 gui_checkbox/gui_value_selector와 호출부를 확인해 작은 메뉴 선택 기능을 고른다.
  069.json/69체크포인트는 아직 없다. 승인한 선택 명세만 DeepSeek 사용, 직접 검수.
- 68:14절·5문제·접힌 전체파일7개. 즉시모드와 상태 소유·반복 비용·GL명령 실행의 구별,
  level/edge·프레임 사건 압축·현재 위치/최초press 위치·optional·좌표 변환·반열린 경계,
  순수 판정/앱 명령/그리기와 중복 dispatch를 설명한다. Space도 기존 confirm 경로 유지.
- 기준코드: pointer_edges.h가 프레임 내 첫 왼쪽press와 held를 누적한다. sdl.cpp는 이 창의
  사건을 필터하고 focus loss/leave/minimize/종료를 취소한다. platform_window_pointer는
  최종 커서 위치와 press origin을 결합한다. immediate_ui.h는 순수 매핑/판정이다.
  main은 전이 전 메뉴에서 시작 클릭을 한 번 confirm으로 전달하고 전이 후 화면을 그린다.
  시작 버튼은 (14,64,64,28), 캐시 라벨 ‘시작’. 한 press당 발동, release/capture/겹침 우선순위는
  별도 계약이다. SCRIPTED/WIN32 학습 backend는 pointer unavailable이고 누적 그래픽은 SDL이다.
- 실제오류수정: gui_hover_rect의 int x+w/y+h overflow를 int64 덧셈·양수 크기 검사로 교정.
  root SDL/Win32의 mouse snapshot차이 방식을 KeyEdges<3> 사건 누적으로 바꾸어 같은 펌프
  down/up를 보존한다. init/shutdown reset. Win32 마지막 held 해제 때만 capture 반환,
  정상 ReleaseCapture의 WM_CAPTURECHANGED는 엣지를 유지하고 강제 상실은 취소한다.
  root mouse 좌표는 최종 위치이며 학습의 최초press 위치 저장과 다름을 원고에서 설명한다.
- Part2 현재 발췌·mouse edge/capture 설명, Part3 GUI 무상태/추가 비용0 단정·경계/겹침 설명,
  Part5 종료 발췌 reset을 갱신했다. gui.h 주석도 상태 소유와 실제 눌림 색으로 교정.
  관련 부분 대응 해시 재검토. 68매핑은 Part2입력/SDL과 Part3UI에 부분 대응으로 추가했다.
- 검증 scripts/check_learning_immediate_ui.py: Release SCRIPTED72/SDL73CTest·경고0,
  순수 UI6,456,681 반정수 좌표 oracle/edge/취소/HiDPI/letterbox·UBSan/float-cast-overflow.
  학습SDL 실제 사건 큐+통제한 최종커서/포커스의 입력전용 세션에서 빠른클릭·다른창/버튼·취소·
  종료·메뉴 confirm후 규칙틱없음 검증. root hit 수정전 UB재현/수정후531,441독립 차이식 조합.
  root SDL빠른클릭 누락 전후 실제 사건큐·포커스 회귀; Win32소스분기 추출+capture API모형
  정상해제/복수버튼/강제상실·UBSan; root17CTest·sim_hash_dump골든 유지.
- 첫 pointer_sdl 검사는 offscreen GL의 doublebuffer=0으로 platform_init에서 실패했다.
  생산 코드의 doublebuffer 요구를 낮추지 않고 입력전용 SDL 세션 fixture로 분리해 통과했다.
  이는 실제 데스크톱 포커스·GL초기화 검증이 아니다. 수동GUI·타OS네이티브 미검증.
- DeepSeek068-ui-input 초안의 ‘상태기계없음’/native pixel 단위를 수정했다.
  068-outline은16절·5문제 후보였고 모두a/answer값a 오류를 교정, 용어 나열을 메뉴 구현 흐름으로
  재구성했다. 최종14절,정답c/b/a/d/b. out/learning-jobs/068-*에 원자료/응답 보존.
- 로컬 정적 release1a057820d716ae86,15파일. 원문28/소스174,coverage344:
  covered3/partial70/unassigned271/needs-review0. Part·Markdown·334문제·ZIP재현성 통과.
  DOM 전체차시 마운트·탐색·코드·저장 실패 계약 검사도 통과했다. 공개 release708d2e519c17c011은 그대로다.
- 외부작업·전체빌드·회귀·정적/DOM 검사는 모두 종료0. 남은 실행 작업은 로컬 HTTP 서버뿐이다.
  로컬 HTTP127.0.0.1:18767(PID1599057) 제공. 전체 goal complete 처리/커밋/푸시/공개 없음.

### 67차시 완성 기록

아래는67차시종료시점이며 현재 상태는 위를 따른다.

- 2026-09-28 사용자 “계속”에 따라 67차시를 완성했다. 로컬 **1~67차시·329문제**,
  공개 **1~55차시·269문제**. 전체 177차시 목표는 미완료다. 이번 턴은 코드·원고·검수로 progress.
  목표 조회 도구가 반환한 상태는 usageLimited였으며 작은 차시 완료로 complete 처리하지 않았다.
- 다음 시작점: **68차시 “즉시모드 UI: 그리기와 입력 판정”**. 67-font-cache에서 이어 간다.
  68 원고/체크포인트는 아직 만들지 않았다. 포인터의 창→drawable→논리 좌표 변환과
  즉시모드 UI의 매 프레임 기술/입력 판정/상태 소유를 연결하고, 이어지는 버튼 상태 차시와
  겹치지 않도록 작은 기능을 선택한다. 승인한 선택 명세만 DeepSeek에 맡기고 직접 검수한다.
- 67: 16절·5문제·기본 접힘 전체 파일8개. 키 동등성·메모이제이션·양자화·논리/비트맵 단위,
  Font 소유·공유 스냅샷·CPU/GPU 캐시 무효화·자원/실패 정책·누적 main 갱신 순서를 설명한다.
- 기준 코드: Font 기존1~128 계약 유지, horizontal과 device1~2048 API/공통 래스터 구현 추가.
  GlyphCache는 Font 독점 소유·64항목·shared_ptr<const CachedGlyph>. GPU 캐시는 스냅샷을
  보유한 채 Region 재사용·atlas revision 무효화. 고정 배열·숨은 자동 clear/축출 없음.
  main은 실제 viewport/logical 높이 비율을 사용하고 정수 굽기 높이가 달라질 때 두 라벨을
  하나의 갱신 단위로 재준비한다. 512×512 페이지, 준비 실패는 세션 오류로 처리한다.
- DeepSeek CPU/GPU 단순 구현과 문제 초안을 검토했다. 잘못된 헤더 경로·개행 리터럴,
  대표 배율을 키 필드로 포함한다고 한 설명·불필요한 bitmap 복사·추측성 성능 단정을 수정했다.
  문제의 요청 메트릭 높이/실제 bitmap 축 혼동·scalar=1 단정·중복 clear 상황을 교정했다.
- 검증: Release SCRIPTED71/SDL72CTest·경고0, root17CTest·sim_hash_dump 골든 유지.
  CPU392독립 메트릭/상자 조합·same integer height hit·재로드/실패 보존·스냅샷 수명·용량/재시도.
  GPU warm 래스터/GL 증가0·논리 pen/baseline/폭 불변·revision/폰트 교체·업로드/clear 실패 회복.
  cache/gpu_cache/glyph 계약 UBSan+float-cast-overflow 통과.
  offscreen GL 30사례·8,406,136픽셀·27,442잉크·6,056경계 제외·RGBA오차2.01바이트.
  실제 모니터간 DPI 전환/수동GUI/타OS는 수행하지 않았다.
- 검증 진입점 scripts/check_learning_font_cache.py와 checkpoint dispatcher의 67 등록 완료.
  1단계 정책/root키충돌 검사 check_learning_font_policy.py는 별도 유지한다.
- Part 원문은 1단계 교정본을 대조했고 이번 턴 새 루트/Part 수정은 없었다. 해당 폰트/아틀라스/
  DPI/배치 절의 부분 대응에 lesson-67을 추가했다. coverage344:covered3/partial70/
  unassigned271/needs-review0. 원문28/현재소스174. 기존 reviewed 체크포인트는 보존했다.
- 최종 로컬 release **7461c123159cf674**,15파일. 329객관식·Markdown·Part·DOM전체차시 탐색·
  코드 참조·저장/파일URL 실패 처리·재현 가능한 정적ZIP 검사 통과.
  공개 release708d2e519c17c011은 변경하지 않았다. 커밋·푸시·배포 없음.
- 외부 작업/빌드/검사는 모두 종료0. 로컬 HTTP 서버가 종료되어 있어 127.0.0.1:18767로
  복구했다(PID1599057,docs/learn 제공,index 바이트 일치). 이 서버만 실행 중이다.
  DeepSeek 작업자료는 out/learning-jobs/067-{cpu-cache,gpu-cache,questions}-*에 보존한다.
- 최신 사용자 계약 유지: 기록 이동 없음·즉시 객관식 해설·접힌 짧은 복습·GUI 확인 필수 아님.
  승인한 선택 자료만 외부 전송, 공개55 이후 로컬 집필. 막힘 없음.

### 67차시 1단계 종료 기록

아래는 크기/키 정책만 완료했을 때의 기록이다. 최신 상태는 위를 따른다.

- 직전66차시목표턴은완성원고/누적코드/검증으로progress였다.이번턴은67차시의크기·키정책,
  실제root키충돌수정·회귀·원문교정을완료해progress다.67차시전체는아직집필중이다.
  검토된로컬강의1~66차시·324문제.공개1~55차시·269문제.전체177차시목표는active.
- 다음시작점:67-font-cache/DESIGN.md의CPU/GPU캐시및배율갱신구현을계속한다.
  현재67체크포인트는66을복사한상태에text/raster_policy.h와font_policy_contract만추가했다.
  main은아직66과같다.067.json미작성,course-plan unit-067=draft.차시완료나전체캐시검증으로
  주장하지않는다.다음에단순캐시초안을승인된선택명세로DeepSeek에맡기고직접통합한다.
- 남은핵심:논리메트릭/물리비트맵API분리,폰트교체/주소재사용/수명과캐시키,
  CPU래스터재사용·GPU Region중복업로드방지·atlas revision무효화,배율변경시라벨재준비,
  측정불변/bitmap상자반올림차이·캐시실패/용량정책,실제DPI픽셀대조와강의·문제작성.
  학습Font의rasterize1~128계약과기존glyph_contract를조용히확장하지않고새굽기API/공통구현을설계한다.
- 완료된순수정책:font_raster::plan(scalar,logical_height,render_scale).양자화1/8,최소배율1,
  논리/굽기요청높이1~2048·원배율유한양수≤2048.곱셈/반올림/정수변환전에검증한다.
  허용영역안에서scalar32/논리16/굽기16필드가겹치지않는다.같은정수굽기높이는재사용한다.
  요청높이는ascent-descent기준이며em이나개별bitmap행수가아니다.초안의오개념주석을수정했다.
- root16/65552높이가uint16절삭으로같은키가되어낡은advance를반환하는문제를실제함수로재현했다.
  rootglyph_for에정책적용,범위밖요청은advance유지/비트맵·캐시생략.실제상자가atlas-2범위에
  드는지stb할당전검사한다.폰트교체시캐시clear와기존실패재시도는유지한다.
  before-text_gl.cpp와before-part3-rendering-and-ui.md는out/learning-checkpoints/67-font-cache-check에보존.
- 정책정수oracle304135성공/1006585거절·packed필드복원·양자화반경계·NaN/무한/거대값·
  FP예외/UBSan/float-cast-overflow통과.실제root키충돌전후·할당전과대상자거절·재시도/캐시적중,
  64/65/66의비트맵/업로드/폰트교체/CPU측정회귀·root빌드17CTest·sim_hash_dump골든통과.
  전체67체크포인트SDL/SCRIPTED/실제GL/DPI검증은아직수행하지않았다.
- Part3현재발췌·크기정책·양자화절대/상대오차·정수높이반올림·같은높이재사용·폰트교체clear를교정했다.
  캐시정리가atlasfull뿐이라는단정과bitmap논리상자가항상같다는단정도수정했다.
  Part13에새root헤더목록추가.기존65부분대응해시만재검토했고67대응완료로올리지않았다.
- 최종로컬releasef726057488a5b20c,15파일.원문28/소스174.학습본문은66차시324문제유지.
  coverage344:covered3/partial70/unassigned271/needs-review0.문제·Part·정적ZIP검증통과.
  공개release708d2e519c17c011은변경없음.이번턴공개·푸시없음.
- 실행중외부작업/빌드/검사없음.막힘없음.검증scripts/check_learning_font_policy.py는67의부분검사다.
  DeepSeek067-raster-policy-{material.txt,events.jsonl,draft.json}보관.원본자료로자동재실행하지않는다.
- 최신사용자계약유지:기록이동없음·객관식즉시해설·접힌짧은복습·GUI스크린샷필수아님.
  선별자료만외부모델사용,공개55후의집필은로컬.작은단계완료로전체목표종료하지않는다.

### 66차시 종료 기록

아래는66차시종료시점기록이다.현재진행중인67의범위와release는위항목을따른다.

- 직전 목표 턴은65차시 완성·현재 코드/원문 수정·검증으로 progress였다. 이번66차시도
  기준 코드·15절·5문제·완성파일6개·사이트 연결과 검수 완료로 progress다.
  로컬1~66차시·324문제. 공개1~55차시·269문제. 전체177차시 목표는 계속 active.
- 다음 시작점:67차시 “폰트 캐시: 해상도와 키 설계”. 기준66-text-layout을 복사한다.
  현재 Paragraph는 매 준비 때 CPU 글리프를 래스터화하고 AtlasText는 중복등록한다.
  같은 모양 재요청·크기 변경·폰트 교체·아틀라스 세대 변경에서 캐시를 구별한다.
  root glyph_for의논리크기/화면배율1/8양자화/dev_px/uint16캐시키/페이지재사용을 대조한다.
  uint16로 잘리는 크기와 과대 크기의 lround→int 변환, DPI 변화의 비용·레이아웃 불변을
  실제 입력으로 확인한다. 66의CPU측정은이미 GPU독립이다. 단순구현은승인된선택명세로DeepSeek사용.
- 66-text-layout은65의CMake/README/main/Font공개·구현만 바꾸고 나머지기준파일을보존한다.
  새Layout은16글리프/17줄의순수수치누적·최종행advance폭·비트맵상자합집합을관리한다.
  Font::kerning은같은폰트glyph index쌍을픽셀보정으로반환한다. Paragraph는최대16스칼라,
  LF지원/CR·TAB거절·빈문자열한줄/후행빈줄유지. AtlasText는계산된pen/baseline을소비한다.
  main은폭을읽어x=14+(64-width)/2로정렬하고baseline82를유지한다.
- root measure_text가glyph_for를불러폭조회만으로비트맵·아틀라스·큐를변경하던의존을제거했다.
  CPU advance+kerning을같은논리크기로누적하며scale조회도루프밖으로이동했다.
  최종반올림은double에서하고INT_MAX초과는제한하여범위밖실수→int변환을피한다.
  draw_text의펜/개행규칙과실제함수추출로대조했다.새캐시키/DPI굽기정책은아직변경하지않았다.
  out/learning-checkpoints/66-text-layout-check/before-text_gl.cpp는덮어쓰지않는다.
- Part3 section7/12/15를재대조했다.공개y는메트릭상단임을명시하고측정의GPU독립/반환범위를
  문서와헤더에반영했다.배처제출시점의오래된원문주석도현재상태·용량·자원경계와일치시켰다.
- ReleaseSCRIPTED68/SDL69/root17CTest·경고0·sim_hash_dump골든유지.
  독립배치2634성공/120거절조합·실패보존/비유한/빈줄.번들kern표113쌍×3높이=339대조.
  UBSan/float-cast-overflow·측정GL무호출/큰폭제한통과.실제GL10사례3064480픽셀·14220잉크·
  7520경계제외·RGBA오차2.01바이트.수동GUI·타OS네이티브는검증하지않았다.
- 최종로컬release24ec75ede4bff4e9,15파일.원문28/소스173.324문제·DOM·Part·Markdown·
  정적ZIP재현성통과.coverage344:covered3/partial70/unassigned271/needs-review0.
  http://127.0.0.1:18767/#lesson-66 에서읽는다.로컬HTTP파일대조통과.
- 공개release708d2e519c17c011/commitb04c0232fe009e3f0ab475f75c908d339c594358을유지한다.
  이번턴공개·푸시없음.56~66로컬집필본.완료✓/이전읽던위치·브라우저저장유지.
- 실행중작업/검사/배포없음.막힌사항없음.검증scripts/check_learning_text_layout.py,
  로그out/learning-checkpoints/66-text-layout-check. DeepSeek자료066-layout-*는out/learning-jobs.
  첫호출은첨부경로오류로terminal실패였고경로수정뒤events-2작업의exit0응답을검수했다.
- 최신사용자계약:기록이동없음·객관식즉시해설·기본접힘복습·화면조작/스크린샷필수아님.
  안정스니펫과가변레퍼런스구분,한차시만렌더링.외부모델에선택자료만전송한다.

### 65차시 종료 기록

아래65차시의다음시작점과release는당시기록이다.현재값은위66차시항목을따른다.

- 직전 목표 작업은 64차시 완성과 65차시 구현 준비로 progress였다. 이번 턴은
  65차시 원고·기준 코드·검수·원문 교정·정적 묶음을 완성했으므로 progress다.
  로컬1~65차시·319문제. 공개1~55차시·269문제. 전체177차시 목표는 계속 active.
- 다음 시작점:66차시 “글자 배치: advance·베이스라인·커닝”. 기준65-atlas를 복사한다.
  Font의 메트릭과 Line의 pen 누적, root measure_text/draw_text의 공통 배치 계약을 대조한다.
  glyph index별 커닝 조회와 음수/양수 조정·advance와 실제 잉크 경계의 차이,
  줄바꿈/기준선 간격·측정과 그리기의 동일 경로를 작은 숫자 예제로 구현한다.
  실제 폰트에서 비영 커닝 쌍을 확인하고 테이블/독립 기준과 대조한다. 67차시의
  DPI별 재굽기·캐시 키 설계를 한꺼번에 넣지 않는다. DeepSeek는 선택된 단순 구현만 맡긴다.
- 65는 순수 Shelf → R8 MaskTexture → GlyphAtlas → AtlasLine 순서다. 16절·5문제와
  기본접힘 완성파일5개를 제공한다. 한 페이지 공유와 캐시/배칭의 책임을 구분한다.
  16×12 수치 배치·outer/ink·반열린 구간·후보 상태·실패 시 커서 보존·네 방향 실제0쓰기,
  UV 경계/텍셀 중심·선형 보간·unpack 상태/PBO·revision과 큐 수명을 설명한다.
- root text_gl.cpp는 재사용 영역의 옛 잉크가 여백 샘플에 섞이던 문제를 수정했다.
  (w+2)×(h+2) padded 타일 전체를 업로드한다. mask_upload.h가 R8 생성/갱신 오류를
  확인하고 관련 GL 상태를 복원한다. 실패한 업로드는 UV/새 커서/캐시를 확정하지 않아
  재시도하며 advance는 유지한다. 페이지 재사용을 이미 시작했다면 캐시 삭제는 되돌리지 않는다.
  before-text_gl.cpp를 out/learning-checkpoints/65-atlas-check에 보존했다.
- SCRIPTED67/SDL68/root17CTest, 기존 sim_hash_dump골든 유지. Shelf는2374성공/
  82746거절·독립 점유 격자/실패 커서·UBSan. Atlas/루트R8 helper 각각 생성25/업로드19
  오류 단계·해제/복원/재시도. root glyph_for 생성/업로드 실패의 간격/해제/캐시/재시도 확인.
  실제 GL6사례1838980픽셀·11254잉크·4220경계 제외, 허용오차2.01바이트.
  clear 세대/실패복구/번호소진과 옛 라벨 거절. 수동GUI·타OS네이티브 검증은 수행하지 않았다.
- Part3 아틀라스/필터/실패 설명·현재 발췌와 Part13 헤더 목록을 갱신했다.
  GL3.3 최소1024를 공식 표6.38과 대조했다. MaxRects 최적 보장·고정문자만의 포화·
  아틀라스 항상 메모리 절약·한 글자 실패 시 나머지 정상 보장 같은 단정을 바로잡았다.
- 최종 로컬 release2428074a1c59d834,15파일. 원문28/소스173.
  coverage344:covered3/partial70/unassigned271/needs-review0. 319문제·DOM·Part·Markdown·
  정적ZIP재현성 검증 통과. http://127.0.0.1:18767/#lesson-65 에서 읽는다.
  로컬HTTP의 index/lessons.js를 실제 파일과 대조했다.
- 공개 release708d2e519c17c011/commitb04c0232fe009e3f0ab475f75c908d339c594358을 유지한다.
  이번 턴은 공개/푸시하지 않았다.56~65는 로컬 집필본이다. 완료✓와 이전 읽던 위치 유지.
- 실행 중 외부 작업·검사·배포 없음. 막힌 사항 없음. DeepSeek065-shelf-*와 검증 로그는
  out/learning-jobs, out/learning-checkpoints/65-atlas-check에 있다.
- 최신 사용자 계약: 기록 이동 없음·즉시 객관식 해설·짧은 기본접힘 복습, 화면 조작/스크린샷
  필수 아님. 안정 스니펫과 가변 코드 레퍼런스를 구분하고 현재 한 차시만 렌더링한다.
  승인된 선택 C++/JS/문서만 외부 모델에 전송하며 키/설정/전체 저장소는 제외한다.

### 64차시까지의 보존 기록

아래는 64차시 종료 시점의 기록이다. 현재 시작점·수량·release는 위의 65차시 기록을 따른다.

- 직전 목표 턴은 63차시 완성과 UI 수정·공개 검증으로 progress였다.
  이번64차시도 구현·16절·5문제·전체 파일5개·검수를 완료해 progress다.
  로컬1~64차시·314문제. 공개1~55차시·269문제. 전체177차시 목표는 계속 active.
- 공개주소 https://rein-arxiv.github.io/Tetris-Multiplayer-RL/
  최신 공개release708d2e519c17c011, commitb04c0232fe009e3f0ab475f75c908d339c594358.
  Pages run35882459789 성공·15파일 HTTP 대조 완료. 이번 턴에는 공개본을 변경하지 않았다.
  단색 ✓와 이전 읽던 위치·브라우저 저장은 유지한다.56~64는 로컬에만 반영한다.
- 다음 시작점:65차시 “아틀라스: 작은 이미지들을 한 텍스처에”. 기준64-glyph를 복사한다.
  현재 TextLine은 글리프마다 RGBA 이미지 핸들을 만든다. 여러 coverage를 한 텍스처에
  놓는 좌표/UV·shelf packing·용량·빈 여백·업로드/실패 경계를 단계적으로 구성한다.
  root pack_glyph/ensure_atlas와 Part3를 대조한다. 단일채널 R8/RED와 샘플링 경로,
  행 정렬·픽셀 저장 상태, 텍스처 재사용과 큐의 이전 UV 수명을 확인한다.
  특히 새 패킹/아틀라스 재사용에서 여백 픽셀과 선형 샘플링 경계를 실제로 검증한다.
  크기별 재굽기·DPI·커닝/멀티라인 측정을 모두 한 차시에 넣지 않는다.
  승인된 선택 명세를 DeepSeek에 맡기고 직접 검수하는 흐름을 유지한다.
- 64는 text/font.h/.cpp의 Font가 bytes+fontinfo를 소유하고 glyph/metrics를 반환한다.
  Line은 최대16스칼라의 단일 행·advance 누적·white straight RGBA를 준비한다.
  TextLine이 이미지 핸들을 소유하며 부분 업로드 실패/예외/파괴에 해제한다.
  main은 높이16의 대기실/플레이를 한번 준비해 baseline82에 그린다. 규칙/입력 유지.
  패키지 폰트·OFL고지·출처/해시·stb트루타입을 포함한다. 신뢰한 자산 전제이며
  악성 폰트 파서 안전성·shaping·fallback·모든 내부 OOM 대응을 주장하지 않는다.
- 64의 미사용 Glyph 배열 멤버가 이동하면서 불확정 bool을 읽던 문제를 UBSan으로
  찾아 멤버와 배열 전체를 값 초기화했다. 초안/최종 설명에 수명·부호 계약을 보강했다.
- root glyph_for의 양수 크기/null 비트맵 캐시 문제를 수정했다. advance 유지·그리기와
  캐시 생략·다음 요청 재시도. renderer_load_font는 옛 글리프 큐를 제출한 뒤
  아틀라스 커서를 초기화한다. 실제 함수 추출로 수정 전후 순서를 검증했다.
  Part3의 GPU 불가능/크기·잉크 경계/누락 글리프/단일draw/예외 단정을 수정했다.
  out/learning-checkpoints/64-glyph-check/before-text_gl.cpp는 덮어쓰지 않는다.
  63 UTF-8·62 거리·61 회전·60 핸들·59 이미지 디코더의 수정 전 자료도 보존한다.
- SCRIPTED65/SDL66/root17CTest·56글리프/메트릭 사례·UBSan·부분 업로드 회수 통과.
  실제 GL6사례:1838980픽셀 대조·8680잉크·4220텍셀경계 제외,RGBA오차2.01바이트.
  소유권 이동/재로드 실패·루트 실패/교체 순서·기존 sim_hash_dump골든 유지.
  수동GUI·타OS 네이티브 검증은 수행하지 않았다. 세부 근거는 REVIEW_LOG.md.
- 최종 로컬 releaseaf57f97a2dfee968,15파일. 원문28/소스172.
  coverage344:covered3/partial68/unassigned273/needs-review0.314문제/DOM/정적ZIP통과.
  실행 중 외부 작업·검사·배포 없음. 막힌 사항 없음.
- 공통 Markdown 변환기 scripts/learning_markdown.py가 한국어 조사/문장부호의 별표
  강조를 처리한다. 코드·이스케이프·HTML 안전성·밑줄 규칙은 유지한다.
  scripts/check_learning_markdown.py를 회귀 검사 때 함께 실행한다.
- 사용자요청대로56차시부터로컬집필을계속한다. 공개본은55차시에서유지하며
  임의로매차시자동업데이트하지않는다. 이후공개갱신은별도배포로수행한다.
- root 회귀 기준 parser를보강했다. python/tests/determinism_reference.py가빈파일·시드순서/
  단계입력·누적틱·종료표식·마지막요약을검증한다. compare_records는공통접두뒤길이도검사.
  test_determinism_crossplatform은초기해시+모든관측필드/개수+최종요약을비교하며,
  네이티브미설치여도기준구조검사는수행한다. 기존골든숫자/규칙/해시는바꾸지않았다.
- 학습골든은JSON LRND-trace/1, rules checkpoint-52/1, hash_format LRND/1.
  mixed-v1 seed1 초기+48호출49행, overflow-v1 초기+3호출4행. 학습mask는매틱제출,
  루트Step은한번SubmitInput후N틱이다. 두시나리오를동일입력으로혼동하지않는다.
  비교기check는읽기전용, capture는별도경로xb모드로기존파일덮어쓰기거절.
- root StateHash는legacy 통신 필드집합을유지한다. DiagnosticStateHashV2는SIMH/2로
  가방을추가한로컬진단이며HASH통신은아직전환하지않았다. 형식협상과legacy가방누락
  제한은남아있다. 학습LRND/1과실제SIMH/2 숫자자체의동등성은요구하지않는다.
- 사용자 최신 요구: 기록 이동 기능 없음, 객관식 즉시 해설·짧은 기본접힘 복습 유지.
  화면 조작/스크린샷 검사는필수가아니다. CS깊이·현재디자인·차시별렌더링·안정스니펫과
  가변루트레퍼런스구분을 유지한다. Part순서를강제하지않고중복/불필요한반박을줄인다.
- 승인된선택C++·문서·JS만DeepSeek에보내고직접검수한다. 키/계정/설정/전체저장소제외.
  승인된55차시완료후첫공개는끝났다. 이후로컬집필을계속하며작은차시완료로목표를종료하지않는다.


### 62차시 완료

- 062.json: 14절·접힌 완성파일4개·5문제. 중심 local/반크기/radius, SDF 부호와
  유클리드 거리, 안쪽 사각형의 거리→r만큼 팽창, 세 점 손계산, smoothstep 다항식,
  논리 전이 폭/기하 밖 조각 부재, 52바이트 정점·flat 설정, 알파 곱과 누적 UI를 집필했다.
- DeepSeek062-distance-{material.txt,events.jsonl,draft.json}: 순수 CPU 거리/마스크 초안만
  승인된 명세로 요청했다. 반복 주석을 통합하고 수학적 거리식과 double 평가의 차이를 보강했다.
  외부 도구 탐색을 금지했으며 키·계정·설정·저장소 전체는 전송하지 않았다. 작업 exit0 확인.
- 62-rounded-corners는61의CMake/README/main/image_store만 변경한다. rounded_distance.h
  CPU 실습, rounded_geometry.h Draw{image,radius}/52바이트 정점, rounded_quad.h GPU 경로를
  추가했다. 위치·UV/tint는 이미지 변환을 사용하고 local은 회전 전 중심 좌표다. UV 반전과 독립.
  radius는0~min(w,h)/2, 유한성 검사, 각 축 크기1e6 정책. 수치 범위와 화면 품질을 구별했다.
- RoundedQuad는 기본보간 local과 flat half/radius, 1-smoothstep(-.5,.5,d)를 알파에 곱한다.
  radius0은 마스크를 생략하고 작은 양수는 유지한다. straight RGB와 Separate 블렌딩은 유지.
  외접 사각형을 확장하지 않으므로 기하 밖 전이까지 그리지 않는다. 정확한 coverage로 주장하지 않는다.
  main은 한번 생성한 흰1×1 텍스처의 둥근 패널 뒤에 이미지3개를 그린다. 메뉴5/game6draw.
- 실제 renderer.cpp draw_rect_rounded는 NaN roundness가 비교 clamp를 통과해 NaN radius로
  큐에 들어가던 경로를 수정했다. 양수 크기·유한성을 먼저 검사한다. 유한 clamp/논리 radius<1
  직각 근사 정책은 유지한다. 주석의 수학적 동일 단정과 물리1픽셀 표현을 교정했다.
  before-renderer.cpp를 out/learning-checkpoints/62-rounded-corners-check에 보존했다.
- CPU236196개 입력을 별도 선분/원호 최소거리 기준과 대조, 부호/대칭·마스크 수치/단조성·
  비유한/범위·local/UV 독립을 검사했다. UBSan 및 거리부호/마스크 반전 mutation 거부.
  RoundedQuad대역/UBSan:52바이트 local32/half40/radius48·초기화5/draw17단계 실패·
  자원/속성 오류·입력검증전GL무호출·업로드실패후제출억제·재시도/해제 확인.
- Release SCRIPTED61/SDL62CTest·누적게임/데모빌드·경고0. SDLoffscreen llvmpipeLLVM21.1.8의
  8요청×3Layout=24사례: 독립 역변환/구간별거리/샘플/알파 기준으로7371983픽셀 검사,
  817기하/텍셀 경계 제외,7627 smooth mask 샘플. 내부 허용오차2.01바이트·외부정확보존.
  실제root roundness 수정전후·회전/핸들 실패정리 회귀·루트16CTest·sim_hash_dump골든 통과.
  수동GUI/타OS네이티브/실제GPU OOM 검증은 아님. Part3 현재발췌/단위/거리식 설명 동기화.
- 304문제·차시탐색/답안·Part·재현가능정적ZIP 검증 통과. 최종 로컬release d1a6b4f477cb1d0e,
  원문28/소스170,coverage344:covered3/partial67/unassigned274/needs-review0.
  검사로그는62-rounded-corners-check/{verification.log,pixels.log,site-verification.log,dom.log,
  root-before.log,root-after.log,ctest-SCRIPTED.log,ctest-SDL.log,root-ctest.log}.

### 사용자 후속: 한국어 괄호/인라인 코드 강조 수정과 공개 반영

- **용어(설명)**은·**`ptr`**를에서 CommonMark flanking 규칙으로 강조가 닫히지 않는 문제를
  재현했다. DeepSeek markdown-ko 명세/초안/응답을 out/learning-jobs에 보관했다. 직접 검토 후
  scripts/learning_markdown.py를 공통 변환기로 적용했다. 한국어와 문장부호 사이의 별표만
  추가로 열기/닫기를 허용한다. 기본 delimiter pairing/postprocess·underscore 규칙은 유지한다.
  HTML 문자열 정규식 재해석이나 zero-width 문자 주입은 사용하지 않는다.
- check_learning_markdown.py:14한국어/중첩/리터럴/HTML안전성 사례와13기존규칙 보존 통과.
  두 생성기와 CI·집필지침 연결. 코드/fenced code/이스케이프·javascript 링크 차단 유지.
- 공개55차시는8acbdcbe49fc390c의 원문/문제/소스스냅샷을 유지하고 강조 태그만 고쳤다.
  같은 원문을 기존/새 파서로 만든 HTML 조각을 정확히 매칭해24곳 교체, 미매칭0.
  code/pre 내용·강조 외 DOM구조·문장 내용·HTML 외 메타데이터가 동일함을 검사했다.
  남은 본문 **는 실제 void** 표기 한 곳이며 유지했다. 56~62차시와 새 게임 소스는 공개하지 않았다.
- 공개release892bdb30bf0a140b, gh-pages commit2bee496d44eed798fab4cfa940142d50debc2b58.
  공개 DOM회귀 통과 후5파일(index/library/lessons/site-config/site-manifest)만 커밋·일반push.
  Pages run35878118485 성공,루트200과15파일HTTP바이트 일치. PUBLICATION.json 갱신.
  준비/검증기록 out/learning-markdown-fix/{publication-prepared.json,public-dom.log};
  배포용ZIP out/learning-site/892bdb30bf0a140b.zip. main 작업폴더는 커밋/푸시하지 않았다.
- 기본shell은mountinfo 오류라 승인된require_escalated로 진행했다. 자동승인거절 없음.
  실행 중 DeepSeek/검사/배포 작업 없음. 전체 목표 active, 다음63차시 UTF-8.

### 61차시 완료

- 061.json: 14절·접힌 완성 파일 4개·5문제. 목적지/UV/텍셀 중심, 부분 선택·반전,
  pivot 이동→회전→복원, y-down 방향, 논리→NDC, 아핀 보간, 정점 ABI, tint와 알파 합성,
  검증/업로드/제출 실패 경계, 저장소 오버로드와 누적 게임의 세 이미지 표현을 집필했다.
- DeepSeek061-geometry-{material.txt,events.jsonl,draft.json}: 승인된 선택 명세로 순수 기하
  helper 초안만 받았다. std::offsetof 수식·header static helper의 ODR 경계·NDC 주석을
  교정하고 직접 검토했다. 설정·키·저장소 전체 전송 및 추가 외부 탐색은 없었다.
- 61-image-transform은 60의 CMake/README/main/image_store.h만 변경하고 나머지 기준
  파일을 유지한다. 새 image_geometry.h는 Draw(rect/uv/tint/pivot/angle) 검증, double 중간
  계산, remainder(angle,360), float 범위 확인과 정점6개 생성을 담당한다. UV 역순/동일
  끝점과 화면밖 위치를 허용한다. 양수 크기가 항상 서로 다른 float 정점이 된다고 보장하지 않는다.
- ImageQuad는 position2/UV2/tint4의 32바이트 정점과 192바이트 VBO 갱신, sampled*tint,
  BlendFuncSeparate(SRC_ALPHA,ONE_MINUS_SRC_ALPHA,ONE,ONE_MINUS_SRC_ALPHA)를 사용한다.
  바인딩/업로드/상태 실패 뒤 DrawArrays를 억제한다. 정상 종료는 VAO/VBO/program/유닛0
  텍스처를 비우고 blend를 끈다. viewport/clip·다른 테스트·sampler/write mask는 패스 계약이며
  임의 GL 상태를 전부 복원하는 API가 아니다. 학습 이미지 저장소의 소유/핸들은 유지한다.
- main은 메뉴/게임 핸들 하나로 기울어진 전체 아이콘, 위쪽 절반+tint, 반투명 회전을 표시한다.
  컬러 Stream.finish→이미지3draw→present. 메뉴4/game5draw, opaque 통계는 컬러만 집계.
  게임 규칙/골든·입력 기록은 바꾸지 않았다. 이미지 픽셀도 매 프레임 다시 만들지 않는다.
- 실제 renderer/image_gl.cpp에서 FLOAT_MAX 같은 유한 각도를 float 라디안으로 만들며
  무한대/NaN 정점을 큐에 넣는 오류를 재현했다. NaN/Inf를 거절하고 double·360도 축약을
  먼저 적용하도록 수정했다. image.h에 유한 각도 계약을 명시했다. 수정 전 소스는
  out/learning-checkpoints/61-image-transform-check/before-image_gl.cpp에 보존한다.
- 기하 UBSan: 모서리90도·54 회전/pivot·거리 보존/주기성·UV 반전/상수·tint·45비유한
  필드·표현범위·화면밖 위치 검사. 회전 sin 부호 및 UV 대입 mutation을 계약 검사가 거부.
  ImageQuad 대역/UBSan은 초기화5단계·draw17단계 실패, 추가 자원 생성/속성 오류,
  입력 검증 전 GL 무호출·실패 제출 억제·재시도·정리를 검사했다.
- Release SCRIPTED59/SDL60 CTest·누적 tetris 빌드·경고0. SDL offscreen llvmpipeLLVM21.1.8에서
  8요청×3Layout=24사례: 독립 역변환/NEAREST/tint/알파 기준으로 7,371,983픽셀 검사,
  817경계 픽셀 제외, 내부 허용오차2.01바이트, 외부 정확 보존. premultiplied 배경
  (.025,.05,.075,.25)도 포함한다. 배경 표현을 명확히 한 뒤 transform_real만 재빌드/재실행.
  무효 각도/해제 핸들은 전체 프레임버퍼 불변. 경계 제외를 전체 래스터 일치로 주장하지 않는다.
- 실제 회전 함수 추출로 수정 전 재현/수정 후 큰 유한 각도·NaN/Inf·90도/810도 검증.
  현재 핸들 stale/할당/등록/업로드 실패 정리 회귀, root클라이언트 빌드·16CTest·기존
  sim_hash_dump 골든 통과. 수동 GUI·타OS 네이티브·실제 GPU OOM 검증은 수행하지 않았다.
- Part3 section16의 API 현재 발췌, section18의 회전 함수/UV·tint 설명을 동기화했다.
  GPU 회전 자체가 계단현상을 개선한다는 단정을 NEAREST·래스터 경계 설명으로 교정했다.
  root의 legacy BlendFunc 저장알파와 학습 Separate 알파를 명시적으로 구별했다.
  루트 블렌드 설정은 이번 변경에 포함하지 않았다. Khronos 공식 refpage XML과 대조했다.
- Part 발췌·299객관식·전체 차시 DOM 탐색/복습/정적 소스 연결·재현가능 ZIP 검사 통과.
  로컬 release e5d3cf58998de887, 15파일. 원문28/소스170, coverage344:
  covered3/partial67/unassigned274/needs-review0. 공개55차시는 그대로 유지한다.
  로그: out/learning-checkpoints/61-image-transform-check의 verification.log, pixels.log,
  root-before.log, root-after.log, ctest-SCRIPTED.log, ctest-SDL.log, root-ctest.log.
  전체목표 active. 다음62차시, 실행 중 외부 작업·막힌 사항 없음.

### 60차시 완료

- 060.json:15절・4접힌완성파일・5문제. 핸들/포인터/GL이름/콘텐츠ID,위치재사용과수명,
  64비트분할/시프트,inline공유발급카운터,포화/clear,unique_ptr슬롯,삽입복잡도/커밋,
  공통조회・소유자/비소유복사・GL등록실패・미제출사용・게임두아이콘선택을집필.
  C++draft expr.call/expr.shift에서값매개변수소멸시점과시프트폭조건대조,곁들임출처추가.
- DeepSeek060-pool-{material.txt,events.jsonl,draft.json}:선택명세로순수HandlePool초안.
  실행종료확인.벡터확장전발급소진거절을추가하고std고정폭타입/비재진입계약보강.
  저장소전체/키/설정전송없음.초안추출후직접검토・컴파일/검사.
- 60-image-handles는59의CMake/README/main만변경하며그외기준파일동일유지.
  추가handle_pool.h・image_store.h・session_icons.h.풀은GL독립/소유unique_ptr/
  token상위stamp+하위slot1,공유counter는템플릿밖inline.0/범위/번호/객체존재공통검사.
  clear는메타데이터를비우고counter유지.할당실패가번호를소비하지않게순서설계.
  같은풀을소멸자/콜백에서재진입변경하지않는계약.스레드안전/영구ID/보안토큰주장없음.
- ImageStore는load/create→0,query/draw/unload→bool.실패size출력보존.
  Texture소멸자가등록실패도정리.메뉴PNG/플레이주황두이미지동시소유,화면상태로선택.
  FixedQuad즉시draw와컬러배치경계유지.게임규칙/골든변경없음.
- root ImageHandle은uint64_t.호출부는ImageHandle/auto로보관해타입적용,프로토콜/저장ID로
  쓰지않음을확인. intABI와달라재빌드필요를Part명시. image_size/unload/tinted/rotated가
  풀의전체토큰조회공유. image_init은lazy/no-op,shutdown은GL정리후pool.clear.
  root ImageEntry는plain메타데이터이므로풀등록실패시생성함수가GL삭제를담당.
- 실제이전함수추출에서A삭제→B같은슬롯→옛A조회/해제로B오삭제재현.
  현재함수추출+actualpool/upload helper에서stale조회/기본・회전draw/해제/재초기화거절.
  Entry할당bad_alloc과업로드후슬롯할당bad_alloc・모든GL연산실패・최대크기・소진에서
  핸들0/미등록/GL정리.배치대역은미제출사용flush가delete보다앞인지검사.
- 학습/현재풀각각UBSan:512증가후borrowed주소유지,복사/stale/다른풀・T/번역단위,
  zero halves/범위밖/clear/소진・생존개수・할당실패정리.번호비교제거mutation거부.
  초기테스트는insert호출과alive비교를한식에묶어매개변수소멸전관찰로실패했다.
  반환값저장문장뒤검사로수정하고표준계약확인.결함을구현누수로오판하지않음.
  rootprobeinclude경로충돌도study GL대역과actualpool경로를명시해수정.
- ImageStore UBSan/대역:파일/생성/복수이미지/업로드실패/무효자료/출력보존/clear/소멸/
  번호소진등록실패에서도GL이름누수없음.57SCRIPTED/58SDLCTest・경고0・누적tetris빌드.
  테스트의불필요한zero람다를지역변수로단순화후pool_contract만마지막재확인.
- SDLoffscreen llvmpipeLLVM21.1.8:두아이콘각각640×480,639×477,479×479의6프레임,
  아이콘9216/9025/5184픽셀정확・바깥전체RGBA보존.해제뒤draw거절/재로드/clear검사.
  GUI/다른OS/실제GPU OOM검증아님. root클라이언트빌드・16CTest・sim_hash_dump골든유지.
  이전decode예외/BitmapReadLock대역회귀도다시통과.
- Part3section16저장소설명/전체API현재발췌/종료/소유권/실패책임재구성,section18그리기
  조회앞부분현재발췌갱신.60의대응근거는section16에추가;회전교육대응을미리주장하지않음.
  Part13header목록/내부helper설명동기화. 학습본문의이름삭제/바인딩해제혼동표현교정.
- Part발췌/294문제/DOM앵커・복습・정적레퍼런스/재현가능ZIP통과.
  로컬release1ae1df99d9f27acc,원문28/소스170,coverage3covered/67partial/274unassigned.
  로그out/learning-checkpoints/60-image-handles-check/{verification.log,site-verification.log,
  root-before.log,root-after.log,pixels.log,ctest-SCRIPTED.log,ctest-SDL.log,root-ctest.log}.
  공개55는유지. 전체목표active,다음61. 외부작업/막힘없음.

### 59차시 완료

- 059.json:15절・5문제・핵심스니펫과4개접힌완성파일. 파일바이트/픽셀바이트,
  PNG/JPEG형식개념・binary읽기・입출력제한・헤더와전체복원・원본/출력성분수・
  RAII/deleter・예외시출력커밋・signed stride/BGRA・게임GPU연결・실패검사집필.
  집필환경보고는validation/기록에만유지. 표준PNG와CgBI예외는곁들임메모로분리.
- DeepSeek059-row-{material.txt,events.jsonl,draft.json}:선택한행변환명세만전송,
  정수범위/signed stride/deleter검토. 채널이동주석의잘못된R from B등교정.
  키/설정/디코더전체헤더전송없음. 외부작업종료.
- 59-image-decode는58의CMake/README/main만수정하고기존규칙/Texture/Quad유지.
  image_decode는단일cpp에stb v2.30정의,PNG/JPEG/memory API만활성화.
  vendored헤더SHA256/license보존.8MiB입력/8192한변/64MiB출력정책,
  총작업메모리/시간보장과분리. Result가optional소유Image와Error반환.
  stdfilesystem binary/ate→크기→정확읽기/후속바이트검사→info→load(4)→
  unique_ptr(stbi_image_free)→vector복사. 오류/decode내부자원/할당구별한진단.
- player.png는절차적8×8배지와정확히같은256RGBA바이트. main은마지막--image PATH지원,
  시작시1회디코딩/업로드후CPU메모리해제. CMake복사위치와cwd/Windows다중구성설명.
  투명입력픽셀은보존하지만고정Quad는불투명전제인점을본문명시.
- 루트image_rows.h:newpositive/size_t/ptrdiff byte검사와BGRA행복사.
  stride최솟값/절댓값/행offset+폭범위를쓰기전에검사,패딩제외/alpha보존.
  image_gl은stbiunique_ptr로복사예외정리,지역결과후swap으로출력보존.
  Windows는UTF8실패/unsigned크기검사,vector선확보,BitmapReadLock으로
  조기반환/예외/명시해제경로관리. 네이티브Windows빌드증거는없음.
- 수정전실제함수추출+stbi대역에서24바이트vector복사bad_alloc시누수/예외전파재현.
  수정후동일실패에서free/false/기존출력보존,무효크기/디코더실패/정상성공검사.
  실제stb에malloc/free/realloc계수와operatornew실패주입을연결하여학습경로도
  임시할당0/retry성공확인. 두경로UBSan. BitmapReadLock실제추출대역은미획득/
  close/예외/해제실패의정확히1회해제시도를검사. OS GDI실행과혼동하지않음.
- root행helper3952개positive/negative+padding조합과실패출력보존UBSan,
  R/B교환누락mutation거부. PNG RGB/RGBA/grayexact・JPEG±2・입력덮어쓰기/
  확장자/한글경로/빈파일/8MiB초과/8192²헤더/손상/모든prefix샘플검사.
  prefix는전부거부한다고가정하지않고성공시일관된24bytes계약검사.
- Release SCRIPTED55/SDL56CTest,경고0. decodedPNG→GL세Layout전체픽셀비교:
  9216/9025/5184badge픽셀정확,바깥보존. hostileunpack3×2복사도유지.
  SDLoffscreenllvmpipeLLVM21.1.8;GUI/모니터/타OS검증과분리.
  root클라이언트빌드/16CTest・기존sim_hash_dump골든유지.
- Part3이미지디코딩전체발췌/행helper/RAII/크기정책/표준PNG계약갱신.
  Part13헤더목록추가,TTF를GPU에서처리할수없다는단정을이렌더러의CPU방식과
  OpenGL3.3의직접TTF API부재로교정했다.168→169소스,원문28개.
  coverage344:covered3/partial67/unassigned274/needs-review0.
- 로그out/learning-checkpoints/59-image-decode-check/{verification.log,pixels.log,
  root-before.log,root-after.log,ctest-SCRIPTED.log,ctest-SDL.log,root-ctest.log}.
  Part/289객관식/차시탐색・정적코드참조DOM검사통과. 공개55차시는변경없음.
  최종로컬정적release21b12251b9ef11fa,15파일・재현가능ZIP검사통과.
  전체목표active,다음60차시. 실행중작업/막힌사항없음.

### 58차시 완료

- 058.json:15절・15본문 스니펫・5접힌 완성 파일・5문제. 총284문제.
  RGBA8/texel・정수 범위와 비소유 뷰・이름/저장소・unpack 정렬/행/skip/PBO・
  성공 시 소유권 확정・크기와GL실패・필터/mipmap조건・최소UV・sampler유닛・실제게임연결.
  API 설명은 Khronos 공식 glTexImage2D/glPixelStore/glTexParameter XML과 대조했다.
  집필환경 보고는 검수 메타데이터에만 기록했다.
- DeepSeek058-pixels-{material.txt,events.jsonl,draft.json}:선택명세만으로RGBA뷰/8×8아이콘
  두헤더초안. rawJSON통과,논리검토후실제1×2눈을2×2라쓴주석만교정. 외부작업종료.
  API키/설정/저장소전체/디코더헤더전송없음.
- 58-texture-storage는57의CMake/README/main/gl_api.h/cpp/loader_contract만확장.
  rgba_pixels.h는positive/size_t곱범위/정확한길이확인,실제메모리접근성은호출자계약.
  badge_pixels.h는불투명8×8/256bytes,상단왼쪽2픽셀빨강으로방향표시.
  texture.h는immutable1회upload,GLlimit/단계별오류/임시이름해제/정상상태복원후commit.
  현재유닛은유지,Texture2Dbinding・PBO・alignment・rowlength・skiprows/pixels저장복원.
  복원자체실패면false이며원상복구를보장한다고쓰지않음. raw포인터/PBO해석분리.
- texture_quad.h는정점위치2+UV2/stride16/offset8/6정점96bytes,프로그램/VAO/VBO소유.
  GLGetUniformLocation/u_image와유닛0사용. init의바인딩실패뒤BufferData를멈추도록보강.
  unit0 sampler객체없음/불투명전용pipeline.위v0/아래v1로top-down원본방향표시.
  main은stream.finish후전체clip으로badge.draw.원본/정점은루프밖에서한번업로드.
  메뉴2draw/게임3draw,콘솔opaque stream통계는기존색상부분만세는것을본문에명시.
- root renderer/texture_upload.h 추가:크기/size_t곱/GL_MAX_TEXTURE_SIZE・상태저장정규화・
  임시이름업로드/복원/오류정리. image_create_rgba는슬롯을GPU생성전에확보하고예외를0으로
  처리,업로드성공후used등록. ImageHandle최대범위검사. GL함수추가없이상수6개추가.
  image.h에readable bytes/currentcontext/실패0계약명시. CMake IDE헤더목록/Part13동기화.
- 실제함수추출재현:before TexImage2D오류인데used핸들반환. after전단계GL실패에서0・
  임시이름해제・used미등록,실패빈슬롯재사용. Texture대역의25GL연산실패/이름0/진입오류/
  크기제한/hostcopy검사. Root API는길이인수가없어호출자의읽을수있는배열계약을유지한다.
  C++ bad_alloc 강제주입은안했고할당선행구조를검토했다. 실제GPU OOM을유발한검사는아님.
- Windows min/max 매크로와새numeric_limits::max호출의충돌가능성발견,괄호형호출로보호.
  max(a,b)를정의한실제helper/등록함수추출컴파일・UBSan통과. Windows SDK빌드증거아님.
- Part3 현재발췌/설명을업데이트했다. GPU실패・slot선확보・정상unpack복원・입력버퍼범위・
  RGBA4정렬도맞는조건을설명. Part13에서CMake헤더목록이접근제한을강제한다는오개념도수정.
  처음자동교체가선언과정의prefix를혼동한차이는Part검사로발견해정확한코드블록으로교체.
- Release SCRIPTED54/SDL55CTest・누적SDLtetris/CPUdemo・경고0. 학습Texture/Quad UBSan.
  새57개GL슬롯전부누락대역검사,Quad stride/offset/바인딩실패무업로드/정리검사.
  skiprows초기화누락・실패객체공개・실패객체누수 세Release변형을계약검사가거부.
- SDLoffscreen llvmpipeLLVM21.1.8:hostile 유닛2/PBO/alignment8/rowlength19/skip2,3에서
  3×2RGBA24bytes upload/readback일치,CPU배열수정해도저장값유지,이전상태전부복원.
  단일640×480FBO에서640×480,639×477,479×479 Layout을적용하여9216/9025/5184개
  아이콘픽셀정확・바깥전체픽셀보존. native창resize/모니터관찰/타OS검증주장없음.
- root클라이언트빌드・16CTest・기존sim_hash_dump골든유지. 마지막macro교정후실제함수대역과
  root빌드재확인. check_learning_texture.py와전체체크포인트디스패치58연결.
- 344절:covered3/partial66/unassigned275/needs-review0. 원문28/소스168개뷰어.
  로그:out/learning-checkpoints/58-texture-storage-check/verification-final.log,
  pixels.log,root-before.log,root-after.log,root-final-build.log,각mutation/result.log.
  284문제・Part현재발췌・생성물・DOM탐색/코드참조・정적ZIP검사로로컬배포물을검증한다.
- 최종로컬정적release:f4902ec96c865460,15파일・재현가능ZIP검사통과. DOM/코드참조최종검사통과.
- 공개본은55차시그대로이며이번작업은로컬만반영. 실행중DeepSeek없음・막힌사항없음.
  전체177차시목표는active,다음59차시부터이어간다.

### 57차시 완료

- 057.json:15절・15본문 스니펫・5접힌 완성 파일・5문제. 총279문제.
  지연 실행・batch key・CPU값/공유상태/이름과내용・정수 시저 투영・입력 검증・
  용량 경계・실패 고정・명시적 finish・원자성 범위・GameView 연결・이미지 수명 설명.
- 57-flush-boundaries:56에서 CMake/README/main/color_batch.h만 확장하고 기존 파일 보존.
  clip_box.h는 DeepSeek 선택 명세 초안을 검토해 통합, Layout 전제 주석과 승격 위치 보강.
  flush_stream.h는 Sink 참조/Clip/pending을 관리한다. 같은 Clip/빈 큐 무제출, 새 Clip을
  저장하기 전에 flush, 새 그룹 검사 후 잔여 용량 초과 시 flush, 단일 초과 그룹은 거부.
  실패는 sticky, 이전 제출 롤백 없음. 소멸자 제출 없음, finish 명시/멱등, 닫힌 뒤 입력 거부.
- flush_device.h는 기존 Device와 GL Scissor 연결. flush_scene.h는 board/ghost/active
  1248정점과 preview/score108정점을 구분하여 초기 총1356정점・2draw. 메뉴1draw.
  main의 화면 전환・규칙・진단 유지. 유효 Layout마다 Stream 생성, scene/finish 모두 검사.
- DeepSeek 작업은057-clip/057-quiz의 material/events/draft 파일에 보관, 외부 작업 종료.
  quiz 초안의 clip을 append에 넘긴다는 표현・존재하지 않는 Stream.submit・여러 draw
  혼동・ImageHandle과GLuint 혼동 및 해제 순서 설명을 실제 API로 교정, 보기도 균형 조정.
  첫 quiz 작업은 입력 파일 경로 오류로 exit1, 경로 수정 뒤 정상 종료. 승인된 명세만 전송.
- root glb_before_texture_delete:해당 s_batch_tex면 flush 후이름0. image_unload・
  image_shutdown・renderer_text_shutdown에서 삭제 전 호출. 부모 renderer_shutdown은
  자원들이 살아 있는 동안 잔여 큐 먼저 제출. image.h에 렌더 스레드/current context 계약.
  before 함수 추출 대역에서 pending_delete=1, stale submission=1 재현.
  after 실제 함수 추출 대역으로 삭제 전 제출・무효/반복해제・다른 텍스처・GL 이름 재사용・
  image_shutdown을 검사했다. 텍스트/부모 종료는 소스 순서 검사, 실제 GPU root 이미지
  재현을 했다는 주장 없음. 컨텍스트 공유/다중스레드・세대핸들 도입은 범위 밖.
- Part3의 현재 발췌/수명 설명 수정. 이전오프셋 flush 잔재・모든글자1draw 단정・
  텍스트/이미지는GL을직접못만진다는 설명・RAII가GPU를관리못한다는 설명・
  PNG/JPG는GPU디코딩불가 단정을 실제 구현 범위로 교정. image_gl.cpp 주석도 함께 수정.
- Release SCRIPTED52/SDL53 CTests,누적 tetris/데모 빌드,경고0. CPU2000 GameView 구성의
  정점순서/색상/규칙해시 일치. Clip INT_MAX/최소창(1x1은유효Layout없음)/반올림/
  같은Clip/A→B→A/용량/실패보존/finish/no-retry 검사. CPU/root probe UBSan 통과.
  새clip선대입・clear누락・실패고정누락 세Release변형을 계약검사가 거부했다.
- SDL offscreen llvmpipe LLVM21.1.8:진행6/종료1/메뉴 old경로 픽셀차이0.
  세Layout에서 겹치는A→B→A fullquad의 전체RGB를 독립마스크와 비교,모두일치.
  수동GUI/실제모니터・Windows/macOS 네이티브 미검증. 집필 환경 문구는 본문에 넣지 않음.
- root 클라이언트빌드・16CTest・기존sim_hash_dump골든 유지. check_learning_flush.py는
  fresh out에서도 root를구성하도록함. 전체check_learning_checkpoints.py에서누락된
  52~57차시전용검사분기를연결하고실제경로대조. 전체1~57누적모든타깃재실행은하지않음.
- 344절 중 covered3/partial65/unassigned276/needs-review0. 279문제Node・전체로컬차시
  DOM 탐색/단일mount/코드참조/검색/저장・Part현재발췌・생성물최신성검사 통과.
  검증로그:out/learning-checkpoints/57-flush-boundaries-check/verification.log,
  navigation.log,root-before.log,root-after.cpp 및각변형result.log.
- 최종 로컬 정적 release:3cb8bf83bc963667,15파일과재현가능ZIP검사통과.
- 공개본은PUBLICATION.json의55차시release를유지한다. 57 변경은로컬에만있고push없음.
  전체177차시목표는계속active. 막힌사항없음. 다음58차시로이어간다.

### 56차시 완료

- 056.json:14절·14스니펫·5접힌완성파일·5문제. 총274문제.
  배칭조건·호출/전송/정점/픽셀비용·interleaved형식·유효prefix·용량·검사후추가·
  순서·VAO·GPU소유/실패·CPU재구성tradeoff와실제main통합을설명했다.
- DeepSeek의순수CPU Batch초안은논리수정없이검토해통합했다. 응답JSON코드펜스만벗김.
  056-batch-{material.txt,events.jsonl,draft.json}. 띄어쓰기별도056-spacing작업66필드는
  공백외문자·문단수불변을확인해적용했다. 불필요한반박표현과코드식별자표시를직접교정.
  모든외부작업종료. API키/전체저장소전송없음.
- 56-color-batch는55의CMake/README/src/main.cpp외기존파일동일. 새color_batch.h:
  위치2+RGBA4 float,24바이트,stride24색상offset8. capacity2304,현재상한2172.
  count0무호출·3배수·finiteXY/RGB범위·alpha1·검증후복사·실패prefix/used보존.
- batch_scene.h는빈/고정셀→고스트→조각→미리보기→종료X→점수의순서를유지한다.
  완성후에만값배치를교체한다. main은매프레임재구성하고사용자료만업로드한다.
  기존dirtyVBO재사용보다전송/CPU비용이늘수있음을본문명시(속도비율미측정).
- batch_device.h는공통색상Program/VBO/VAO소유,noncopy/nonmove,GL3.3전제.
  비어있지않은배치당한draw. 초기화/바인딩/업로드/제출오류시정리.
  마지막보강으로VBO바인딩실패때BufferData도하지않게하고대역검사추가했다.
  SDL/INPUT전용SCRIPTED타깃을구별한다. 실제tetris는SDL에서빌드한다.
- 실제renderer_set_view_offset의불필요한flush제거. rect/quad모두CPU에오프셋을
  구워정점을저장하므로다른오프셋정점도순서그대로같은배치에제출가능하다.
  renderer.h/gl_internal.h/renderer.cpp의고정draw횟수·잘못된only조건주석갱신.
  Part3배칭/오프셋/비용단정,Part4B.5발췌와설명을함께갱신했다.
- Release SCRIPTED51/SDL52CTest,SDL게임·CPU데모빌드·경고0.
  CPU5000프레임구성·순서/보존/경계·GPU대역stride/offset/usedbytes/1draw/실패정리,
  CPU/Device UBSan·clear누락/alpha검사제거/정점역순3변형거부통과.
- SDLoffscreen llvmpipe(LLVM21.1.8)프레임버퍼:진행6장면(최대점수포함)·종료1·메뉴.
  모든픽셀채널차이0. 초기1356정점32544바이트,기존5draw→1;고정후6→1.
  장면정점/픽셀작업량이줄었다거나실측속도가향상됐다는뜻은아니다.
- 실제root함수본문추출대역의rect/quad오프셋·순서·텍스처변경제출확인,
  루트클라이언트빌드·16CTest·기존골든유지. out/learning-checkpoints/56-color-batch-check.
  batch_real최초링크누락study_triangle수정후전체검증통과. 마지막GPU가드/main정리후
  관련빌드·Device계약·UBSan·픽셀검사재실행통과. GUI/네이티브다른OS는미검증.
- 교재/자료생성·274문항·마지막main완성파일포함전체차시DOM·정적재현성·diff검사통과.
  로컬배포IDa8fdf5b958dcb4ef,15파일,28문서/167소스. 원문344절중covered3/partial61/
  unassigned280이며needs-review0. 공개Pages/PUBLICATION.json은55그대로,배포/푸시없음.
  모든외부/검증프로세스종료,대기중작업없음.

### 55차시 완료·챕터 공개

- 055.json:14절·13스니펫·4접힌완성파일·5문제. 총269문제.
  전이표·흡수상태·소유불변식·생성/해제·전환프레임·입력게이트·실패보존·
  참조수명·캐시무효화·재시작초기조건을실제게임구현으로설명했다.
- DeepSeek의선택명세순수screen_transition.h초안을검토해통합했다.
  out/learning-jobs/055-screen-{material.txt,events.jsonl,draft.json};외부작업종료.
- 55-screen-state는54의CMake/README/src/main.cpp외기존파일동일.
  Application이초기Round기준+optionalGame을소유한다. 메뉴/종료는게임부재,
  finished는최종Game동결,playing만advance한다. back우선,confirm생성프레임즉시반환.
  모든조작키release프레임을거친후활성화;cancelled는0틱보류입력도삭제한다.
  화면변경과Game교체를별도보고하며main은전환후참조재획득·GPU캐시갱신.
  메뉴는기존GL경로의재생삼각형,기존보드/종료X는유지. 폰트/버튼메뉴단계아님.
- 실제main의Single메뉴진입과R재시작을beginSingleRound로통합했다.
  새Game준비후소유자교체,accumulator/보류입력/DAS/콜아웃/흔들림초기화,
  기록중프레임삭제+seed정렬,마지막모드변경. 실제held키는다음프레임정상수집;
  학습의releasegate와다름을본문에명시했다. 봇/Net초기화는건드리지않았다.
- Part4§8/9의“대입뿐/항상/잔재없는리셋”을교정하고함수와수명범위를설명.
  부록D와Part5발췌갱신. Part5는장치참조유지와곡위치유지보장을구별했다.
  두새원문절은55부분대응으로등록(봇/온라인전체대응은후속).
- Release SCRIPTED49/SDL50 CTests·누적게임/데모빌드·경고0.
  12전이표,동시명령/소유/무효시간/종료기준재시작/0틱보류/게이트/포커스/동결,
  50시드최대120프레임Game직접대조통과. 학습UBSan·3개Release변형거부.
  실제beginSingleRound본문추출대역의외부상태리셋·생성실패보존,실제클라이언트
  빌드·루트16CTest·기존legacy해시골든유지통과. 대역은실제장치검증이아니다.
  out/learning-checkpoints/55-screen-state-check/verification.log. 모든작업종료.
- 교재·269객관식·모든공개차시DOM·정적배포검사·git diff --check통과.
  완성파일경로규약오류를교정해재생성했다. GUI조작/Win32/macOS실행은생략.
- 현재챕터마지막55검수후승인된Pages공개를완료했다. 소스브랜치강제변경없이
  정적배포전용gh-pages를새로푸시했고자동Pages활성화됨(추가POST는409,
  이후GET에서이미올바른gh-pages /설정·HTTPS·built상태확인).
  배포run35855628510success,HTTPS15파일+루트200동일검증. 다음은로컬56차시.

### 54차시 완료

- 054.json:14절·14스니펫·4접힌완성파일·5문제. 총1~54차시·264문제.
  합성·명령/조회·값 뷰·보고 소비·Rule of Zero·참조 별칭·소유 수명을 설명했다.
- DeepSeek에선택명세를전달해GameView/make_view초안을받아검토·통합했다.
  out/learning-jobs/054-adapter-{material.txt,events.jsonl,draft.json}. 외부작업종료.
- 54-game-adapter는53의CMake/README/src/main.cpp외기존파일을유지한다.
  CPU Game이FrameRunner와AccentNoise를값으로소유하고advance당한번위임,
  잠금보고마다표현난수를한번소비한다. view는표시값복사본이며조회로상태를바꾸지않는다.
  학습main의규칙진행·GPU준비·배경색과실제로연결했다. GPU자원은렌더스코프소유.
- 실제Game의암시적복사를장치대역으로재현했다:복사본score변경이원본sim을변경,
  두소멸자로4개핸들중복해제와초기화참조-1. 복사/이동을명시적으로삭제하고
  unique_ptr전달로객체주소를유지한다. 값멤버만가진학습Game은복사를허용한다.
- 실제main의정상종료에서session.Close와네Game소유자reset을백엔드종료전에배치했다.
  모든예외/조기종료경로를검증한것은아니다. audio.h와game.h의XAudio2전용주석을
  SDL공통계약으로갱신하고,실패한audio_init에도shutdown짝이필요함을명시했다.
- tests/game_wrapper_test.cpp는실제Game.cpp/SimGame을장치대역에링크한다.
  별칭·안정주소·공유음악·사건소비·실패초기화·대체재생·1회해제를검사한다.
  실제음향/픽셀검사가아니다. 루트CMake에game_wrapper CTest를추가했다.
- Release SCRIPTED48/SDL49 CTests와누적게임·데모빌드,경고0. 100시드×150프레임
  직접FrameRunner대조·규칙bytes·시간위상·보고·뷰·표현상태를검사했다.
  과거뷰·독립복사·100회조회·무효시간·깨진뷰·배치두잠금·종료후보존도검사했다.
  학습/실제UBSan,복사/이동컴파일거부,이중진행/보고누락/조회소비3변형거부.
  실제클라이언트빌드·루트16CTest·기존sim_hash_dump 전체골든유지.
  out/learning-checkpoints/54-game-adapter-check/verification.log에기록했다.
- Part4 Game계약/별칭/종료순서,Part5 audio.h,Part13 CMake현재발췌갱신.
  변경된Part4두절은54부분대응으로대조했으며전체절완료로올리지않았다.
- 교재검사·264객관식·모든공개차시DOM계약·정적배포재현성검사통과.
  배포ID28c91985b88093d1,정적15파일,28문서/167소스. GUI조작검사는생략했다.
- 다음55차시가module-04의끝이다. Part4§8의“대입하는것뿐/항상”과§9의
  “잔재없는리셋”은실제main의외부누적시간/입력과대조해교정할것.
- 2026-09-23GitHub조회:origin Rein-ArXiv/Tetris-Multiplayer-RL,main,public,
  viewerPermission ADMIN. Pages조회404(미설정). 55차시검수후명시승인된첫공개진행.
  56차시이후는로컬계속집필. 공개는수동workflow_dispatch로구분한다.

### 53차시 완료

- 053.json:15절·13스니펫·4접힌완성파일·5문제. 핵심은회귀/오라클의근거·조건/기록
  스키마·입력시간의미·관측해상도·파서완전성·공통접두길이·프로세스출력·후보채택이다.
- DeepSeek에선택명세만보내순수compare_traces초안을받았다.
  out/learning-jobs/053-golden-{material.txt,events.jsonl,draft.json}.
  검증된키의직접조회,metadata진단접두,해시비교주석을교정해통합했다. 외부작업종료.
- 53-golden-regression은52의CMake/README외기존파일을바꾸지않는다. 실제Round를쓰는
  src/golden_trace.cpp, tools/check_golden.py, tests/golden_compare_contract.py와두골든추가.
  매틱mask디코드·가비지대기후tick결과/정규bytes/hash기록. 종료후호출은stopped관측.
  main/FrameRunner게임코드는그대로이며같은Round를관측도구가직접사용한다.
- mixed는DOWN1/5번째하강(5/9),고정11/14/17/48,대기가비지12→14주입,
  고정뒤30번째중력47을포함한다. overflow는20줄주입game_over와stopped×2.
  초기및tick1/2는52의독립바이트기준벡터와대조해후보를채택했다. 전체규칙분기커버아님.
- 파서는중복JSON키·필드·태그·시드·tick연속성·mask/가비지범위·초기행·bytes/hash
  내부일관성을확인한다. 도달가능한전체게임상태를검증하는역직렬화기는아니다.
  check종료0일치/1유효한차이/2비교불가. capture는xb로기존후보·골든모두덮어쓰기거절.
  프로세스가실패하면stdout부분을정상후보로저장하지않는다. 최소Python3.10을CMake명시.
- 실제구버전Python비교를빈파일로실행해무검사성공을재현했고,끝step만빼면같은접두
  길이차에서StopIteration이발생함을재현했다. 엄격한legacy텍스트파서와길이진단으로수정.
  기준파일누락은skip조건에서제거하고네이티브없어도별도구조검사실행. 새parser회귀17항목.
- tests/sim_hash_dump.cpp의groundtruth/모든분기/모든피어DESYNC단정을교정했다.
  기존strtoull의부호/후행/오버플로침묵을from_chars+엄격한base0표기로보강.
  decimal/octal/0x표기는유지한다. 모든인자를검사한후출력하며마지막fflush/ferror실패는1.
  기존골든 _sim_hash_dump.txt는바꾸지않았고전체stdout동일을확인했다.
- Part1§11.5를실제동작기준으로재구성하고첫관측차이/원인시점·골든/오라클·기준파싱/
  반복재현/바인딩/타OS증거·후보채택을구별했다. 출처없는Windows골든주장은삭제.
  §11.4의즉시분기단정도조건있는설명으로교정했다. 원문현재발췌검사통과.
- Release SCRIPTED47/SDL48CTest 및누적tetris/golden_trace빌드·경고0.
  4시드×2시나리오반복·후보독점생성·기준보존·깨진기록·프로세스실패·/dev/full·CLI경계,
  중력초기값/첫입력/바이트버전3변형의각진단, UBSan기록기검사통과.
- .venv/bin/python(3.12)의pytest/pybind11재사용. 현재root소스로tetris_py+sim_hash_dump를
  새로빌드했다. PYTHONPATH로out빌드의tetris_py를먼저import하고__file__을검증한뒤
  parser/회귀21pytest통과. 저장소python/sim의기존.so는덮어쓰지않았다.
  루트게임전체를이번턴다시빌드한것은아니며게임코드/규칙변경은없다.
- 검증기 scripts/check_learning_golden.py; out/learning-checkpoints/53-golden-regression-check/
  verification.log·native-reference-tests.log·root-build-final.log·변형별result.log.
  Python최소버전추가뒤configure+golden3CTest재통과. GUI/타OS네이티브검사는생략했다.
- 라이브러리28문서/166소스. 원문344절:partial58/covered3/unassigned283/needs-review0.
  259문제·전체차시DOM·Part·git diff --check통과. 마지막원고변경뒤묶음이낡은것을
  정적검사가거절해재생성했으며최종재현ZIP/상대경로/허용목록/해시검사통과.
  릴리즈20d8fe5ae308e74d(15파일). site-verification.log·quiz-navigation.log기록.
  실행중인외부작업/도구세션없음. 공개배포·커밋·푸시없음. 다음54차시로계속진행한다.

### 52차시 완료

- 052.json:15절·14스니펫·4접힌완성파일·5문제. 동등성범위·시간위상·정규화·정수폭/
  엔디언·패딩·부호변환·실패버퍼·모드/길이·가방·충돌·비교시점·형식호환을 재구성했다.
- DeepSeek 선택명세로 Bytes초안을받았다. 052-hash-{material.txt,events.jsonl,draft.json}.
  용량덧셈을남은공간비교로보강하고noexcept를추가했다. 적용중정규식의제어문noexcept
  오삽입은즉시컴파일에서잡아고쳤다. 외부작업종료, 남아있는실행세션없음.
- 52-state-hash는51의CMake/README/main/두공급기관찰함수외기존파일을유지한다.
  core/canonical_bytes.h·simulation/state_hash.h·hash_demo·hash_contract추가.
  Bytes실패는고정되어digest가nullopt를반환한다. u32/u64는전체폭검사후기록한다.
  Source태그·활성패턴·남은가방순서/엔진·홀·카운터·누적/전투·종료·활동셀을직렬화한다.
  논리queue만기록하고미사용꼬리/물리head는제외한다. 스키마최대346바이트/용량512.
- 시드1 LRND/1 tick0/1/2는342바이트·행0·중력0/1/2,
  c799419b70bbe2de / c3409b53b6d47de1 / 32a79d4c3c185c58.
  8비트로의도적축약실험에서u32값143과256은4a로충돌한다. 원본64충돌주장이아니다.
- root core/hash.h는고정32/64정수만허용하며명시적LE. SimGame은32bit int요구를명시.
  StateHash/Breakdown의그리드원시읽기를필드순회로바꾸고공통ComputeStateHash추출.
  기존legacy hash숫자유지,별도DiagnosticStateHashV2추가. tests/sim_hash_test.cpp 및
  CMake CTest추가. 실제SimGame1000시드×70틱복사와표현플래그제외검사통과.
- 실제함수추출 HashProbe로남은가방순서/개수변조를재현했다. legacy누락확인, V2검출,
  카운터/대기/200개셀변조검사통과. 루트정수래퍼bool/pointer/struct 컴파일거부도확인.
- Release SCRIPTED44/SDL45CTest 및학습tetris,경고0. 루트tetris및15CTest통과.
  독립Python struct기반342바이트재구성·FNV값대조, 학습1000시드×60틱·숨은필드/
  부작용없는조회·링표현·패턴꼬리·실패/stopped·용량경계검사통과.
  학습/실제함수UBSan, 중력누락/가방순서누락/실패버퍼성공처리3Release변형거부.
  root sim_hash_dump전체stdout은보관된python골든과동일. 타OS/bigendian네이티브실행
  또는GUI검증을주장하지않으며사용자요청으로화면검사는생략했다.
- Part1§9/11.4/13/부록헤더를수정했다. 즉시DESYNC·가방누락·충돌확률실험의혼동·
  원시메모리정수설명을교정하고legacy/V2호환정책을기록했다. Part11 mutable/표현알림
  분류설명과현재발췌, Part13 CMake발췌도갱신했다. README의오래된44/45차시표시도수정.
- 검증기 scripts/check_learning_hash.py. 로그 out/learning-checkpoints/52-state-hash-check/
  verification.log·ctest-*·root-build/ctest·production_hash.inc·mutation/result.log.
  라이브러리28문서/165소스. 원문344절:partial58/covered3/unassigned283/needs-review0.
- 최종254문제/전체차시DOM/Part발췌/정적재현ZIP/git diff --check통과.
  릴리즈46fe09c2ac017676 (15파일),site-verification.log및quiz-navigation.log기록.
  공개배포·커밋·푸시없음. 다음53차시로전체목표를계속진행한다.

### 51차시 완료

- 051.json:14절·13스니펫·4접힌완성파일·5문제. 스트림/상태소유·호출결합·규칙/표현
  의존방향·XOR유도와0경계·가비지묶음·실패/종료소비·개입실험을 재구성했다.
- DeepSeek에core/rng.h와선택계약을보내HoleSource/AccentNoise초안을받았다.
  out/learning-jobs/051-streams-{material.txt,events.jsonl,draft.json}.
  상태를바꾸는sample을pure draw라고부른주석과중복설명을고쳤다. 외부작업은종료했다.
- 51-rng-streams는50의기존코드를Round/SeededBagSource/main/board_scene/CMake/README
  외에는 유지한다. session_seed.h로0정규화와garbage_tag를모았다. XOR결과0은엔진의
  88172645463393265기본값으로처리한다. 게임기본값을XOR뒤에적용하지않는다.
- HoleSource는optional엔진 또는고정{4,8,1}커서를소유한다. Round가양수실제주입
  한묶음마다한번호출한다. create_seeded는조각/가비지공급기를같은요청시드에서별도생성.
  기존create및상황실습은고정홀계약을유지한다. cursor는seeded모드에서0인고정패턴조회다.
- AccentNoise는규칙헤더/참조없이자신의엔진만소유한다. main의잠금보고분기가0~4를
  샘플링해배경blue0.09375~0.10175로변환하고renderer배경인자로전달한다. 잠금사이는
  색을유지한다. 보고하나당한번샘플링하며한프레임여러잠금에서는마지막색이표시된다.
- 실제시드CLI에선택garbage인자를추가해3줄을대기시켰다. 잘못된추가단어/부호/여분인자는
  거절한다. 새게임코드실행기/FrameRunner와동일경로를사용한다.
- Part1§10.5/11.7/공유스트림함정의시드0순서·표현소유권·플레이어vs복제본·즉시해시
  탐지단정을교정했다. root게임실행로직은이번턴에변경하지않았다.
- Release SCRIPTED43/SDL44 CTests 및tetris빌드,두구성경고0.
  100000교차소비·100시드Round×3표현비율·병합/0/무효/20줄overflow/stopped·복사,
  XOR0와고정패턴호환을검사했다. GL호출대역은5색전달/invalid색의clear전거절을확인했다.
- 실제SimGame1000시드에서첫잠금의가비지보드와조각RNG를대조했다. 실제InsertGarbage
  추출36경계(0/음수/상한/초과·XOR0 포함)를대조했다. 단일잠금/추출범위를전체게임의
  모든전이동등성으로확대하지않는다. 학습/실제fixture UBSan통과.
- XOR뒤게임0정규화·행별추가소비·0pending소비 세Release변형을거부했다.
  CPU출력/독립링크·실제SDLdummy의CLIsetup뒤GL창실패경계를검사했다.
  실제화면/픽셀또는Win32/macOS실행검증으로표현하지않는다. 화면검사는요청대로생략했다.
- 검증기 scripts/check_learning_streams.py, 로그
  out/learning-checkpoints/51-rng-streams-check/verification.log 및하위빌드/fixture.
  라이브러리28문서/164소스,원문344절:partial56/covered3/unassigned285/needs-review0.
  최종249문제/탐색/정적사이트검사와릴리즈식별값은아래에기록한다.

- 51차시 최종 정적 릴리즈 `cf9e614ac7f2e331`:15파일·상대경로·허용목록·해시·재현 ZIP 통과.
  전체249문제와전차시jsdom탐색/한차시마운트/문제즉시해설/복원/코드복사검사를통과했다.
  Part현재발췌·원고형식·git diff --check도통과했다. 실행중인외부작업없음.
  다음 시작점은52차시상태해시이며전체목표는계속진행한다.

### 50차시 완료

- 050.json:15절·스니펫16개·접힌완성파일5개·객관식5문제. 시드/상태/출력,
  순차XOR시프트·unsigned64순환·게임/엔진의서로다른0정규화·호출횟수·복사소유권·
  variant와태그있는합집합·Round후보게시·십진수입력경계를 단계적으로 재구성했다.
- DeepSeek의 선택자료는 core/rng.h, SevenBag/ScriptedSource와 두공급기의 구체적명세다.
  050-rng-{material.txt,events.jsonl,draft.json}에 보관했다. 미정의SevenBag수식형,
  variant를타입소거로부른주석,엔진상태만을스냅샷으로읽을수있는주석을 교정했다.
  외부작업은종료했다. 통합·검사·CS집필은주작업자가담당했다.
- 50-seeded-rng는49의기존파일을Round/main/CMake/README 외에는 유지한다.
  core/rng.h는실제루트엔진과바이트동일한기준복사본. 루트게임실행코드는이번에수정하지않았다.
  SeededBagSource는엔진과가방을소유하고,매추출마다상한1에서도한번소비한다.
  PieceSource는예외없는복사·이동을static_assert로제한한variant를값으로소유한다.
- Round의매개변수/멤버를PieceSource로바꾸고읽기전용source관찰을추가했다.
  tetris --seed 42는실제기존게임루프에연결된다. 기존종류/상황CLI는패턴공급을유지한다.
  seed_option은0~UINT64_MAX 십진수·앞0을허용하고부호/공백/부분읽기/초과를거부한다.
- Part1§8은불필요한통념반박을걷고엔진/범위축소의역할을설명했다. 엔진과게임의0기본값,
  state/반환출력구별,8바이트RNG와전체게임스냅샷의차이,시드1전이벡터를 추가했다.
  원문대응은56partial/3covered/285unassigned(344절)이며§8은부분대응을유지한다.
- Release SCRIPTED 전체42CTest·SDL전체43CTest와tetris 빌드,두구성경고0.
  Round저장타입이바뀌어모든누적규칙소비자를검사했다. 마지막복사/이동static_assert
  보강뒤에도verification-final.log의최종빌드와전검사를통과했다.
- 65,536개엔진전이를64개bool비트이동/반복덧셈 오라클과대조했다. 시드1고정벡터,
  상위비트/UINT64_MAX/0/1범위축소,100,000가방추출/복사·100시드Round생성/잠금/종료/
  무효입력·초기막힘·프레임0틱/무효시간/다음틱소비를 확인했다.1/60double의나노초절삭을
  고려해한틱실험은20ms로고정했다. 수열의전체주기나통계품질증명으로표현하지않는다.
- 실제SimGame1000생성시드와실제GetRandomBlock/GetAllBlocks함수추출100000회대조.
  새SeededBagSource의종류/남은순서/RNG상태가일치했다. root/학습코드UBSan통과.
  출력→state저장/상한1소비생략/잘못된0기본값 세Release결함변형을거부했다.
- CPU데모·독립링크와실제SDLtetris의시드CLI를dummy에서확인했다. GL창생성불가로
  setup출력뒤실패하는경계검사이며GUI실행증거가아니다. Win32/macOS네이티브실행미수행.
  사용자요청대로화면조작/스크린샷은생략했다. 공개배포/커밋/푸시없음.
- 검증기 scripts/check_learning_rng.py. 로그는
  out/learning-checkpoints/50-seeded-rng-check/{verification.log,verification-final.log}.
  최종사이트검사와릴리즈식별값은아래에기록한다.

- 50차시 최종 릴리즈 `899d20f5f0e60cc8`:15파일·상대경로·허용목록·해시·재현 ZIP 통과.
  Part 발췌·원고 형식·244문제·공개 전 차시의 jsdom 탐색/한차시마운트/저장/복원/코드복사
  검사를 통과했다. 최종 원고의 중복 부정문을 정리한 뒤 정적사이트 검사를 다시 실행했다.
  모든 외부/검증 작업 종료. 다음 시작점은51차시다.

### 49차시 완료

- 049.json:15절·핵심/실행 스니펫11개·접힌 완성 파일2개·객관식5문제.
  복원/비복원 추출, 활성 접두 구간, 불변 조건과 귀납,ID/위치,순서 보존 삭제,
  실패 보존,순열7!,조건부 균등성,modulo 편향,복잡도,경계 공백/14개 창을 재구성했다.
- DeepSeek는 선택 카탈로그와 구체적인 계약으로 SevenBag와 bag_demo 초안을 작성했다.
  자료/응답/초안은 out/learning-jobs/049-bag-{material.txt,events.jsonl,draft.json}.
  include 경로·중첩 타입 using·개행·빈 가방 예제를 교정했다. 원문/정확성/강의 집필과
  완전 열거·게임/실제 함수 대조 검사는 주 작업자가 담당했다. 외부 작업은 종료했다.
- 49-seven-bag는48의 기존 파일을 CMake/README 외에는 바꾸지 않는다. SevenBag의
  생성/remaining/next_bound/at/take와 private refill은 고정 배열을 소유한다.
  범위 밖 인덱스를 refill보다 먼저 거절해 빈 상태를 보존한다. 삭제는stable erase다.
  Release의 GCC 배열 상한 경고는capacity와활성count 경계를 명시하여 해소했다.
- bag_demo는 의도적으로 고른 O/I/Z/L/T/J/S 순열을 ScriptedSource와Round에 넣는다.
  현재O/미리보기IZL/cursor4→세 번 잠금→현재L/미리보기TJS/cursor0을 확인했다.
  이 source는 한 패턴을 반복한다. tetris 공급기를 자동 난수 가방으로 바꾸었다고 말하지 않는다.
- Part1의 동일 분포/같은 시드 결과 혼동, 조각RNG 유일 호출 지점 단정, 입력과무관한
  RNG 호출 횟수, RngState의const 반환 설명을 교정했다. rng/garbageRng 소비를 구분했다.
  §8.3은2^64균등 가상입력의 계산과 실제엔진의 비영상태·순차호출을 분리했다.
  근거 없는 “게임에서 관측하기 어렵다/충분하다” 단정과 MT 객체크기 단정도 정리했다.
  root core/rng.h·GetRandomBlock은 주석만 수정했다. 런타임 알고리즘·시드 순서는 유지한다.
- Release SCRIPTED 전체41 CTests·SDL bag/queue/frame3 CTests와tetris 빌드 통과.
  두 구성 경고0, DISPLAY 없는 CPU데모/독립링크, 정확한 출력과UBSan 확인.
- 5,040선택경로/순열과 모든접두상태/실패/독립복사/재충전, 각순열의Round 생성과3승계,
  특정종류의49경계쌍·2,401개14구간·최대연속2,modulo3/3/2를 검사했다.
  swap-tail·refill선행·erase누락 세 결함 변형을 Release에서 거부했다.
- 실제 SimGame 생성1,000시드의4추출·시드0 처리·미리보기조회 RNG보존을 검증했다.
  실제 두 공급함수를 추출한 fixture로100,000추출의종류/남은순서/RNG상태를 대조했다.
  추출함수검사를 전체게임장기실행으로 표현하지 않는다. root runtime 변경이 없어
  전체14CTest를 반복하지 않았으며49의실제함수fixture를UBSan으로 실행했다.
- 라이브러리28문서/164소스, 원문344절 중partial56/covered3/unassigned285/needs-review0.
  Part1§7·§8·오류와함정은49와관련한부분만partial연결했다. 전체 원문 대응 완료는 아니다.
- Win32/macOS 네이티브 실행 미수행. 사용자 요청대로 브라우저 화면 조작과 스크린샷은 생략했다.
  검증기는 scripts/check_learning_bag.py이며 상세 로그는
  out/learning-checkpoints/49-seven-bag-check/verification.log와하위fixture에 보관한다.
  최종 사이트 검사 결과와 릴리즈 식별값은 아래에 기록한다.


- 49차시 최종 정적 릴리즈 `d00acd3058c670a9`:15파일·상대경로·허용목록·해시·재현 ZIP 통과.
  전체239문제의즉시해설/재선택/복원/저장실패와공개전차시의jsdom단일마운트/탐색/코드복사
  계약을검사했다. Part현재발췌·원고형식검사·git diff --check통과.
  최종코드스니펫들여쓰기를정리한뒤정적사이트검사를다시실행했다.
  실행중인외부작업없음. 다음은50차시시드RNG다.

### 48차시 완료

- 048.json: 15절·핵심 스니펫10개·접힌 완성 파일2개·객관식5문제. 비트 자리값,
  OR 멱등성/AND any·all/NOT·정수 승격, 축소 전 검증, encode/decode의 서로 다른
  왕복 성질, raw32가지→의미24가지 정규화, 입력 수명·표현/조작 정책을 재구성했다.
- OpenCode DeepSeek에 승인된 선택 명세와 PendingControls/PendingHorizontal/core/input.h만
  전달했다. 자료·응답·초안은 out/learning-jobs/048-mask-{material.txt,events.jsonl,draft.json}.
  프레임당 여러 capture라는 초안 주석과 학습 raw 마스크를 현재 wire 정책과 동일시하는
  표현을 교정했다. 현재 정책과의 차이는 본문과 레퍼런스에 명시했다.
- 48-input-mask는 47의 플랫폼·FrameRunner·Round 및 기존 검사를 그대로 유지했다.
  Intent는 simulation/intent.h로 분리, input_mask.h에 Mask/비트/조회/valid/encode/decode.
  PendingControls만 pending 바이트+held bool로 바꾸고 consume_mask/consume/clear 제공.
  실제 FrameRunner가 consume을 계속 사용하므로 누적 게임 입력 경로에 연결되어 있다.
- Root core/input.h에 INPUT_KNOWN_MASK와 uint64_t isValidInputMask를 추가했다.
  RankedGame과 bot reward_replay의 하드코딩31 검사를 공통 함수로 바꿨다.
  유효 비트·좌우 순차 시도·기존 틱 값은 바꾸지 않았다. 서버의 허용 비트 검사를
  경기 전체의 안전성·권위 검증으로 표현하지 않는다.
- 실제 core/replay.cpp 수정 전256을0으로 잘라 수락함을 재현했다.
  out/learning-checkpoints/48-input-mask-check/before.log: accepted=1, decoded_first=0.
  Load는 uint64_t 십진수→허용 비트→uint8_t 순서로 변환하고 연속 인덱스·개수·끝을
  검사한다. candidate를 완성해야 out에 대입하며, 잘린/중복/범위 밖 파일은 거부한다.
  로컬 제한은1,000,000틱/64MiB/숫자토큰20자리다. 토큰읽기폭21로 거대한 숫자토큰의
  문자열 할당을 제한하고 reserve 전 틱 상한을 검사한다. 파일 크기는 파싱 전에 확인한다.
- Save는 잘못된 입력을 파일 열기 전에 거부하고 flush 결과를 반환한다. 저장 중 실패의
  원자적 교체는 제공하지 않는다. tests/replay_io_test.cpp와 CMake replay_io를 추가했다.
  정상 형식은 그대로다. 예전 로더가 받아들인 부분 파일·순서가 섞인 인덱스·불법 입력은
  의도적으로 더 이상 받아들이지 않는다. F5/F6 기록기 자체를 전체 경기 기록기로 바꾸지 않았다.
- Part1/4의 입력 헤더·1바이트/직렬화/파일 크기/처리 순서·파서 계약, Part6의 잘못된
  헤더1줄 설명과 F5/F6의 온라인/봇 완전 재현 주장, Part13의 CMake 발췌, Part18의
  검증기 발췌를 수정했다. Game::SubmitInput 설명의 사건 카운터도 실제 bool 플래그로 교정.
- 실제 F5/F6는 로컬 inputMask와 p2=0을 기록하며 중간 상태·양쪽 적용 입력을 완비하지
  않는다. 후속 리플레이 강의에서는 이 수집 경계와 저장 실패의 단축키 피드백·자원 한도를
  다룰 것. 현재 API 실패 검증을 단축키 UI 검증으로 확대하지 않는다.
- SCRIPTED 전체40 CTests/SDL4 CTests 및 SDL tetris 빌드, 두 구성 경고0.
  65,536 raw 값·65,536 any/all 조회·24 Intent·32,768 프레임 이력으로 기존 의미를 대조.
  mask_demo 출력과 CPU 독립 링크 확인, 마스크/실제 리플레이 UBSan 통과.
  축소 선행/XOR 누적/held OR 보관의 세 Release 결함 변형을 거부했다.
- 루트 tetris 빌드와14 CTests 통과. Replay IO는 전체 바이트 범위, UINT64_MAX 등
  넓은 값, 숫자 문법·연속 인덱스·누락·여분 데이터·CRLF·실패 시 출력 보존·최대틱
  round-trip·초과파일·invalid Save 시 기존 파일 보존·Linux /dev/full flush 실패를 검사.
  RankedGame·봇 hex decoder도256개 바이트의 수락/거부를 전수 확인했다.
- 234문제·전체 공개 차시의 jsdom 탐색/저장/복원·Part 발췌·원고 형식·정적사이트 검증 통과.
  Win32/macOS 네이티브 실행은 미수행, 사용자 요청에 따라 브라우저 화면 조작은 생략했다.
  기존1~47 체크포인트/문제 ID는 보존했다. git commit/push/공개 배포는 수행하지 않았다.
- 라이브러리28문서/164소스. 원문344절은 partial53/covered3/unassigned288/needs-review0.
  새 Part6 부록A·Part18 검증기 대응은 각각 기록 범위와 지원 비트 설명에 한정한 partial이다.
- 정적 릴리즈9138f1e2769c2023,15파일·상대경로·허용목록·해시·재현 ZIP 검사 통과.
  상세근거: out/learning-checkpoints/48-input-mask-check/{verification.log,root-build.log,
  root-ctest.log,site-verification.log}. 외부 모델 작업은 종료했다.

### 47차시 완료

- 047.json: 14절·객관식5문제, 상태/사건과 정보 손실, 사건→프레임/프레임→틱의
  두 보관 경계, bool 병합, OS 반복, 포커스 취소, 실패 시 보존을 단계별로 재구성했다.
- OpenCode DeepSeek의 승인된 선택 명세로 core/key_edges.h와 순수 검사 초안을 받았다.
  047-key-material.txt / 047-key-events.jsonl / 047-key-draft.json에 자료와 응답 보관.
  초안의 동일 대입 기반 참조 모델을 사건 이력 역산 오라클로 교체했다. abort는
  Release 변형 검사의 종료 코드를 안정화하기 위해 exit(1)로 바꾸었다.
- 실제 SDL의 같은 펌프 down→up 손실을 수정 전 fixture에서 재현했다.
  out/learning-checkpoints/47-input-edges-check/before.log에 실패 증거가 있다.
- core/key_edges.h는 실제 루트와 47 체크포인트의 동일 구현이다. 고정 크기 held/
  pressed/released 배열, begin_frame과 reset의 수명 구분, repeat down 거부,
  cancel 시 앞선 press 제거/held 해제/취소 플래그, 취소 이후 새 press를 제공한다.
- 실제 SDL/Win32와 체크포인트 SDL/Win32/SCRIPTED가 공통 래치를 사용한다.
  WM_KEYDOWN·WM_SYSKEYDOWN은 lParam 비트30, SDL은 repeat 필드를 전달한다.
  Win32는 Microsoft 공식 계약과 소스를 검토했으며 네이티브 컴파일·실행은 미수행이다.
  루트 키 상태 reset은 생성 시작과 창 종료 사건 처리가 끝난 뒤에 수행한다.
- platform_input_cancelled를 루트 AccumulateInput과 체크포인트 FrameInput에 연결했다.
  오래된 pending/DAS를 먼저 지우고 취소 뒤 새 입력을 수집한다. 0틱 프레임에서도
  취소하며, FrameRunner의 무효 시간 호출은 취소를 포함한 전체 호출을 거부한다.
- 47은 46의 누적 프로젝트를 복사해 입력 경계만 발전시켰다. 1~46 체크포인트 파일은
  수정하지 않았다. 7차시 기준 스니펫도 유지하고 현재 소스 연결 심볼·차이 설명만 갱신했다.
- Part2의 끝점 비교 설명·공개 API·생성/종료·이벤트 펌프·포커스/반복 정책과 발췌,
  Part4의 누적 입력 및 VSync=1틱 단정, Part5의 플랫폼 종료 발췌를 동기화했다.
  원문 대응은344절: partial51/covered3/unassigned290/needs-review0. 전체 대응 완료는 아니다.
- Release SCRIPTED39·SDL40 CTests, 두 구성 경고0 빌드. SDL dummy에서 실제 큐 주입→
  플랫폼→FrameRunner 경계 검사; 루트 SDL fixture와 실제 main 입력 함수 추출 검사 통과.
  97,656 사건열의 접두 상태/키 독립성/잘못된 번호/반복/취소/초기화를 확인했다.
  취소의 invalid-time rollback·0틱 취소·취소 뒤 새 입력을 Round 전체 비교로 확인했다.
  UBSan 통과, 탭 손실/반복 복원/취소 시 press 누락 세 변형을 Release에서 거부했다.
- 실제 루트 tetris SDL 빌드·13 CTests 통과. 최종 키 reset 위치 변경 후 tetris와
  플랫폼 fixture를 다시 빌드·확인했다. 루트 규칙의13 CTests는 키 reset과 무관하다.
- 229문제·전체 공개 차시 DOM 탐색/저장/복원·Part 현재 발췌·원고 형식 검사를 통과했다.
  DOM 검사는 jsdom이며 실제 화면/키보드 조작 증거로 표현하지 않는다. 사용자 요청으로
  화면 조작과 스크린샷은 생략했다. 공개 배포·git commit/push는 하지 않았다.
- 상세 로그: out/learning-checkpoints/47-input-edges-check/verification.log,
  root-build.log, root-final-build.log, site-verification.log.


- 47차시 최종 정적 릴리즈: `192220d6870442db` (15파일). 상대경로·해시·허용 목록·재현 ZIP 검증 통과. 실행 중인 외부 작업 없음. 다음 시작점은48차시 비트마스크다.

### 46차시 완료

- 046.json: 14절·스니펫 6개·접힌 완성 파일 3개·객관식 5문제.
- 46-catch-up은 45의 기존 구현 파일을 그대로 유지한다. CatchUpClock/budget_trace/
  budget_contract로 입력 100ms 제한, 6틱 처리 뒤 전체 틱 보관, 전체 틱 폐기를 비교한다.
  게임의 tetris 타깃은 계속 FixedClock/FrameRunner를 사용한다.
- Report의 ticks/pending_ticks/phase/clamped_ns/dropped_ticks 단위를 구분하고, 입력·배정·
  보관·폐기를 합친 보존식을 설명한다. 1초→0ns 사례, 100ms+1ns 경계, 작업 용량과 지속
  과부하, raw dt와 반환 dt, Net의 별도 safeTick 루프를 다룬다.
- DeepSeek 초안의 6초/6틱 혼동, clamp에서 잘린 1ns를 보관하는 기대값, phase를 다음 틱까지
  더 필요한 양으로 쓰던 주석을 교정했다. 시나리오 열로 새 시계의 경계를 분리했다.
  실패 보존을 비영 phase/backlog에서 검사하고, 곱셈은 맞지만 기존값과의 덧셈이 넘는 입력도 추가했다.
- 정책은 생성 때 고정하고, 지역 변수로 계산 후 마지막에 credits_를 게시한다. clamp 상한과
  틱 예산의 관계를 static_assert로 고정하여 수락한 전체 틱을 조용히 버리는 변경을 막는다.
- SCRIPTED 전체 38 CTests·SDL의 frame_contract/clock_accounting/budget_contract 3개 통과.
  경고 0·UBSan·CPU 도구/검사 독립 링크·DISPLAY 없는 실행. 세 정책 30,000개 접두 보존식,
  FixedClock 2,000표본 대조, 표출 CSV·경계·실패 보존을 검사했다. 보류분 손실·폐기량 누락·
  용량 검사 제거 세 오류 변형을 Release에서 거부했다. 기존 구현 바이트 동일 확인.
- Part4 §6의 “현재 반환 dt=2초”를 상한 전 측정값과 분리했다. 120틱이면 특정 개수의 피스가
  반드시 고정된다는 설명도 정책/보드 의존으로 교정했다. Net 내부 루프와 로컬 6틱 계산을
  구분하도록 §6·오류/함정·수동 테스트를 수정했다. 실제 게임 로직은 바꾸지 않았다.
- 재현: python3 scripts/check_learning_catch_up.py. 로그는
  out/learning-checkpoints/46-catch-up-check/ 및 out/learning-jobs/046-check.log.
  046-budget-material.txt / 046-budget-events.jsonl / 046-budget-draft.json 보관.
  DeepSeek 종료 확인. 실행 중 외부 작업 없음. 화면 조작 검수는 사용자 요청으로 생략했다.
- 문서 발췌/224문제/원고 형식/원문 대응/정적 배포물 검사 통과.
  원문 344절 중 51부분연결/3전체대응/needs-review 0, 28문서/162소스.
  정적 릴리즈 `81160a2113bd0f81`, 15파일·상대경로·해시·재현 ZIP 통과.
  공개 배포·커밋·푸시 없음. 전체 목표는 완료 처리하지 않는다.

### 이번 사용자 요청과 편집 범위

- 디자인·차시 순서·실제 게임 기반 구성은 유지. 방어적 부정문·중복 정리와 차시별 렌더링을 적용.
- 기존 29개 차시에서 문맥별 59개 교정/메모 추가. 무관한 반박은 삭제하고 유용한 오해 설명은
  notes로 이동했다. 코드 전제·실패 계약은 유지한다. 3-2의 20ms 반복과 6차시 RAII 중복 정리.
- 화면 조작 검수는 사용자 요청으로 생략한다. 이후 정확성·자동 검사를 필수로 삼는다.
  기존 29~35/36의 화면 검수 대기 표시는 더 이상 집필 진행의 차단 조건이 아니다.
- 사용자가 JS 파일과, 후속 집필용으로 주 작업자가 선별한 C++ 코드·문서 발췌의 DeepSeek
  전송을 명시적으로 승인했다. 키·계정정보·설정·저장소 전체는 제외한다. 같은 범위에 재확인하지 않는다.

### 차시별 렌더링

- course.js가 현재 차시만 DOM에 생성하고 이전 차시를 제거한다. 같은 차시의 절 이동에서는
  재생성하지 않는다. 전체 문제 인덱스와 제목·앵커 메타데이터는 DOM과 분리한다.
- app.js는 이벤트 위임과 새 본문의 답안/확인/완료 복원을 사용한다. 검색·재개·편성안은
  DOM에 없는 차시도 찾는다. 동적 본문의 코드복사·배포본 라벨·접근성 속성도 적용한다.
- DeepSeek JS 초안의 잘못된 앵커 유효성, 최초 #content/#course-sidebar 진입, 동적 소스 라벨을
  교정했다. 차시 교체로 사라진 소스 버튼 대신 본문으로 키보드 초점을 돌려준다.
- scripts/check_learning_navigation.cjs: jsdom 26.1.0 DOM 검사. 공개된 전 차시 단일 마운트,
  같은 차시 DOM 유지, 절 링크/접힘, 검색·재개, 답안·레거시·미래 기록 보존, 확인/재선택/해제,
  코드/답안 복사, 소스 참조, file URL·저장 실패, 중복 이벤트와 ID를 확인했다.
- 기존 tests/learning/quiz.cjs의 대역에 문서 이벤트 위임을 추가하여 219문제 검사 통과.
  임시 jsdom은 /tmp/learning-dom-check에만 설치했다. 배포 의존성에 추가하지 않았다.
- DeepSeek 작업 review-render-js-events.jsonl 종료. 최소 예제 작업은 사용하지 않았으며
  그 작업의 도구 호출은 문자열 반환용 execute였고 저장소 탐색/수정은 없었다.

### 44차시 완료 기록 보완

- 044.json: 13절·스니펫 10개·접힌 완성 파일 4개·GL 그림 2개·문제 5개.
- FrameRunner는 시간/입력/Round를 후보에서 진행하고 틱별 값 보고와 최종 상태를 분리한다.
  0틱 입력 보관, 최대 6틱 보고 배열, 중간 고정 스냅샷, 변경 플래그 OR, stopped 재보고 억제.
  초안의 네임스페이스 누락과 용량 초과 틱을 조용히 잘라내는 처리를 수정했다.
- 기존 검증: SCRIPTED 36/SDL 36 CTests, UBSan, 세 오류 변형 거부, 18 SDL offscreen GL
  픽셀·VBO 비교. 이전 턴에 수행한 화면 확인 결과는 메타데이터에 보존한다.
- Part4의 프레임/틱·제출/표시·사건 소비 설명, SimGame 음향 bool 주석과 Part1 발췌를 수정했다.
  이번에 Part1 충돌 절의 변경 주석과 소비 의미를 재대조하여 stale 대응 해시를 갱신했다.

### 45차시 완료

- 045.json: 12절·스니펫 4개·접힌 완성 파일 2개·문제 5개.
- 45-clock-accounting은 44의 기존 구현 파일을 그대로 보존하고 clock_trace와 clock_accounting을
  추가한다. 나노초×60 위상, 몫/나머지, 8/9/5/11/17ms의 0/1/0/0/2틱, 1ns 경계,
  수락 시간 보존식, 초→정수 나노초의 양자화와 분할 조건을 설명한다.
- DeepSeek 초안의 마지막 합계만 검사하는 문제를 매 프레임 독립 보존식 검사로 보완했다.
  양자화 실험의 시계를 분리하고 입력 거부를 0틱으로 덮던 처리를 제거했다.
- SCRIPTED 전체 37 CTests, SDL의 clock_accounting/frame_contract 2개 통과. 경고 0.
  새 CPU 도구·검사의 플랫폼 독립 링크와 DISPLAY 없는 실행, UBSan 통과.
  10,000개 입력의 모든 접두 합, 분할, 양자화, 실패 보존을 검사하고 나머지 삭제/틱 기준 반올림
  두 오류 변형을 Release에서 거부했다. 44의 기존 구현 파일과 바이트 동일도 확인했다.
- Part4의 잘못된 음수 나머지 계산을 정수 위상 표로 교정했다. 가변 스텝에는 순번이 없다는
  단정, 공통 상수만으로 봇 속도/동기화가 보장된다는 설명을 수정했다. 실제 게임 로직 변경 없음.
- 재현: python3 scripts/check_learning_clock_accounting.py. 로그는
  out/learning-checkpoints/45-clock-accounting-check/ 및 out/learning-jobs/045-check.log.
  DeepSeek 선택 자료/응답/초안은 045-clock-* 이름으로 보관. 실행 중 외부 작업 없음.

45차시 당시 확인: 원문 344절 중 50부분연결/3전체대응/needs-review 0. 28문서/162소스.
정적 릴리즈 `84652f092ac968ff`, 15파일·상대경로·해시·재현 ZIP 검사 통과.
공개 배포·커밋·푸시 없음. 전체 강의 목표를 완료 처리하지 않았다.

## 다른 기기에서의 학습 요구

사용자는 GitHub Pages를 선택했다. 저장소는 public/main이고 Pages는 조회 시 비활성이다.
learning-pages.yml은 push/PR에서 준비·검증하고 workflow_dispatch/main에서 배포한다.
현재 턴은 파일 준비만 하며 실제 공개는 추후 배포 시점에 수행한다.

정적 사이트에 강의·현재 배포 시점의 소스 레퍼런스를 포함하고, 개발용 작업 폴더 API는
공개 서버의 필수 기능으로 삼지 않는다. 하위 경로 배포와 모바일 가독성을 검증한다.
학습 기록은 브라우저별이다. 사용자 요청으로 기록 파일 이동 기능은 삭제했다.
자동 서버 동기화도 제공하지 않는다. 다른 기기에서는 사이트를 읽고 문제를 풀 수 있다.
업데이트에도 차시/문제 ID를 유지해 기존 답안과 완료 기록을 보존한다.

## 완료 조건

전체 교육 범위·원문 대응 검토·각 문제의 즉시 해설이 HTML에 연결되고 가능한 검증을 통과해야 한다.
또한 배포물 생성과 반응형 학습 화면을 검증하고 운영/갱신 절차를 기록한다.
기록 이동은 최신 사용자 요청으로 제외되었다.
작은 묶음 완료만으로 전체 목표를 complete로 바꾸지 않는다.

## 최신 편집 요청 — 검수 보고는 학습 본문에서 제외

- 사용자가10차시의 집필 환경·미수행 플랫폼 보고 삭제를 요청했다. 해당 문단과
  전체 차시 머리말의 validation 출력을 제거했다. 2·7~13차시의 유사 보고도 정리했다.
- 검증 결과는 내부 review/validation 메타데이터와 REVIEW_LOG에 보존한다.
  본문에는 실행 조건·실습 방법·예상 결과·코드 계약을 설명한다. AUTHORING에 반영했다.
- 10차시 실제 화면에서 대상 문단/머리말 제거 확인, 객관식59문제 검사 통과.
- 이 원칙을14차시부터 적용한다. 집필 결과/OS 미검증을 본문에 반복해서 넣지 않는다.

## 완료한 20차시 인수인계

- 020.json·20-raster·README·check_learning_raster.py와 통합 검사 분기 추가.
- OpenCode DeepSeek raster 작업 exit0. 승인 명세/응답/초안은
  out/learning-jobs/raster-material.txt, raster-events.jsonl, raster-draft.json.
  실행 중 외부 작업 없음. 불필요한 template 제거, equal-w=1 보간 조건 명확히 수정.
- renderer/raster.h는 C++17 표준 array/cmath/optional만 사용한다. Point·Weights·
  Region(outside,boundary,inside)·Sample, edge/sample_triangle/pixel_center/mix_rgb.
  유한값과 절댓값≤1e6 실습 정책. computed area0은nullopt, 양쪽 winding 허용,
  바깥 판정이 boundary보다 먼저다. epsilon/subpixel스냅/공유변 소유권은 구현하지 않는다.
- 색상은 Ared/Bgreen/Cblue. 단순 가중합이 smooth와 대응하는 equal clip w1만 실험한다.
  일반 perspective 보간 식은 곁들임 메모로 설명하며 깊이 등 모든 값으로 일반화하지 않는다.
- raster_sources.h에 smooth/flat/cutout 정점/조각 조합. gl_VertexID로 세 색을 선택하므로
  DrawArrays TRIANGLES first0/count3 전제. 다음차시 정점6개 확장 때 반드시 수정 필요.
- flat은 기본 LAST_VERTEX_CONVENTION의 마지막 정점 C색. cutout은 gl_FragCoord/8의
  floor 칸 번호 합이 짝수면discard. 새 컨텍스트·단일샘플·기본채움·블렌드/깊이 등비활성.
- main의 VBO/VAO/Program/draw/더블버퍼 정책 유지. 선택한 두 셰이더만 초기화시 교체.
  coordinates_demo와 기존 probe/테스트도 보존했다. GlApi37함수 그대로.
- raster_demo는16×16, 삼각형(4,4)/(12,4)/(8,12), 내부32개; (8,6)의 가중치 .375/.375/.25.
- raster_probe는SDL 단일버퍼/MSAA0 요청·실제속성조회. CPU로 예상한 창정점
  (32,32)/(96,32)/(64,96)에서 전체RGB 비교. 색반올림허용3단계.
  PPM은위행부터 저장. 이미지3개는 그대로PNG인코딩하여본문dataURI내장.
- out/learning-checkpoints/20-raster-check에 빌드/대조군/PPM/PNG가 있다.
  경계점 없는 선택도형에서 비교한 증거이며 GL전체래스터규칙 구현/물리GPU증거는 아니다.
- Part3 section04신규부분연결, section08추가연결·해시갱신.21partial/0covered 유지.
  section04의 물리하드웨어 전체 설명·SDF/텍스처/블렌드 전체는 아직 미완료다.
- Part발췌검사가Part4중복shader주석 불일치를 발견하여 동일교정 후 통과했다.
  브라우저에서 무게중심좌표의 Markdown굵게문법이문자로남는것도수정후확인.
- QA탭닫음·viewport복원·20-1선택해제. 사용자탭·기존답안보존. 다음턴21차시부터 진행.

## 완료한 19차시 인수인계

- 019.json·19-coordinates·README·check_learning_coordinates.py와 통합 검사 분기 추가.
- OpenCode DeepSeek coordinates 작업 exit0. 선택 자료/응답/초안은
  out/learning-jobs/coordinates-material.txt, coordinates-events.jsonl, coordinates-draft.json.
  실행 중 외부 작업 없음. w0에서 부등식이 무의미/GL이 폐기한다는 설명, underflow→inf,
  임의 NDC/window depth 범위 보장 주석을 수정했다. 문제는 배운 수치/단위 적용으로 재집필.
- 좌표 헤더는 C++17/표준 라이브러리만 사용. 별도 공간 타입과 optional로 실패 표현.
  inside_clip_volume은 w>0 정책의 점 술어일 뿐 primitive clipping이 아니다.
  perspective_divide는 음수 w 산술 허용, to_window는 밖의 NDC도 산술 매핑하며
  GL viewport 크기 제한/깊이 clamp를 재현하지 않는다. ui_to_clip은 divide-first 방식.
- 기존 GlApi37함수/VBO/VAO/Program/draw_triangle 유지. 정점 셰이더4변형을 실행 인자로
  선택한다. main의 더블 버퍼 정책 유지, probe는 single buffer/readback이다.
- CPU demo viewport(10,20,200,100): base (60,45)/(160,45)/(110,95),
  w2 (85,57.5)/(135,57.5)/(110,82.5), scaled는base와 같음,
  oversized(-40,-5)/(260,-5)/(110,145). 원래 정점은 모두밖이지만 도형 일부가 남는다.
- GL128×128: base/scaled2048, w2 512, oversized11776 색상 픽셀.
  CPU 예상 경계±1픽셀, 내부/배경 점, 양수 동차 배율 결과 비교를 함께 검사했다.
  고정 예제의 결과이며 임의 도형/셰이더의 비트동일·픽셀개수비율 보장으로 확대하지 않는다.
- 첫 대조군 실행은 원본/변형 헤더 중복으로 빌드 실패했다. 변형 projection_cases와
  shader_sources를 함께 복사해 경로를 통일한 뒤 재실행하여 의도한 나눗셈 실패를 확인했다.
- 픽셀 PPM/PNG·verification.log는 out/learning-checkpoints/19-coordinates-check에 보관.
  PNG는 실제 RGB 데이터를 인코딩해 HTML 본문에 data URI로 내장했다.
- Part3 section08/10 대응 해시 갱신·lesson19 부분 연결. 여전히20개 부분 연결/0개 전체완료.
- 저자 검수/환경 보고는 이 기록과 review/validation에만 둔다. 학습 본문에는 넣지 않는다.
- 검수 탭의19-1 테스트 선택 해제 후 탭 닫음·viewport 복원. 콘솔 오류 없음.
  사용자 탭과 기존 답안 보존.
- 공개 배포·커밋·푸시는 하지 않았다. 다음 턴은20차시부터 진행한다.

## 완료한 18차시 인수인계

- 018.json·18-triangle·README·check_learning_triangle.py와 통합 검사 분기를 추가했다.
- OpenCode DeepSeek triangle 작업 exit0. 선택 자료/응답/검토 전 초안은
  out/learning-jobs/triangle-material.txt, triangle-events.jsonl, triangle-draft.json.
  실행 중 외부 작업 없음. 모델의 차시 번호·clear 범위·CPU 원본 수명 설명을 수정했다.
- draw_triangle은 비소유 함수다. 양수 픽셀 크기/이름과 깨끗한 오류 상태를 요구하고
  UseProgram·VAO·viewport·색 지정·clear·draw(0,3) 후 선택을0으로 해제한다.
  실패에도 해제를 시도하며 이전 상태 복원이나 픽셀 롤백을 보장하지 않는다.
- 새로운 컨텍스트의 기본 프레임 버퍼/테스트/쓰기 상태가 전제다. 다른 렌더러와 섞을 때
  GL 상태를 관리하는 범용 구현은 아니다. VBO·VAO·Program 구현은17차시 그대로다.
- GlApi37함수: Viewport/ClearColor/Clear/DrawArrays/ReadPixels/ReadBuffer 추가.
  로더 누락 검사는 비트 마스크가 아닌 인덱스 방식을 유지한다.
- fragment 출력 location0 명시. vertex 입력 location0과 다른 경계임을 설명했다.
- 플랫폼에 DrawableSize·platform_present 추가. SDL은 drawable 픽셀을 매번 조회하고
  RESIZABLE/ALLOW_HIGHDPI 창 사용. 입력 전용 백엔드는 크기0·present 무동작.
  main은 양수 크기에서만 draw/present하고 end_frame으로 페이싱한다.
- triangle_probe는 실제 색상 픽셀을 CPU RGBA로 읽는다.128×128 결과를 선택적으로 PPM
  저장한다. 별도 glFinish 없음. 픽셀 읽기와 모니터 표시를 구분한다.
- 정상 PPM은 out/learning-checkpoints/18-triangle-check/triangle.ppm, PNG는 같은 위치.
  PNG는 실제 RGB 데이터를 그대로 인코딩했다.447바이트 PNG를 강의에 data URI로 넣어
  정적/하위경로 배포에도 외부 파일 없이 결과 예시가 보이게 했다.
- 프로그램·VAO 선택0 정리,8개 API 실패·비양수 크기·이름0·이전 오류를 대역 검사한다.
  잘못된 count24 대조군은 NDEBUG에서도 거부한다. 실제 버퍼 범위 밖 draw는 실행 안 함.
- tests/learning/drawable_probe.c는 기존 gl_probe.c를 포함한 SDL 호출 대역이다.
  platform_drawable.cpp는 창320×240과 drawable1280×960을 달리해 단위 혼동을 검출한다.
- tests/learning/current_frame_template.cpp에 실제 renderer_begin을 추출한다. 수정 전
  빈 영역에서 stale viewport가 남는 실패를 관찰했고 수정 뒤0영역/양수복원/미초기화 통과.
- Part3 section09/10/20을 부분 연결했다. section10은 신규 부분 연결이며 레터박스·
  마우스 역매핑 전체를 완료했다고 표시하지 않는다. Part11 발췌 갱신은 대응 완료 아님.
- 검수 탭 닫음, 뷰포트 복원,18-1 테스트 선택 해제. 사용자 탭·기존 답안 보존.
- 공개 배포·커밋·푸시는 하지 않았다. 다음 턴은19차시부터 진행한다.

## 완료한 17차시 인수인계

- 원고017.json·17-program·README·check_learning_program.py와 통합 검사 분기를 추가했다.
- OpenCode DeepSeek program 작업 exit0. 승인 자료/이벤트/초안은out/learning-jobs/program-*.
  실행 중 외부 작업 없음. 모델의 ‘detach가 독립성을 만든다’ 설명과 current 프로그램
  삭제를 GL 자체 오류로 오해할 수 있는 주석을 수정했다.
- Program은 API/current 컨텍스트를 빌리고 자기 프로그램 이름만 소유한다. 정상 컴파일된
  서로 다른 vertex/fragment 이름을 받아 attach/link/조회/성공 후detach한다.
  bool 실패/catch에서 자기 이름만 정리하고 빌린 Shader는 삭제하지 않는다.
- 성공 뒤 Shader 범위를 끝내도 프로그램은 남는다. 사용 종료 뒤 정리는 학습 정책이며
  GL 자체의 current 프로그램 지연 삭제와 구별한다. reset은 UseProgram을 호출하지 않는다.
- GlApi는31함수. 로더 검사의 비트 마스크를 누락 인덱스(-1없음/-2전체)로 교체했다.
  후속 함수 추가 때32비트 shift 문제를 되살리지 않는다.16-shader는 변경하지 않았다.
- program_probe는GLSL330 vec3/vec2 입력 불일치를 실제 출력 계산에 사용하며,
  COMPILE_STATUS 성공·LINK_STATUS 실패·GetError0을 구분한다. 프로그램A 선택 중 B링크가
  A선택을 유지하고, 부착 수0·셰이더 정리 뒤 B사용 가능·선택 해제 후 삭제를 관찰한다.
- program_contract는9개 API 경계·생성0·빈 실패/6000자 성공 로그·잘못된 길이·재시도·
  합성 예외·진단 보존·정리 멱등성·차용 Shader 미삭제·선택 미변경을 검사한다.
- 현재 production link_program을 추출해 생성0의 후속 호출 중단/셰이더 정리와
  컴파일 실패·링크 실패·성공의 소유 흐름을 검사한다. 전체 게임 런타임 검사는 아님.
- Part3 section20의 발췌·stage 인터페이스·성공/실패 경로 삭제를 보완해 근거 해시 갱신.
- 브라우저17-1 테스트 선택 해제, 검수 탭 닫음, 뷰포트 복원 완료. 사용자 탭 유지.
- GitHub Pages 준비물을 재생성했다. 실제 배포·커밋·푸시는 하지 않았다.

## 완료한 16차시 인수인계

- 원고016.json·16-shader·README·check_learning_shader.py와 통합 검사 분기를 추가했다.
- OpenCode DeepSeek shader 작업은exit0. 자료/이벤트/초안은out/learning-jobs/shader-*.
  원본 GLSL 소스가 소유자보다 오래 살아야 한다는 오류와 GL3.3 stage가 두 개뿐이라는
  주석을 보정했다. 입력 오류의 진단 갱신·NUL 초기값·성공 로그 의미도 수정했다.
- Shader::compile은 string_view 길이를 명시하며 bool 반환과 C++ 예외를 구별한다.
  GL 소유 이름을 실패/catch에서 정리한다. main은 std::exception을 받아 플랫폼을 종료한다.
- 소스/컴파일/로그6함수를 추가해 GlApi23함수. 타입은 호스트 Khronos와 일치한다.
- 로더 검사의 unsigned 비트 마스크는 현재23함수에 맞는다. 후속 함수가32개 이상이 되기
  전에 missing-index/전체 누락 표시 방식으로 바꿀 것.32비트 shift를 추가하지 않는다.
- 실제 shader_probe는 소스 복사 후 원본 소멸·COMPILE_STATUS0/GetError0·정상 두 stage·
  실패 후 재시도를 관찰한다. 긴 성공 로그와 빈 실패 로그는 대역으로 검증했다.
- tests/learning/current_shader_template.cpp에 실제 production compile_shader를 추출해
  생성 실패 뒤 소스/컴파일/조회로 가지 않는지 확인한다. 구현을 복사한 가짜 함수가 아니다.
- Part3 section20의 현재 발췌·소스 수명·컴파일/로그 계약을 보완하고 근거 해시를 갱신했다.
- 브라우저16-3 검수 선택 해제, 검수 탭 닫음, 뷰포트 복원 완료. 실행 중 외부 작업 없음.
- 학습 원고 생성 임시 Python에서 역슬래시 이스케이프에 주의할 것. JSON 스니펫의 실제
  NUL과 문자열 안 개행을 보정하고 C++ 스니펫7개를 문법 검사했다.

## 완료한 15차시 인수인계

- 원고015.json·15-vao 체크포인트·README·check_learning_vao.py와 통합 검사 분기를 추가했다.
- OpenCode DeepSeek vao 작업 완료(exit0). 선택 자료와 이벤트/초안은out/learning-jobs/vao-*.
  입력0과 중복 configure의 우선순위를 혼동한 후보 문제를 제외하고5문제를 재작성했다.
- VertexArray는 이름만 소유하며 buffer는 빌린다. gl_api는17함수다. 로더 검사는 개별17개
  누락+전체 누락+성공/실패 재로딩으로 구성했으며2^17 전수 검사를 주장하지 않는다.
- vao_probe는 두 VAO/두 VBO로 캡처·바인딩·활성화 독립성·y offset4·stride8을 확인한다.
  잘못된 stride4가 GL 오류 없이 기록되어도 의미가 틀릴 수 있다는 예제를 포함한다.
- 초기 관찰에서 VAO0의 EnableVertexAttribArray는 error0이었다. 이를 표준 허용으로
  해석하지 않았다. GL3.3 Core가 명시한 VertexAttribPointer 실패로 관찰 경로를 구성해
  0x0502를 확인했다. 자세한 근거와 이 차이는 REVIEW_LOG에 보관한다.
- Part3 section20의 VAO/ARRAY_BUFFER 구별·float 단위 offset 변환·stride0 설명을 보완했다.
- 현재 게임의 기능 코드는 이번 턴에 변경하지 않았다. 실습 코드는 별도 체크포인트다.
- 다음 단원의 로더·README·코드 목록도 실제 추가 함수에 맞게 갱신해야 한다.
- 실행 중 외부 모델/빌드 작업 없음. 전체 목표 완료 아님.

## 완료한 14차시 인수인계

- 원고014.json과14-vbo 체크포인트·README·검사 스크립트를 모두 완성했다.
- OpenCode DeepSeek vbo 초안의 include·멤버·상수 오타를 검토해 수정했다.
  이동 금지를 RAII 일반 법칙으로 설명하지 않고, 진입 오류 처리도 예제 정책으로 한정했다.
- VertexBuffer는 단독 소유·중복 upload 거부·실패 reset·context 이전 소멸 계약이다.
  readback으로 원본 수정/소멸 후24바이트 유지, unbind 후 보존,16바이트 재정의를 관찰한다.
- Part3 section09/20의 복사·사용 힌트·오펀링·삭제 설명을 보정하고 근거 해시를 갱신했다.
  재정의가 물리 새 할당이나 무대기를 보장한다는 표현을 사용하지 않는다.
- scripts/check_learning_vbo.py를 통합 체크포인트 검사에 연결했다. 이미 이 턴에 통과했다.
- 강의 본문에 작성자 환경·검수 결과 보고를 넣지 않았다. 이 파일과 REVIEW_LOG에 보관한다.
- OpenCode vbo·learner-prose 작업 모두 완료. 실행 중 외부 작업 없음.
- 검수용 브라우저14-1 선택을 해제하고 검수 탭을 닫았다. 사용자 탭·기존 답안은 유지했다.
- 다음 체크포인트는14-vbo를 기반으로 하며 README·로더 계약·코드 연결을 함께 갱신한다.

## 직전 13차시 인수인계

- 직전 턴은12차시 제작 결과와 최종 화면 검증·기록 갱신을 완료한 progress였다.
  이번 턴도13차시 원고·기준 코드·문제·관련 설명을 실제 수정했다. 전체 목표는 active다.
- OpenCode DeepSeek vertex-data 작업 완료. 자료/응답은 out/learning-jobs/vertex-data-*.
  실행 중인 외부 작업 없음. 하드코딩 개수 함수·약한 clear 검사·일부 문항 표현은 수정했다.
- 13-vertex-data는12-gl-loader의 플랫폼/로더를 유지한다. renderer/mesh.h는 Vertex2와
  Triangle/std::array·make_triangle·byte_count를 제공하고 형식 전제를 static_assert로 명시한다.
- study_mesh는 INTERFACE 타깃이다. CPU layout_demo/mesh_contract 및 SDL tetris가 공유한다.
  SCRIPTED 선택으로 SDL 개발 패키지 없이 CPU 실습 가능. main에는 CPU 데이터 준비만 추가했다.
- scripts/check_learning_mesh.py에서 SDL/SCRIPTED Release 빌드·출력·CTest·형식 오류와
  좌표 오류를 검증한다. 통합 체크포인트 검사에도13번 분기를 연결했다.
- 본문은 포인터/컨테이너 크기와 payload, 멤버 패딩/원소 stride, 구조체/평평한 float 배열,
  size/capacity와 무효 포인터를 구분한다. C++ 표준 초안의 필요한 계약을 링크했다.
- 현재 renderer.cpp의 ‘재할당 방지’ 주석을 capacity 조건으로 보정하고 텍스처1개 제한을
  GL 보편 제약이 아닌 이 배처의 정책으로 설명했다. 실행 동작은 바꾸지 않았다.
- Part3 같은 발췌·14float의56바이트 전제·vector 범위/수명 설명을 수정했다. 전체 셰이더·
  업로드/배칭 내용까지 검토 완료로 표시하지 않고 section08/09를 부분 연결로 기록했다.
- 새 원고의 괄호 뒤 한국어 조사와 강조 표시가 충돌해 별표가 노출된 부분은 원고에서 보정했다.
  const 배열에서 받은 포인터 설명은 const Vertex2*로 통일했다.
- 브라우저13-1 테스트 선택은 해제했다. 완성 파일5개 접힘·문제/복습 기본 접힘 확인.
- 기록 파일 이동은 다음 작업으로 되살리지 않는다.

최종 정적 배포물: `f687b49528cb72bd` (15개 파일). 허용 목록·상대 경로·해시·ZIP 재현성 통과.

## 완료한 21차시 인수인계

- 021.json·21-quad·README·check_learning_quad.py와 통합 검사 분기 추가.
- OpenCode DeepSeek quad 작업 exit0. quad-material.txt, quad-events.jsonl,
  quad-draft.json은 out/learning-jobs/에 보관. 실행 중 외부 작업 없음.
- DeepSeek의 quad.h 초안을 검토해 뒤집힌 winding의 공유 방향 주석과 edge의
  부호 있는 면적 두 배 표현을 수정했다. 구현/최종 수업은 직접 대조했다.
- Quad는 Vertex2 여섯 개 ABC/ACD, reverse는 ACB/ACD. 위치 8바이트, 전체 48바이트.
  included_edge는 y-up CCW의 dy<0 또는 수평 dx<0. covers_top_left는 raster의
  유효성/inside/outside를 먼저 사용하고 경계만 정규화해서 소유 여부를 계산한다.
  optional<bool>의 false와 nullopt를 구별한다. GL의 특정 변 소유 방향으로 일반화하지 않는다.
- VertexBuffer는 포인터/개수와 array 오버로드, 성공한 count_를 보관한다.
  raw 포인터의 실제 읽기 가능 범위는 호출자 책임. GLsizei/GLsizeiptr 한도를 곱셈 전에 검사.
  실패/해제는 0, 기존 버퍼 재업로드 거부는 기존 count를 보존한다.
- draw_triangles는 available/first/count를 검사하고 count%3==0 정책을 둔다.
  first+count 대신 음수/순서 검사 후 available-first 사용. available은 caller metadata라
  VAO의 실제 버퍼와 일치해야 한다. GL이 메타데이터를 인증한다고 설명하지 않는다.
  기존 draw_triangle은 3/0/3 래퍼. draw마다 clear하며 culling은 호출자가 선택한다.
- GlApi에 Enable/Disable/FrontFace/CullFace를 추가해 총41개. 모두 GLenum→void,
  STUDY_GL_CALL ABI와 Khronos 선언 일치 검사. 누락 테이블은 전부 거부한다.
- quad_sources.h는 gl_VertexID<3에 따른 두 색을 flat 전달한다. 기존 세 원소
  색 배열로 여섯 정점을 읽지 않는다. 이전 raster probe의 count3 경로는 유지한다.
- quad_demo의16×16 격자: 닫힌 경계 중복8, 열린 경계 구멍8, 소유권 적용0/0.
- quad_probe: main과 달리 singlebuffer 요청/실제 버퍼 읽기. 네 모드 전체 결과를 읽고,
  culling을 끈 뒤 첫/둘째를 각각 clear하여 읽은 마스크 합이 사각형 안1·밖0인지 검사.
  reverse-cull 전체는 둘째 마스크와 일치. GPU의 어느 쪽이 공유 변을 가질지는 고정하지 않는다.
- 검사 로그: out/learning-checkpoints/21-quad-check/verification.log.
  같은 폴더 quad-quad/reverse/cull/reverse-cull.ppm 및 PNG는 실제 readback 결과다.
  물리 GPU/네이티브 창 출력/타 OS를 검증했다고 주장하지 않는다.
- Part3 section-09 부분 대응 해시·강의21 연결 갱신. 전체 배칭/속성 대응은 남아 있다.
- 정적 최종본: out/learning-site/releases/2d54440dbebcad3d/ 및 동일 이름 ZIP.
  사이트15파일을 두 번 생성하여 동일 ZIP 바이트와 파일 해시를 확인했다.
- UI 임시 QA 탭은 닫고 모바일 viewport를 원복한다. 사용자 탭/기존 답안은 보존한다.

## 완료한 22차시 인수인계

- 022.json·22-blending·README·check_learning_blend.py와 통합 검사 분기를 추가했다.
- DeepSeek CPU helper 작업 exit0. out/learning-jobs/blend-material.txt,
  blend-events.jsonl, blend-draft.json 보관. 실행 중 외부 작업 없음.
  초안에 alpha0의 straight 복원 불가·LDR/HDR 범위 차이·noexcept를 보완했다.
- blend.h: Rgba(double4), valid_rgba, valid_premultiplied, premultiply,
  over_premultiplied, over_straight. 유한0..1/LDR RGB≤alpha 정책, invalid nullopt.
  over_straight의 destination과 두 함수의 출력은 선곱. alpha0은 나눗셈 변환 불가.
- triangle.*: begin_color_frame은 크기/색 검증→viewport→clearcolor→clear.
  submit_triangles는 범위/이름검증→program/VAO→draw→해제. clear/viewport 변경 없음.
  이전 draw_triangle/draw_triangles는 begin+submit 래퍼. invalid range/name은 clear보다
  먼저 거부한다. 이전 상태 복구 API가 아니다. 프레임 시작3호출·제출5호출 실패를 검사했다.
- blend_scene.h: make_layers는12정점96바이트, A x[-.75,.25], B[-.25,.75], y[-.5,.5].
  vertexID<6은 red alpha.5, 나머지 blue alpha.5. first6에 ID가 반영되는 조건.
  shader는 straight 또는 RGB*alpha를 출력. uniform/색속성을 아직 도입하지 않는다.
- 모드 ab/ba/premul/double-alpha/replace/legacy-alpha. 정상 RGB계수는 straight→As,
  선곱→ONE, destination→1-As. 정상 alpha계수 ONE/1-As. legacy는 alpha에도As.
  configure는 culling/sRGB/dither 끄고 FUNC_ADD 및 계수를 명시, BLEND on/off.
  render는 한 번clear 뒤 두 submit. main은 doublebuffer/플랫폼 경계를 유지한다.
- GlApi43개: BlendEquation(GLenum), BlendFuncSeparate(GLenum4) 추가. Khronos ABI 일치,
  각 엔트리 누락/전체누락 재로딩 거부 검사. current production의 GL 함수목록 변경은 없다.
- blend_probe는6모드×배경alpha0/1, actualGL3.3+Core/단일샘플/alpha8bit 이상 확인.
  RGB뿐 아니라alpha도 독립 numeric oracle과 모든픽셀에서 비교한다. 채널허용2/255.
  actual llvmpipe4.5 Core 중심값: ab alpha0=(64,0,128,192), alpha1=(64,0,128,255).
  ba는 red/blue 비중 반전, premul은ab일치, double-alpha=(32,0,64,192/255),
  replace=(0,0,255,128), legacy=(64,0,128,96/159). 단일버퍼 probe와 main은 구분한다.
- 실제 PNG/PPM 및 verification.log는 out/learning-checkpoints/22-blending-check/.
  새 패스로 이전 quad probe도 정상/반전/cull/반전cull 마스크 검사 통과.
- Part3 section08/09/19/20, Part4 section16을22차시와 부분 연결했다. section19·16은
  넓은 GUI/통합패널 내용 중 alpha 관련 일부만 대조했다. 완전 대응 상태로 올리지 않는다.
- 본문은 작업환경 보고를 포함하지 않는다. sRGB/LDR/HDR/dithering은 적용 범위와 함께
  설명했다. Markdown에서 **(수치)** 뒤 한국어 때문에 강조가 남는 문제를 inline code로 수정.
- 최종 정적본 out/learning-site/releases/817273a74e14c9da/ 및 동일 이름ZIP. 15파일.
  빌드 보조 원고 경로/현재 renderer_init 심볼의 생성기 오류를 수정한 후 재생성/최신성 통과.
- QA탭22 닫음, viewport 원복. 22-1 테스트 선택만 해제, 기존 사용자 탭/답안 보존.

## 완료한 23차시 인수인계

- 023.json·23-present·README·check_learning_present.py 및 통합 검사 분기 추가.
- DeepSeek의 단순 CPU시점 helper 작업 exit0. out/learning-jobs/present-material.txt,
  present-events.jsonl, present-draft.json 보관. 실행 중 외부 작업 없음.
- cpu_timing.h: 네uint64시점과frequency를 받아 CpuStages(submit_ms/sync_ms/present_ms).
  frequency0/역행 거부, 동일시점허용. 정수차이 후double변환. helper주석의 큰 시점 반올림
  표현과 ticks_to_ms의 주파수전제 보정. 실제 main은 steady_clock프레임원점→정수ns.
  나노초단위와 실제해상도/CPU사용시간/GPU시간/표시시각을 구분한다.
- submission.h: Mode submit/flush/finish,parse,boundary. invalidmode·dirtyentry 거부,
  mode별Flush/Finish선택후GetError. Flush/Finish45엔트리에 추가, Khronos ABI일치검사.
  submit모드도오류조회가있어완전무오버헤드가아님. 모든타이밍은진단계측경로다.
- platform API(학습용23만): SwapIntervalReport{attempted,accepted,reported_interval},
  platform_set_swap_interval(0|1). 세션/current context/current window 검사.
  reported0은unknown가능. 실패시query값을requested로덮지않는다.
- bool platform_present는positive drawable의현재세션SDL swap API dispatch 여부만 반환.
  SDL2void호출이라native성공/표시완료인증아님. input-only SCRIPTED/WIN32는false/{}.
  production의voidAPI는변경하지않았다.
- main은22의ab합성그림유지. CLI tetris[submit|flush|finish][0|1],잘못된입력은init전2종료.
  유효swap표본120개후종료/ESC닫기가능. submit→boundary→present 시점저장.
  false present경로도end_frame도달하도록continue삭제. 로그는측정뒤,1/60/마지막표본만.
  60Hz소프트웨어페이싱은interval0에서도유지한다.
- present_probe는실제GL에서세모드를실행한뒤(타이밍구간밖)RGB모든128x128픽셀검사.
  readback자체가동기화할수있으므로앞선반환시점의완료를증명한다고쓰지않는다.
  doublebuffer요청1/실제0,interval0/1은각각accepted1/report0/1.
  이것은실제모니터VSync증거가아니다. main은실제0때기존요구대로거부했다.
- tests/learning/present_probe.c는공통gl_probe.c를재사용한LinuxLD_PRELOAD대역.
  tests/platform_present.cpp와함께세션불일치·zero drawable·closing·intervalfailure,
  accepted/report불일치를검사. 실제GL과별도로분리했다.
- SDL/SCRIPTED각12CTest·두새ABI·대역검사·정밀도오류대조군통과.
  로그out/learning-checkpoints/23-present-check/verification.log. 실제GUI/물리GPU미검증.
- Part2section04/11/12,Part3section04/20의부분대응과해시갱신. 전체coverage24partial/344.
  Part2원문의stale SDL vsync주석을현재source발췌로동기화했다. platform.h주석만수정.
- 학습본문에는실행환경보고를넣지않았다. scanout주사/tearing/vblank/합성기정의보충,
  'Finish8ms=GPU대기8ms'오인이없도록CPU호출구간경과로표현했다. 실제측정첫표본의
  75ms와다음0.04ms를성능순위증거로쓰지않는다(초기화/드라이버준비/진단오버헤드혼재).
- 정적최종본out/learning-site/releases/09f3bcc1ee70816d/와동일ZIP(15파일),재현성통과.
- QA탭23닫음/viewport원복/23-1테스트선택해제,사용자탭과기존답안보존.

## 완료한24차시 인수인계

- 024.json·24-letterbox·README·check_learning_letterbox.py·통합 검사 분기 추가.
- OpenCode DeepSeek CPU helper 초안 작업 exit0,events는step_start/text뿐.
  out/learning-jobs/letterbox-material.txt·letterbox-events.jsonl·letterbox-draft.json 보관.
- letterbox.h: Size/Rect/Point/Layout,make_layout,gl_y,window_to_logical,logical_to_window.
  int최대32비트전제·64비트곱셈·floor fit,viewport좌상단. 후속함수는 수정안한Layout전제.
  입력반열린범위/finite 검사,정방향닫힌범위. 실수경계왕복정확성은 보장하지 않는다.
- letterbox_scene.h: logical320×240,red(40,60)-(200,180),blue(120,60)-(280,180),12정점.
  초기화때ui_to_clip,창크기에따라VBO업로드불필요. 최대viewport초과거부.
  scissor끄고검정전체clear→viewport/scissor설정→녹색.25G배경clear→두삼각형묶음.
  기본framebuffer/쓰기마스크/depth·stencil비활성전제. 상태복원범용패스아님.
- GL46엔트리(Scissor추가),전부누락/개별누락로드검사,PFNGLSCISSORPROC ABI일치.
- 플랫폼WindowSize/WindowMouse추가. SDL창좌표와drawable픽셀분리,mousefocus구분.
  main은같은Layout입력/그리기,Escape/닫기까지지속. presentation_demo는23진단보존,
 120표본후종료. 새Usage이름수정·대상빌드/잘못된CLI검사완료.
- 기존23 tests/platform_present.cpp는복사돼있으나24checker가재실행하지않는다.
  새tests/platform_letterbox.cpp와root tests/learning/letterbox_probe.c가24조회대역검사.
- production platform/mouse_coordinates.h:64비트차이/곱셈·음수몫floor·int포화·invalid-1.
  SDL/Win32공유. 루트tests/learning/mouse_coordinates.cpp(UBSan),current_platform_focus.cpp
  (실제SDL어댑터호출)로경계검사. Windows네이티브는실행하지않았다.
- Part2section04/06/10/14,Part3section10,Part11section06에부분연결. coverage26partial/344.
  Part11의1080×960좌측바사례와'좌상단→중앙'잘못된예를원문정리중수정했다.
- 실제GL5크기에서전체RGB오차≤2/255비교. 먼저magenta전체clear/작은scissor/viewport로
  dirty상태를만든뒤begin을실행. PPM은out/learning-checkpoints/24-letterbox-check/frame-*.
  첫resize버전은offscreen이이전surface폭유지하는차이로실패,새창방식으로분리했다.
- CPU홀수yflip누락/production음수floor삭제대조군실패확인,Release에서검사유지.
  최종verification.log·site-verification.log는out/learning-checkpoints/24-letterbox-check/.
- 브라우저 첫퀴즈 이동의CDPtimeout은실제hash이동완료;fresh상태확인후재클릭없이진행.
  정답확인후24-1만선택해제. 원문뷰어보관스냅샷새helper확인. 모바일표문구짧게수정.
  임시탭24닫음/viewport원복. 최종정적묶음 1ea4cab42a8e7512 및동명ZIP,15파일.

## 완료한25차시 인수인계

- 025.json·25-grid·README·check_learning_grid.py·통합 검사 분기 추가.
- DeepSeek선택명세out/learning-jobs/grid-material.txt,응답grid-events.jsonl,grid-draft.json.
  exit0,step_start/text만,추가탐색/명령없음. 주작업자검토적용후C++17빌드검사.
- simulation/grid.h:Cell(enum class uint8_t empty0/filled1),Position,Grid.
  kRows20/kColumns10/kCount200,std::array<Cell,200> private 값 소유.
  contains→index_of,position_of,get(optional),set(bool),is_empty(outsidefalse),clear,cells.
  set은Cell2/255등거부,실패시상태보존. 전체셀색/블록ID는후속확장사항.
- public const배열참조는소유자변경을관찰한다. auto값복사/constauto&대여와Grid복사
  차이를검사했다. 임시Grid에서참조를보관하지않는수명계약을본문에설명.
- board_example.h는(0,0),(1,2),(19,9) 채움. grid_demo는20행ASCII/인덱스12왕복/
  optional존재여부/복사본수정출력. main은board를생성후run_session(constGrid&)에전달,
  점유개수3만출력. 보드그림은26차시산출물이라고명시.
- CMake study_grid INTERFACE/grid_demo/grid_contract. CPU툴컴파일/링크에SDL/GL
  의존없음. SCRIPTED/SDL각14CTest. 창용tetris는SDL에서만빌드한다.
- tests/grid_contract.cpp:전200개순번/왕복/중복·모든셀채움,외부각축음수/경계/극단값,
  size_t최대·허용하지않은enum·상태보존·복사/참조. static_assert + NDEBUG런타임CHECK.
- tests/learning/current_grid.cpp:현재SimGrid초기빈칸/ID0·8대1..7·9/경계/독립복사.
  두CPU검사를UBSan에서도실행. flat-only대조군은컴파일static_assert하나만제외하고
  런타임경계검사실패를확인했다. 결과log는out/learning-checkpoints/25-grid-check/.
- production src/sim_grid.h주석:int연속성/해시표현전제/public접근범위수정.
  src/game.cpp DrawBlockMini주석:GetCellPositions는offset더한위치라는설명으로수정.
  게임런타임동작수정없으므로hash골든재생성/넓은게임회귀재실행하지않았다.
- Part1:int선택이유/sizeof(int)==4전제/가상주소연속성/캐시단정/고스트별도표현/
  헤더include와링크실패구분수정. 부록전체소스주석도동기화. section03/04/21부분연결.
  coverage29partial/344,완전대응0. 현재GameDrawGrid는src/game.cpp99행부근참고.
- C++공개초안array.overview/dcl.array/expr.add로배열/포인터경계계약확인.
  학생본문에는집필환경보고없음. 검증한새main범위는빌드연결이며native창실행은아님.
- 119객관식·Part체커·인라인6개소스일치·정적bundle재현성통과.
  QA25-1만오답/정답/해제.스냅샷SimGrid새주석확인.모바일375px문서가로넘침없음.
  복습/퀴즈/완성6파일기본접힘,콘솔0.임시탭닫음/viewport원복.
- 최종정적out/learning-site/releases/c7a851cf09753b95 및동명ZIP,15파일.

## 완료한26차시 인수인계

- 026.json·26-board-render·README·check_learning_board_render.py·통합 검사 분기 추가.
- DeepSeek 자료board-render-material.txt/응답board-render-events.jsonl/board-render-draft.json.
  처음 sandbox 로그 EROFS로 시작 실패 후 승인된 escalation에서 API 작업 exit0.
  이벤트 step_start/text만. 추가 탐색/명령 없음. 실행중 작업 없음.
- board_geometry.h:고정logical320×240,origin110,20,pitch10,ink9. cell_rect 축별검사,
  cell_at finite→전체칸반열린범위→int변환. 간격도 앞선 칸에 포함하는 정책.
  Mesh는Vertex2[1200]·empty_vertices. 두 순회(empty/filled) 각각row-major.
  BL BR TR / BL TR TL→ui_to_clip. Grid 두 상태 불변식·유효 루프좌표를 전제로 한다.
- 초안의 잘못된 ../grid/grid.h include 수정, 실패를(0,0)으로 숨기는 fallback 제거.
  고정배치 static_assert 추가. 내부 helper를 임의입력 API처럼 사용하지 않는 계약 설명.
- board_scene.h:vertex 위치만, fragment2개 고정불투명색(empty .125/.25/.375,
  filled1/.5/0). GL_API46 그대로. begin은24letterbox함수 재사용, 배경 .03125/.0625/.09375.
  blend/cull/sRGB/dither끄고 두 그룹 제출; 빈 그룹 생략. metadata1200/multiple6검사.
  default framebuffer/write masks/FILL/depth-stencil-rasterizer discard off전제. 상태복원아님.
- main은고정board→mesh→VBO한번,프로그램둘,마우스논리→cell_at로그. Grid수정은자동
  반영되지 않는다. upload기존최초한번계약 유지. 다음동적시점엔갱신설계필요.
- board_render_demo (1,2)=[130,30)-[139,39),1182/18/1200/9600바이트,간격hit1,2.
  15CTest각backend/경고0,CPU link검사,4패턴전1200정점/CCW/입력극단/스냅샷/불변,
  UBSan. x축에row쓰는대조군은Release CHECK에서실패.
- board_render_probe는예제/전체빈/전체채움 각각640×480,960×480,640×960,320×240.
  바깥bar/배경/칸/간격RGB전픽셀독립예측과≤2오차. GL4.5llvmpipe singlebuffer.
  dirtymagenta/scissor/viewport설정후잘못된empty_vertices1 거부시원본픽셀보존확인.
  실제새main의창표시/입력/프레젠테이션검사는아니다. Windows/macOS/물리GPU미검증.
- frame-p0-320x240.ppm을무손실PNG인코딩해본문dataURI포함. 뷰어style.css에
  lesson-prose img max-width100%/heightauto추가. 인라인8개소스정규화일치.
- Part4 section04: include만으로link실패단정, Game교체만으로전역상태잔재불가능단정
  교정. DrawGrid의rowcol→xy/pitch와ink/논리간격설명추가. section14 draw_rect정점
  누적중texture변경flush가능성교정. 현재renderer.cpp ensure_texture/glb_flush대조.
  두절부분연결추가:31partial/344,0covered. production코드변경없음.
- Khronos OpenGL-Refpages/main/gl4/glDrawArrays.xml로first/count/count0계약확인.
  명세링크는본문메모. 집필환경보고는validation/작업기록에만 둔다.
- QA26-1 오답·정답·선택해제,코드src/game.cpp스냅샷,모바일375px문서와320px이미지.
  body PageDown 도구실패후현위치확인·CUA이미지클릭으로화면스크롤해육안검수.
  페이지결함으로취급하지않음. 임시탭26닫음/viewport원복/사용자답안유지.
- 최종정적 a5445ea7c9e653f8,15파일·재현가능ZIP. 로그는
  out/learning-checkpoints/26-board-render-check/{verification.log,site-verification.log}.

## 완료한27차시 인수인계

- 027.json·27-local-piece·README·check_learning_piece.py·통합검사분기 추가.
- DeepSeek 선택명세piece-material.txt/응답piece-events.jsonl/piece-draft.json.
  승인된 escalation호출 exit0,step_start/text만. 추가탐색·명령·실행중작업없음.
- simulation/piece.h:LocalCell/Origin별도struct,Shape=array4,BoardCells=arrayPosition4.
  T=(0,1),(1,0),(1,1),(1,2),Piece가모양/기준점값소유. local_bounds는inclusiveextrema.
  to_board는각피연산자를int64로넓힌뒤더하고int범위확인;intdigits<=31정적전제.
  전체네좌표 또는nullopt. 밖의좌표/중복모양도변환하며충돌/등록유효성검사는아니다.
  반환은스냅샷. 초안은includeguard→pragmaonce/cstddef명시 정도로검토적용.
- board_geometry.h:quad_vertices(Rect) 공유함수추출. 기존보드 helper가그6정점붙임.
  이전26기준체크포인트는보존. 유한한고정cell_rect결과전제; 임의범용Rect검증함수아님.
- piece_geometry.h:visibleMesh24최대/count실제prefix. 보드밖칸은생략,좌표보정/규칙변경없음.
  piece_scene.h:청록(0,.75,1,1),보드pass상태사용,중간clear없음. count0이면GL호출없음.
- main은board와Piece(4,3)고정. 별도VBO/VAO/program소유,nonempty만최초upload.
  board뒤overlay그림. 창resize는레터박스만. CLI입력/키이동/충돌판정은아직없음.
- piece_demo5사례:4,3→4칸24정점;-1,3→3칸18정점;-4,3→0;19,8→1칸6정점;
  INT_MAX,3→arithmeticfailure. negativeworld좌표반환과보이는수구분.
- CPU16CTest각backend/경고0·SDL/GL독립링크·UBSan.
  27행범위(-4..22)×17열범위(-4..12)=459기준점,모든좌표/상대차이/visible정점비교,
  origin보상,Piece복사/BoardCells스냅샷,정수min/max/잘못된늦은확장대조군검사.
- root tests/learning/current_piece.cpp는현재SimT/O/SimBlock와position.cpp연결하여
  오프셋·vector스냅샷·map/vector독립값복사·기본SimBlock의없는회전키예외검사.
  production현재Move의임의극단값은계약밖이므로실행하지않았다. 주석만보완.
- piece_probe5배치(4,3/-1,3/-4,3/19,8/0,0)×640x480/960x480/320x240.
  배경/여백/보드/간격/청록overlay전체RGB독립예측≤2오차. 겹침시청록이덮어도Grid보존.
  실제GL4.5Corellvmpipe software/singlebuffer. native새main창표시·입력검사는아니다.
- actualPPM2개무손실PNG→본문/접힘메모dataURI. style.css .lesson-note img에도반응형제약.
- Part1 section05부분연결추가(32partial/344). Move0,3이중앙좌표가아님/O는0,4,
  rot1/rot3ASCII다름,GetCellPositions좌표와초기화전제,ghost는값복사+ID8교정.
  section19 T-spin설명:공통3x3틀pivot(1,1),점유boundingbox와구분. B2전체SimBlock주석동기화.
  C++ 공개초안expr.pre로signedoverflow설명확인,본문공식메모링크제공.
- 129문제·Part체커·인라인6일치·정적상대경로/hash/ZIP재현성/diff검사통과.
  최종out/learning-site/releases/02af1b7177e135e7 및동명ZIP(15파일).
- QA27-3오답정답확인후해제.새source주석스냅샷조회성공.모바일375px문서/320px그림
  overflow없음,접힌메모펼쳐이미지실제화면확인.콘솔0/임시탭닫음/viewport원복.

## 완료한 28차시 인수인계

- 028.json·28-piece-catalog·README·check_learning_catalog.py·통합 검사 분기 추가.
- DeepSeek 선택 명세/응답/초안은 out/learning-jobs/catalog-material.txt,
  catalog-events.jsonl, catalog-draft.json. 호출 exit0, step_start/text만 사용.
  추가 저장소 탐색·실행 중 외부 작업 없음. 주 작업자가 계약 검토·적용했다.
- simulation/catalog.h: Kind uint8_t(L1/J2/I3/O4/S5/T6/Z7), Definition(kind,name,
  cells,spawn), 배열 순서는 I/J/L/O/S/T/Z. O 기준점(0,4), 나머지(0,3).
  T는 inherited t_shape 값으로 초기화, cstddef 명시. string_view는 정적 리터럴 참조.
- valid_shape는 0..3 범위를 먼저 검사하고 중복을 거른 뒤 Manhattan 거리1의 간선을
  따라 reached를 전파한다. 네 pass·모든 쌍 비교, 저장 순서 독립. 대각선 불허.
  고정 네 칸 전제이고 임의 다형상/BFS 일반 구현이 아니다.
- valid_catalog는 ID1..7, 대문자1바이트 이름, 중복, 구조, 기준점과 초기 네 칸이
  20×10 안인지 검사한다. static_assert로 컴파일 시점 등록 검사. 충돌/게임 종료나
  T ID에 정확한 T 모양인지 검사하지 않는다. 교체한 J 모양을 허용하는 대조 예제로 설명.
- find_id는 외부 int를 Kind로 좁히지 않고 저장 ID를 넓혀 비교한다. raw257은 없다.
  find_name 정확한 대소문자 비교. make_piece는 로컬 배열/기준점을 복사해 optional 반환.
  Piece의 Kind 부재·main이 종류를 따로 기억하는 설계와 미래 확장 경계 명시.
- main은 대문자 이름/default T/help/오류 검사 → Piece 생성 → platform_init 순서.
  --help 종료0, 잘못된 인수2, 실행 실패1. 새 main 부분은 읽기 쉬운 줄바꿈으로 정리.
  공통 run_session은 정지 상태만 그린다. CPU catalog_demo는 일곱 종류 ASCII 출력.
- catalog_contract: 독립 ID/이름/스폰/16비트 마스크 기대값, lookup 포인터, 복사,
  잘못된 ID/이름/중복/극단 좌표/스폰, 의미 오류와 구조 통과를 구별한다.
  4×4의 16 choose4=1820집합 ×24순열 연결성을 별도 비트 보드 oracle로 비교.
  대각선도 이웃으로 보는 mutation은 Release 검사에서 실패했다.
- tests/learning/current_catalog.cpp는 현재 일곱 SimXBlock 생성 결과를 비교한다.
  실제 GetAllBlocks 호출은 하지 않았으며, 반환 순서는 src/sim_game.cpp 소스 대조.
- catalog_probe 실제 GL: 7종류×640x480/960x480/320x240. 배경/여백/보드/간격/
  청록 블록 전체 RGB 비교, ≤2/255. llvmpipe4.5Core software/doublebuffer0.
  새 main의 native 창·입력·물리 GPU·Windows/macOS 검증으로 확대하지 않았다.
- 로그 out/learning-checkpoints/28-piece-catalog-check/verification.log,
  final-build.log, site-verification.log. T 상수 재사용 뒤 양 backend 해당 계약 재검사.
  마지막 main 서식 정리 뒤 빌드·CLI 범위·9스니펫 소스 일치를 다시 확인했다.
- frame-p0-320x240.ppm을 무손실 PNG로 바꾸어 본문 data URI에 포함했다.
  Part1 section03/05의 원문 해시/부분 대응 근거 갱신. 총32partial/344, covered0.
  기존 routing 후보는 본문 검토 완료와 별개이며 자동 해시 갱신으로 덮지 않았다.
- UI 전체 DOM snapshot 도구가 두 번 CDP timeout. 탭은 정상 로드되어 작은 DOM 읽기와
  실제 스크린샷으로 검수했다. 도구 관찰 실패를 페이지 오류로 보고하지 않았다.
  소스뷰어는 QA 서버에 API가 없어 보관 스냅샷으로 정상 fallback, 화면에 명시됨.
- 정적 결과 out/learning-site/releases/5fb9e60d0fc1cdfc 및 동명 ZIP, 15파일 재현성 확인.
  docs/learning-companion.md의 오래된 “11차시 이후”도 실제 다음 차시29로 교정했다.
- 2026-09-23 환경 전환 뒤 기본 exec sandbox가 mountinfo 비절대경로 오류로 실패했다.
  동일 읽기/워크스페이스 편집·검증을 승인된 require_escalated로 수행. 자동승인 거절 없음.
  다음 턴에는 기본 exec를 먼저 시도하고 같은 인프라 오류면 범위 내 작업을 이어갈 것.

## 완료한 29차시 인수인계

- 029.json·29-horizontal-move·README·check_learning_movement.py·통합 검사 분기 추가.
- DeepSeek 자료/응답/초안: out/learning-jobs/movement-material.txt,
  movement-events.jsonl, movement-draft.json. 승인된 선택 명세만 전송, exit0,
  event types step_start/text. 초안에 Piece using 누락이 있어 using study_piece::Piece를
  추가했다. both released라는 부정확한 주석을 neither press edge로 고쳤다.
- simulation/movement.h는 Result idle/moved/blocked/invalid. 방향-1/0/1 검사→현재
  inside_board 검사→0이면idle→candidate복사→int64열덧셈/표현범위→후보좌표/경계→
  마지막 current=candidate. 실패/idle은 전체 값 불변. 연결성/종류/점유 검사는 아니다.
  기준점 자체가 음수여도 네 점유 칸이 안이면 허용. 현재가 밖이면 복구 이동도invalid.
- renderer/vertex_buffer.{h,cpp}: replace_same_size, gl_api.h DynamicDraw0x88E8만 추가.
  함수 포인터/로더 개수는 그대로. 유효한 현재count와 같은extent만 받고 이름 유지.
  entry/bind/data 단계 진단. GL 오류면 false, 소유 이름은 정리까지 보존하고 제출 중단.
  기존 최초upload 계약은 그대로이며 이전 버전 체크포인트는 수정하지 않았다.
- main은 Piece를 값으로 소유하는 세션. begin_frame→종료확인→의도/전이→moved만
  좌표/mesh/동일크기버퍼갱신→layout/그리기. 모든 점유 칸이 안이므로24정점192바이트.
  최소화 등의 layout무효와 GL객체유효는 별개; 버퍼 갱신을 layout분기 뒤로 미루지 않았다.
- movement_demo 열3에서 LLLL,0,R의 결과3→2→1→0→0→0→1.
  movement_contract7종×24행×16열×5방향=13440사례, 타입 극단/보상 원점/복사 보존.
  초기현재invalid+dir0도invalid. INT_MAX origin과 음수local로 유효좌표를 만든 뒤 +1
  시도에서 기준점 표현범위 실패를 확인했다. 조기대입 mutation은 Release에서 실패.
- buffer_replace_contract는 무효인수시GL무호출, 동일이름/반복복사/정확한192(실앱)/
  24바이트(삼각형대역), entry/bind/data오류 및 1회 해제를 검사한다.
  movement_input은 실제 SCRIPTED 플랫폼의 press/hold/release 프레임을 연결해1회이동확인.
- tests/learning/current_movement.cpp는 실제SimGame/position.cpp로32시드단일방향비교.
  SubmitInput만 호출하며 Tick/중력/게임전체/점유충돌의 동등성을 주장하지 않았다.
  양쪽 비트 정책은 별도 기대값으로 왼쪽벽0→1임을 확인했다.
- movement_real은 한 VBO/VAO로 각 종류 시작→왼벽→오른벽→왼쪽한칸을 연속 처리한다.
  성공마다 GetBufferSubData로 정점값 확인, 각관찰위치×3크기 전체RGB독립마스크비교.
  총84장. BufferData후 이름/정점수 유지, 앞선청록흔적이 재그리기로 사라지는지도
  전체RGB검사가 확인한다. frame-p5-pose1-320x240.ppm을본문무손실PNG로 포함했다.
- 실제 GL3.3 Core Mesa Intel(R) HD Graphics3000(SNB GT2),single-sample/doublebuffer0.
  native main 입력/표시나 타OS 검증은 아님. 실행근거는
  out/learning-checkpoints/29-horizontal-move-check/verification.log.
- Khronos 공식 glBufferData XHTML은도구content-type오류. 공식GitHub동일XML원문으로
  객체의저장소재정의/복사/usage힌트를확인했다. 본문링크는Khronos계약페이지다.
- Part1 section06부분대응추가(33partial/344,covered0). runtime코드변경없음.
- 이번CUA 브라우저는없음. 사이트신규차시실화면은미검수로validation/기록에명시.
  view_image도sandbox mountinfo오류여서 workspacePNG를승인된exec로읽어직접표시검수.
  shell은기본실행실패후승인된require_escalated사용. 자동승인거절없음.
- 139문제Node검사·Part/스니펫·정적ZIP검증통과. UI실화면불가를코드검사통과로덮지않았다.
- 최종 out/learning-site/releases/121768f136d7198a 및 동명ZIP,15파일.

## 완료한30차시 인수인계

- 030.json·30-collision·README·check_learning_collision.py·통합검사분기추가.
- DeepSeek자료/응답/초안:out/learning-jobs/collision-material.txt,
  collision-events.jsonl,collision-draft.json.선택명세만전송,exit0,step_start/text.
  초안의없는 study_piece::Position을auto로수정하고stdarray/optional중복헤더제거.
  -1/+1만허용해0을invalid로만들던실수를-1/0/1로교정했다.추가탐색·실행작업없음.
- simulation/collision.h Placement clear/outside/occupied/unrepresentable.
  classify는to_board전체결과→모든경계→점유 순서.같은도형의저장순서를바꾸어도분류동일.
  형상등록·연결성검사와단일배치·경로검사의차이를본문에명시했다.
- board-aware try_shift는direction/current clear검사→candidate복사→기존경계helper호출→
  moved일때만점유분류→occupied면blocked/예상밖분류invalid→마지막대입.
  Board읽기전용.0입력은현재clear일때idle.겹친상태를복구하는API는아니다.
- main초기배치검사와호출대상/콘솔안내를변경.좌우눌림정책과성공시GPU갱신은유지.
  sampleboard3셀유지.기본T의첫왼쪽은(1,2)셀로blocked,오른쪽과복귀는moved.
- collision_demo classify열3/2/-1/8=>clear/occupied/outside/outside,
  step -1/0/+1/-1/-1 열3→3→3→4→3→3.기존진단targets보존을README/본문에서구분.
- CPU검사는201보드(빈판+단일장애물200)×7종×5행×9열=63315배치,
  각방향3개=189945전이.독립scalaroracle와비교,Grid와Piece보존확인.
  T빈boundingbox모서리,밖+점유24순열,INT_MAX변환실패,잘못된direction/current,
  보상원점,자기기록충돌,목적지clear/중간occupied반례를별도로검사했다.
- SCRIPTED입력검사:첫Left press만blocked1회,hold/release등idle3회,열3유지.
  collision_contract는Release CHECK사용.점유조건을무시하는mutation은실패했다.
- tests/learning/current_collision.cpp는공개SimGrid/SimBlock기반33768배치비교.
  game의private BlockFits를직접호출하지않았으며,설명/메타데이터에서검사범위를명확히구분.
  현재Grid는0/8이empty라는호환규약,가비지9는filled,밖은false를확인했다.
- collision_real은7종×시작/왼장애물앞/오른벽/왼한칸×3크기84화면.
  왼쪽최소origin은Z2,나머지3(보드의(1,2)장애물때문);오른쪽끝은종류폭기반독립예측.
  moved만버퍼갱신,기존VAO/VBO유지,GetBufferSubData와전체RGB비교.고정보드값불변.
  IntelHDGraphics3000 GL3.3Core,doublebuffer0;native키·모니터·타OS증거아님.
- 실제frame-p5-pose1-320x240.ppm을무손실PNG변환하여본문dataURI에삽입,
  blocked-t.png로도보관·직접육안확인.주황색(1,2)오른쪽에T가멈춘그림.
- Part1 section06에단일배치와경로·활성블록을Grid에미리쓰면자기충돌·진단우선순위설명추가.
  section04/06의lesson30부분대응근거갱신.33partial/344,covered0;§4.3하드드롭은후속.
- 144문제Node검사·Part현재발췌·5스니펫·정적export15파일/ZIP재현성/diff통과.
  로그는out/learning-checkpoints/30-collision-check/{verification.log,site-verification.log}.
- 기본shell실행은mountinfo오류.승인된require_escalated로workspace작업진행,자동승인거절없음.
  CUA목록은빈상태라29/30실화면검수는남았다.반복poll대신독립집필진행가능상태로기록.
- 정적결과out/learning-site/releases/1dfab8b808ddd7d2및동명ZIP.

- 148 커밋 전: stage56840완료 뒤 vendor/라이선스/마지막빈줄 공백진단을 분리하고 course.js의6줄끝공백을 제거. 최초 조기커밋은 active index lock으로 실패하여 새커밋없음. 작성범위공백검사와최종사이트70965완료, releasefa59266a96002a50·로컬HTTP일치. 최종커밋/깨끗한작업폴더 확인 후사용자요청에따라goal paused로중지한다.
