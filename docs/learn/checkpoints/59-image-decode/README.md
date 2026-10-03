# PNG/JPEG 파일에서 소유한 RGBA 픽셀로

58-texture-storage의 게임·규칙·Texture·Quad를 유지하고 절차적 아이콘 입력을
assets/player.png 디코딩으로 교체한다. 원본과 같은 8×8 불투명 픽셀을 사용한다.

- renderer/image_decode.h/cpp: 제한된 binary 파일 읽기 → 메타데이터 검사 → RGBA 디코딩 → 소유 vector.
- 파일 8 MiB, 한 변 8192, 출력 64 MiB 정책. 총 작업 메모리/시간 한도와는 다르다.
- 표준 PNG/JPEG 애셋용 stb_image는 한 cpp에 구현한다. 헤더/라이선스/hash는 third_party에 보관.
- source_channels는 원본 성분 수, pixels는 늘 4성분 RGBA8, 위쪽 첫 행, straight alpha.
- 성공한 Image.view는 소유 vector를 빌린다. 동기 upload 이후 CPU Image는 해제된다.

```bash
cmake -S docs/learn/checkpoints/59-image-decode -B out/study59 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study59 -j3
ctest --test-dir out/study59 --output-on-failure
out/study59/decode_demo out/study59/assets/player.png
out/study59/tetris --seed 1 --image out/study59/assets/player.png
python3 scripts/check_learning_decode.py
```

--image PATH는 마지막 두 인자다. 생략 시 assets/player.png는 현재 작업 디렉터리 기준이며
빌드 디렉터리에서 실행하면 복사된 파일을 찾는다. Windows 다중 구성 빌드의 실행 파일은
선택한 구성(예: Release) 아래에 있다. 실행 위치에 맞게 실행 파일/이미지 경로를 지정한다.
GL 없는 계약 검사는 STUDY_PLATFORM=SCRIPTED로 구성한다. 누적 골든은 Python3.10 이상.

기존 메뉴/입력/색상 Stream/텍스처 바인딩 경계는 그대로다. 임의의 투명 PNG를 디코딩할 수
있지만 고정 Quad 파이프라인은 불투명 아이콘을 전제로 하며 이 단계에서 합성 설정을 바꾸지 않는다.
기존 texture_demo와 texture_real은 절차적 픽셀을 사용하는 별도 업로드 진단 도구로 유지한다.
decode_real은 파일 디코딩을 거쳐 같은 기대 픽셀과 비교한다.

검사 자료는 tests/images에 있다. 파일 손상/확장자와 실제 형식/채널 수/알파/한글 경로/
빈 파일과 크기 초과, 픽셀 복사 bad_alloc 시 해제를 확인한다. CPU 입력 버퍼가 사용 중
다른 스레드에서 변경되지 않고 유효한 바이트 범위인 것은 호출자 계약이다.

비표준 CgBI PNG는 기본 stb 옵션에서 BGRA/premultiplied일 수 있어 학습 출력 계약에서 제외한다.
이 로더는 엄격한 PNG 적합성 검증기가 아니므로 일반 PNG로 내보낸 로컬 애셋을 사용한다.
