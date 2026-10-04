# 변경 이유와 행동 계약

누적 Match·Driver·Character의 경계를 독립적인 C++ 실행기로 검사한다.
표현 변경, 잘못된 입력, 정책 실패, 콜백 외부 효과를 구분한다.

```sh
python3 scripts/check_learning_change_boundaries.py
```

ARCHITECTURE 역할은 순수 C++ 규칙을 사용하며 SDL/DB/ONNX 구현을 요구하지 않는다.
검사는 제한된 계약을 확인한다. 모든 입력의 동등성이나 저장소 전체의 SOLID 준수를 인증하지 않는다.
화면 자료의 namespace를 study_character_art로 분리하고 메뉴·설정·표현 검사 호출자를 함께 갱신한다.
README·roles/CMakeLists와 이 이름 치환 외 누적 파일을 보존하고 tests/architecture_contract.cpp를 추가한다.
