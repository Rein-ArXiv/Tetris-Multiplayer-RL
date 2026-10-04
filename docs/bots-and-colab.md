# 캐릭터 봇과 Colab 학습, 공용 BP

> 공개 계정 API는 HTTPS를 사용한다. 온라인 게임 입장은 [Part 16의 WSS·일회용 입장권](blog/part16-secure-admission.md), 이 문서의 봇 도전 티켓은 별도 PvE 검증 경로다.

2026-09-19 기준. **학습은 Colab, 게임과 서버는 C++ CPU 추론**으로 나눈다.
로컬에 PyTorch나 학습용 GPU를 설치할 필요가 없다. Linux 주 서버와 Windows 예비
서버에 같은 모델 묶음을 설치하고, 플레이어에게는 OS별 클라이언트를 배포한다.
브라우저 플레이는 아직 구현되지 않았다.

## 지금 가능한 것

- 메뉴의 Single vs Bot에서 여러 캐릭터 중 하나를 골라 1:1 대전한다.
- 상대마다 표시 이름, 작은 사각형 아이콘, 선택/결과 화면 일러스트, 난이도 표시,
  ONNX 모델, 입력 간격, 생각 시간, 최소 블록 배치 시간을 지정한다.
- 같은 모델을 여러 캐릭터가 공유해도 된다. 모델 실력과 움직이는 속도는 별개다.
- 온라인에서 서버가 발급한 경기에서 이기면 **기존 아이콘 상점의 BP**를 받는다.
  서버가 입력을 재현해 확인하며, 클라이언트가 보낸 점수나 승리 선언은 인정하지 않는다.
- 서버를 사용하지 않으면 연습으로 진행한다. 온라인 보상 도전 발급이 실패하면 선택 화면에서 재시도하거나 **무보상 연습을 직접 선택**한다. 경기와 결과 화면에 Practice를 표시하며 BP는 지급하지 않는다. 연습 결과를 나중에 온라인 보상으로 전환하지 않는다.

기본 Lumen/Rook/Vega는 **같은 휴리스틱에 다른 속도와 기존 임시 이미지를 적용한
예시**다. 학습된 모델 세 개나 완성된 캐릭터 일러스트가 포함된 것은 아니다.
여러 적을 동시에 상대하는 다인전, 스토리 진행·해금·대사는 이번 구현에 포함하지 않는다.

## 캐릭터 추가: 가장 자주 수정할 파일

실행 폴더의 `assets/opponents.cfg`를 편집한다. 한 줄은 다음 9개 필드다.

```text
# id|name|model|icon|portrait|difficulty|input ticks|think ticks|min piece ticks
aria|Aria|model/bots/aria_ppo.onnx|assets/icons/aria.png|assets/portraits/aria.png|Normal|6|18|60
```

| 필드 | 의미 |
|---|---|
| id | 영문 소문자/숫자/`_`/`-`, 최대 32자, 중복 금지. 서버 보상에서 상대를 식별 |
| name | 화면에 보이는 이름. 현재 폰트에 없는 글자는 표시되지 않으므로 폰트도 확인 |
| model | `@heuristic` 또는 실행 폴더 기준 `.onnx` 경로 |
| icon | 작은 정사각형 프레임에 비율을 유지해 표시할 이미지 |
| portrait | 선택·결과 화면에 비율을 유지해 표시할 일러스트 |
| difficulty | 표시용 난이도 문구. 이 값 자체가 모델 실력을 바꾸지는 않음 |
| input ticks | 회전/이동/재시도 사이의 틱 간격 |
| think ticks | 새 피스를 관찰한 뒤 첫 계획 전 대기 |
| min piece ticks | 자발적 드롭을 허용하기 전 소비할 최소 틱 수 |

허용 범위와 기본값은 `bot/pacing.h`의 Pacing, 각 상대의 실제 값은 설정 파일에서
확인한다. 규칙 틱 주기가 Δt라면 N틱은 명목상 N×Δt의 게임 시간이다.
최소 소비 시간은 드롭의 허용 조건이므로 실제 배치가 그 시각에 반드시 끝나지는 않는다.

설정은 UTF-8로 저장한다. 선두 BOM과 줄 끝 `#` 주석을 읽을 수 있다. `#`와 `|`는
필드 내용에 사용할 수 없다. 이름 길이 제한은 UTF-8 바이트 수이며 글자 수와 다르다.
Colab 등록 셀은 공통 encoder로 검증한 뒤 기록하고, 패키징도 같은 주석/필드 계약을
사용한다. 잘못된 행은 게임에서 진단 후 건너뛰지만 배포 도구는 패키징을 거절한다.

