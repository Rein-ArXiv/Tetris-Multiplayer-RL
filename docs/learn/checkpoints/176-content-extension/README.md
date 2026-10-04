# 새 연습 상대와 콘텐츠 묶음

assets/opponents.cfg에 mira를 등록하고 소유하는 카드 자료로 투영한다.
content_demo는 ID로 선택한 프로필의 pacing과 로컬 휴리스틱으로 practice 경기를 진행한다.
기존 그림을 재사용하며 콘솔 출력으로 연결을 관찰한다. GPU 창을 새로 만드는 실행기는 아니다.

```sh
python3 scripts/check_learning_content_extension.py
```

직접 실행할 때 CONTENT 역할의 content_demo에 config-path stable-id tick-budget을 전달한다.
Windows는 wmain에서 UTF-8로 인자를 변환하고 MinGW는 -municode를 사용한다.
모델 선택은 이 실행기가 지원하는 @heuristic만 허용하며 다른 모델의 대체 실행을 숨기지 않는다.
이 휴리스틱은 체크포인트용 평가식이다. 제품 정책/서버 재현과 동일하다고 가정하지 않는다.

```sh
python3 docs/learn/checkpoints/176-content-extension/operations/package_content.py --out /tmp/study-opponents-new.zip
```

패키징 중 원본을 고정하고 새 출력 경로를 사용한다. 하드 링크가 지원되는 저장소가 필요하다.
설정과 참조 파일의 해시는 실제 포함 바이트를 식별하며 제작자 인증이나 모델 실력 검사가 아니다.
로컬 등록은 서버의 공용 BP 지급 권한을 만들지 않는다.
README와 roles/CMakeLists 외 누적 파일을 보존한다.
