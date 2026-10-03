# 109 — 메타 서비스와 프로세스 경계

108 누적 규칙과 제출 객체를 보존하고 독립 HTTP 서비스와 호출자를 추가한다.

```sh
cmake -S docs/learn/checkpoints/109-meta-service -B out/study-109 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-109 --target study_meta meta_submit meta_service_contract -j2
ctest --test-dir out/study-109 -R "^meta_service_contract$" --output-on-failure
```

서버 터미널:
```sh
out/study-109/study_meta 18081
```
호출자 터미널:
```sh
out/study-109/meta_submit 18081 17
out/study-109/meta_submit 18081 17
out/study-109/meta_submit 18081 18
```

같은 키/내용은 같은 행 번호다. 호출자 종료는 서비스 메모리를 지우지 않는다.
서비스 재시작은 메모리 행을 지운다. 현재 실제 서비스의 SQLite 영속성과 구별한다.
서비스는127.0.0.1에만 bind한다. 인증·보상 지급 API가 아니며 공개 운영용이 아니다.
포트0은 OS 자동 할당, stdout PORT 뒤 healthz로 준비를 확인한다.
새 타깃은 창을 열지 않는다. SDL은 별도 빌드 폴더를 쓴다.
다른 폴더로 분리했다면 STUDY_VENDOR_DIR에 저장소 third_party를 지정한다.
Windows 여러 구성 생성기에서는 실행 파일의 구성 하위 폴더를 확인한다.