**중력은 그대로 작동한다.** 최소 배치 시간 전에 자연 낙하로 굳을 수 있다.
자연 낙하로 새 블록이 나오면 이전 블록의 남은 이동 계획을 버린다.
학습 환경의 즉시 배치와 실제 프레임 이동은 다르므로, 학습 점수가 높아도
실제 속도·중력·가비지를 적용한 대전에서 따로 난이도를 확인해야 한다.

설정에 등록하지 않은 `model/*.onnx`, `model/bots/*.onnx`도 기존처럼 탐색한다.
기존 `model/bots.cfg`의 `경로|이름|입력간격[|생각시간|최소배치시간]`은 이 자동 탐색
항목에만 적용한다. 새 캐릭터 설정이 있으면 그것이 우선이다. 보상용 상대는 안정적인
ID로 `assets/opponents.cfg`에 명시 등록해야 한다. 서버는 자동 탐색 상대나 기본 연습
상대를 공식 보상 목록으로 사용하지 않는다. 잘못된 명시 목록은 보상 발급을 막는다.
그림만 바꾸면 엔진 수정이나 재학습은 필요 없다.

## Colab에서 여러 모델 만들기

사용 노트북: `python/train/train_model_zoo_colab.ipynb`.
저장소 변경이 Colab에서 내려받는 브랜치에 반영되어 있어야 한다.

1. GPU 런타임을 선택하고 저장소 주소를 본인 저장소로 맞춘다.
2. `CHARACTER_ID`, `CHARACTER_NAME`, `ALGO`, `RUN_NAME`을 지정한다.
   첫 실행은 `TRAIN_PRESET='smoke'`. 짧은 실행은 설치/학습/내보내기 연결을 확인하는 용도다.
3. 위에서부터 설치 → 시뮬레이터 빌드 → 스모크 → 학습 순으로 실행한다.
   `USE_DRIVE=True`이면 체크포인트·manifest·로그·종료 기록을 `MyDrive/tetris-checkpoints`의 실행별 폴더에 저장한다.
   런타임이 끊기면 마지막 저장 이후의 학습은 사라질 수 있다.
4. 긴 학습은 새 RUN_NAME으로 시작한다. 같은 이름의 실행 폴더가 있으면 실행 셀이 멈춘다. long에는 같은 알고리즘·준비 상태로 성공한 SMOKE_RUN도 필요하다.
   가중치 재사용은 해당 학습기의 `--resume` 범위를 확인해 명시적으로 실행한다. 현재 PPO/정책경사는 새 optimizer·난수·환경을 만드는 warm start다.
   모든 알고리즘이 같은 재개 규약을 갖는 것은 아니다.
5. 내보내기 셀은 `.eval_best.pt`가 있으면 선택하고 없으면 최신 `.pt`를 사용한다.
   MuZero-style은 distill 결과 `.policy.pt`를 선택한다. ONNX 그래프 검사를 통과해야 한다.
6. 캐릭터 등록 셀에서 아이콘/일러스트 경로와 속도를 설정한다. 같은 ID만 갱신하고
   다른 캐릭터는 보존한다. 다른 적을 만들 때 ID와 RUN_NAME을 바꿔 순차 반복한다.
7. 마지막 셀에서 `opponents-날짜.zip`을 받는다. 등록된 ONNX·이미지·설정과 파일별
   SHA-256 manifest가 들어간다. `.pt`, 토큰, 다른 폴더의 파일은 포함하지 않는다.

Drive에는 학습 run 폴더의 체크포인트·실험 기록을 보관한다. 여러 캐릭터의 설정과 내보낸 모델은 다운로드한
ZIP도 보관해야 한다. 새 런타임에서 이전 ZIP을 저장소 루트에 복원한 뒤 다음 캐릭터를
추가하면 앞서 만든 상대를 유지할 수 있다. 실행 폴더에 ZIP 내용을 합치면 된다.

로컬 또는 Colab에서 다시 묶기:

```bash
python3 python/tools/package_opponents.py --out /tmp/opponents-release.zip
```

대상 ZIP이 이미 있으면 덮어쓰지 않는다. 누락 파일, 중복 ID, 잘못된 속도,
실행 폴더를 벗어나는 경로는 오류로 처리한다. 이 도구는 모델의 실력은 평가하지 않는다.
학습 방법의 세부 옵션은 `python/train/README_colab.md`를 참고한다.

## 추론을 포함한 빌드와 배포

