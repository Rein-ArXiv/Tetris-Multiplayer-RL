# 118 — JSON 입력 경계

117의 상점·계정·스키마v5를 유지한다. 문서 검증은 json_document.h,
정수/문자열 타입 추출은 json_fields.h, 도메인별 계약은 wire/profile_wire/shop_catalog에 둔다.

```sh
cmake -S docs/learn/checkpoints/118-json-boundary -B out/study-118 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study-118 --target json_boundary_contract study_account_db -j2
ctest --test-dir out/study-118 -R '^json_boundary_contract$' --output-on-failure
python3 scripts/check_learning_json_boundary.py
```

검사는 자체 임시 DB를 사용한다. 운영 DB와 혼용하지 않는다. OpenSSL 개발 패키지가 필요하다.
Windows 다중 구성에서는 --config Release/-C Release를 맞춘다.
독립 복사 시 STUDY_VENDOR_DIR로 외부 의존 경로를 지정한다.
