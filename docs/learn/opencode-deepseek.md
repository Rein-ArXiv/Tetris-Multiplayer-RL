# OpenCode 2의 DeepSeek 연결

이 기기에서 확인한 실행 파일은 `/home/rein/.opencode/bin/opencode`, 버전은 2.0.11이다.
PATH에 없으면 아래처럼 전체 경로로 실행한다.

```bash
/home/rein/.opencode/bin/opencode
```

TUI에서 다음 순서로 진행한다.

1. `/connect`를 실행하고 DeepSeek 공급자를 검색한다.
2. DeepSeek에서 발급한 API 키를 OpenCode의 입력란에 직접 입력한다.
3. 연결 뒤 `/models`에서 DeepSeek 모델을 고른다. 모델 이름과 ID는 표시된 목록을 기준으로 한다.

`/models`는 현재 프로젝트에서 활성화된 공급자의 사용 가능한 모델을 보여 준다.
키 연결 전에는 DeepSeek가 안 보일 수 있다. `/models`와 공급자를 연결하는 `/connect`를
구분한다. `/connect`에서도 공급자가 없다면 프로젝트·전역 설정의 공급자 비활성화와
카탈로그 로딩을 확인한다. OpenCode 1의 `provider` 설정 예제를 2의 `providers` 설정에
그대로 붙이지 않는다.

터미널 연결 명령은 다음과 같다. 키를 명령행 인자로 붙이지 않고 대화형 입력을 사용한다.

```bash
/home/rein/.opencode/bin/opencode auth login deepseek
```

일시적으로 셸 환경변수로 연결한 키를 사용하는 경우, 이미 실행 중인 공유 서버는
나중에 바꾼 셸 환경을 상속하지 않을 수 있다. 이때 `--standalone`으로 새 프로세스에서
확인한다. 키 문자열을 출력하는 명령이나 전체 설정 덤프를 대화에 붙이지 않는다.

공급자 연결과 모델 목록 조회에는 생성 요청이 필요하지 않다. 키를 연결한 뒤에도
잔액·API 접근·모델 권한은 실제 요청에서 별도로 실패할 수 있다.

## 초안 작업에 사용할 명령

2026-09-21 이 환경에서 `deepseek/deepseek-flash`의 실제 생성 요청과 첨부 자료를
이용한 두 차시 초안 작성을 확인했다. 모델 ID는 환경에 따라 바뀔 수 있으므로
다른 설치에서는 `/models`에 표시되는 값을 확인한다.

```bash
/home/rein/.opencode/bin/opencode run --standalone \
  --model deepseek/deepseek-flash --agent plan --format json \
  --file out/learning-jobs/lesson-material.txt \
  '첨부한 한 차시 재료만 사용해 용어와 설명 순서를 분류하라. 파일 변경이나 명령 실행 없이 초안만 반환하라.'
```

원문 전체를 매 요청에 넣지 않는다. 먼저 로컬 스크립트로 필요한 범위를 추출하고,
결과를 검토한 뒤 강의에 반영한다. 이 문서의 예시는 자동 실행되지 않는다.
`--format json`은 OpenCode의 이벤트 형식이며 모델 본문이 올바른 JSON이라는
보장은 아니다. 이벤트의 텍스트를 분리한 뒤 별도로 형식과 내용을 검사한다.

근거: [OpenCode 공급자 연결](https://opencode.ai/v2/docs/providers),
[모델 선택](https://opencode.ai/v2/docs/models),
[CLI 자동화와 공유 서버](https://opencode.ai/v2/docs/cli),
[DeepSeek의 OpenCode 연결 안내](https://api-docs.deepseek.com/guides/coding_agents/).