기본 휴리스틱은 `TETRIS_BUILD_BOT=OFF`로도 된다. ONNX 상대가 하나라도 있으면
클라이언트와 보상 서버 모두 해당 OS의 ONNX Runtime CPU 라이브러리가 필요하다.
아래 클라이언트 예시는 로컬 meta를 먼저 실행한 상태다. 배포 시 `--meta` 또는
빌드에 포함하는 기본 meta URL을 운영 HTTPS 주소로 설정한다.
학습을 위한 Python 프로세스를 게임이나 서버에서 실행하지 않는다.

Linux 개발 확인:

```bash
bash third_party/fetch_onnxruntime.sh
cmake -S . -B build-bots -DCMAKE_BUILD_TYPE=Release \
  -DTETRIS_BUILD_GAME=ON -DTETRIS_BUILD_META=ON -DTETRIS_BUILD_BOT=ON \
  -DTETRIS_BUILD_PY=OFF -DTETRIS_BUILD_TEST=ON
cmake --build build-bots -j4
ctest --test-dir build-bots --output-on-failure
# 저장소 루트에서 실행: assets/, model/ 상대 경로를 찾는다.
./build-bots/tetris --meta http://127.0.0.1:8080
```

배포 스크립트:

```bash
BOT=1 ./scripts/release_linux.sh
BOT=1 ./scripts/release_server_linux.sh
# macOS 클라이언트는 해당 Mac에서:
BOT=1 ./scripts/release_macos.sh
```

Windows 클라이언트는 `scripts/release_win.ps1 -Bot`을 사용한다.
Windows 예비 서버는 Windows용 ORT의 include와 `lib/win-x64`를 준비한 뒤:

```powershell
cmake -S . -B build-server -DTETRIS_BUILD_GAME=OFF -DTETRIS_BUILD_META=ON `
  -DTETRIS_BUILD_RELAY=ON -DTETRIS_BUILD_REACTOR=ON -DTETRIS_BUILD_BOT=ON `
  -DTETRIS_BUILD_PY=OFF
cmake --build build-server --config Release
```

서버 실행 폴더에 `tetris_meta.exe`, `onnxruntime.dll`, 같은 `assets/opponents.cfg`와
`model/`을 둔다. ONNX는 공통으로 복사하지만 DLL/SO/dylib는 OS별 파일이다.
같은 모델이라도 CPU/ORT 버전에 따라 극히 가까운 출력의 순위가 바뀔 수 있다.
**다른 OS 사이의 보상 검증 호환성은 실제 모델과 장비로 확인한 뒤 배포한다.**
현재 선택은 서버 재현 결과가 최종 기준이며, 불일치 시 보상을 지급하지 않는다.

## 공용 BP 켜기와 검증 규칙

meta의 보상 기능은 기본 OFF다. 설정·모델을 배치한 뒤 기존 실행 인자에
`--bot-rewards`를 추가한다. 인증 설정은 기존 운영 방식 그대로 유지한다.

```bash
# TETRIS_RELAY_SECRET은 기존 환경 파일/비밀 관리 방식으로 먼저 설정한다.
./build-bots/tetris_meta --db /별도/데이터/폴더/tetris.db \
  --http 127.0.0.1:8080 --bot-rewards
```

systemd 사용 시 기존 `ExecStart`에 `--bot-rewards`를 추가한다.
Windows도 같은 인자를 쓴다. 서비스 WorkingDirectory는 모델/설정이 있는 폴더여야 한다.
인터넷에 노출하는 API는 기존 역방향 프록시의 HTTPS를 거친다.

1. 클라이언트가 인증 토큰과 상대 ID로 `/v1/bots/challenge` 요청.
2. 서버가 128비트 난수 티켓, 시드, 공식 상대의 속도를 발급.
3. 클라이언트가 60Hz의 플레이어 입력만 기록하며 같은 시드로 대전.
4. 승리 시 `/v1/bots/claim`으로 티켓과 입력 기록 전송.
5. 서버가 자체 상대 모델·규칙으로 처음부터 재현하여 플레이어만 살아남았는지 확인.
6. SQLite 트랜잭션에서 경기당 1회 지급. 응답 유실 후 같은 티켓을 재전송해도 중복 지급하지 않음.

실제 지급량과 일일 상한은 서버 `meta/database.cpp`의 `saveBotWin` 정책을 따른다.
RP, XP, PvP 승패 통계는 변경하지 않는다. 상한에 도달한 검증 승리는 0 BP 영수증을
남길 수 있으며 UI는 서버가 확인한 이번 지급량을 표시한다.

실행 중 정책 오류나 잘못된 목표가 나오면 클라이언트는 대체 행동으로 계속할 수 있으나
그 경기는 무보상 연습으로 표시한다. 정책 회복으로 기존 경기의 보상 자격을 되살리지
않는다. 서버 재현은 대체 정책을 쓰지 않고 오류를 policy_unavailable로 반환한다.
벽에 막혀 재계획하는 정상 경로 결과나 합법 후보 없음은 정책 장애와 구별한다.

서버는 시작할 때 공식 모델을 준비하고 같은 경로의 캐릭터끼리 세션을 공유한다.
경기 중 파일을 바꾸어도 기존 세션은 재로드하지 않는다. 새 모델 적용은 서버 재시작과
진행 티켓 종료를 포함한 배포 작업으로 수행한다. 클라이언트와 서버에는 같은 모델 묶음이
필요하며, 모델 버전의 자동 협상이나 임의 ONNX의 안전한 실행은 제공하지 않는다.

검증 비용과 재사용을 제한한다:

- 발급 속도 제한과 보관 용량·계정별 진행 티켓·만료 시간은 서버 API/챌린지 정책을 따른다.
- 새 경기 발급은 이전 티켓을 무효화한다.
- 입력 기록 상한은 `bot::kMaxRewardTicks`를 따른다. 넘어가면 해당 경기는 연습으로 전환한다.
- 실제 경과 시간보다 긴 기록은 거절하며, 검증기를 동시에 점유하는 요청 수를 제한한다.
- 재현 루프의 주기적 시간 검사는 단일 추론 호출을 강제로 중단하는 기능이 아니다.
- 잘못된 입력 마스크, 승리 이전/이후로 자른 기록, 패배·무승부는 지급 대상이 아니다.
- 혼잡·통신·저장 확인 실패의 재시도와 정책 오류의 원인 해결을 구별한다.
- 미완료 티켓은 메모리에만 있으므로 서버 재시작/다른 서버 이전 시 사라진다.
  이미 지급한 티켓 영수증과 BP는 DB 백업·이전 시 유지된다.
- 클라이언트 종료 후 미확정 승리를 복구하는 로컬 보관함은 아직 없다.

서버 재현은 **규칙상 가능한 승리**를 확인한다. 사람이 직접 조작했는지 증명하지는
못한다. 자동 플레이, 새 게스트를 계속 만드는 행위, 다중 IP 파밍은 남는다.
100 BP 제한은 실명 사용자당 제한이 아니며, 공개 운영 전 발급/보상량 감시와
보상 경제 조정이 필요하다. PvP도 별도의 [서버 입력 검증](blog/part18-authoritative-results.md)을 사용하지만, 봇의 일일 BP 상한은 PvP에 자동 적용되지 않는다. 전체 출시 차단 항목은 [출시 점검](release-readiness.md)을 따른다.

## 코드 수정 지도

| 바꾸려는 것 | 위치 |
|---|---|
| 캐릭터·모델·이미지·속도 | `assets/opponents.cfg` |
| 설정 형식·기존 모델 자동 탐색 | `bot/opponents.h`, `bot/opponents.cpp` |
| 생각/이동/배치 간격 | `bot/controller.h` |
| 휴리스틱 점수 / 배치→입력 | `bot/placement.cpp` |
| ONNX 이름·float32·고정 shape 검증 및 추론 | `bot/bot_onnx.cpp` |
| 캐릭터 선택/대전/결과 UI·비동기 보상 요청 | `src/main.cpp` |
| 사각형 아이콘·일러스트 비율·애니메이션 | `src/presentation.cpp` |
| 플레이어 입력 기록 재현·가비지 순서 | `bot/reward_replay.h` |
| 티켓·인증·검증 제한 | `meta/bot_challenges.cpp`, `meta/api_server.cpp` |
| 보상 상한·중복 방지 영수증 | `meta/database.cpp`의 `bot_rewards`, `saveBotWin` |
| Colab→ONNX→다운로드 | 노트북, `python/netbot/export_onnx.py`, `python/tools/package_opponents.py` |

현재 배포 정책은 자기 보드·현재 블록·다음 블록만 관측한다. 상대 보드나 남은
가비지를 보는 전술형 캐릭터를 만들려면 관측·모델·학습·추론 계약을 함께 확장해야 한다.
알고리즘이나 학습 시드를 달리했다고 캐릭터별 전술이 자동으로 생기는 것은 아니다.

모델 입력 계약은 board `[1,1,20,10]`, current/next `[1,7]`, 출력은
policy_logits `[1,40]`, value `[1]`, 모두 float32다. 블록 종류/보드 크기 변경은
이 계약과 학습을 함께 변경해야 한다. [외형·규칙 수정 안내](customization.md) 참고.
