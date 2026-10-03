# 73 · 설정 값과 파일 상태

## 분리

config.h는 문자열↔소유하는 Config를 다룬다. 후보가 완성돼야 결과에 게시한다.
store.h는 bounded read·파일 상태·writer 연결을, session.h는 Preferences와 저장 허용을
담당한다. main은 세션을 만들고 실제 Action 변경의 결과를 보고한다.
Preferences의 set_decorations는 로드 값을 명시적으로 적용하며 일반 위젯 계약은 유지한다.

## 파싱과 상태

version=1 필수, decorations/character 누락은 기본값이다. duplicate·unknown·invalid·
unsupported는 전체 문서 실패이며 부분 값은 적용하지 않는다. ASCII space/tab/CR trim,
물리적 줄256/파일4096바이트를 검사하고 NUL을 거절한다. string_view는 파싱 중만 빌리고
캐릭터 ID는 Config가 소유한다. Preferences는 알려진 ID의 정적 카탈로그 view를 저장한다.
Loaded의 error/line과 파일 상태는 기본값과 별개다. loaded/missing만 writable이다.

## 쓰기와 실패

save는 직렬화한 뒤 private_file writer를 호출한다. 이 함수 자체는 기존파일 parse/savegate를
검사하지 않으며 Session이 initial_load로 보호한다. invalid/io_error는 실행 중 변경만 허용.
실패해도 UI 값은 유지하며 다음 실제 변경이 저장을 다시 시도한다. 매 프레임 재시도는 없다.
보호 파일의 외부 수정은 재시작 후 반영한다. 실행 중 외부/다중프로세스 변경 감지는 없다.

writer는 같은 디렉터리에 배타적 임시파일을 만들고 쓰기/파일동기화/닫기/교체를 수행한다.
POSIX는 교체 후 디렉터리도 동기화하므로 실패 반환과 old/new 가시성을 구별해야 한다.
Windows는 owner/SYSTEM ACL·FlushFileBuffers·MoveFileEx 경로다. 네임스페이스 외 구현은
루트 writer와 같음을 검사한다. 저장 경로는 신뢰하는 로컬 사용자 디렉터리라는 전제다.
검사→열기 사이 경쟁이나 적대적인 디렉터리 변경까지 막는 sandbox가 아니다.

## 현재 게임 수정과 차이

루트 parse_int_clamped의 접미사·ERANGE·NUL뒤데이터, malformed legacy bool이37을100으로
바꾸는 오류, fgets 조각이 긴물리줄 뒤쪽을 새키로 읽는 오류를 수정했다.
로드는16KiB/물리줄255한계이며 잘못된줄은무시·현재유효값유지하는 관대한정책이다.
루트save_settings는직렬화후기존private_file writer를사용한다. 설정변경즉시UI적용,
저장오류stderr 보고이며 화면toast/학습Session의손상파일보호게이트는 루트에추가하지않았다.

## 검증

전체후보거절·한계/CRLF/EOF/NUL·형식정규화·네Config roundtrip·실제파일생성/교체/재시작·
보호상태/수동백업복구·저장실패후UI값유지를 검사한다. 루트수정전접미사/legacy/긴줄오류와
RLIMIT_FSIZE 쓰기실패의파일비우기를재현하고 수정후기존파일보존을확인한다.
fsync(file)/rename/fsync(directory)를실패시켜교체전old/교체후new와임시파일정리를확인한다.
네이티브다른OS/전원차단/다중프로세스경쟁의완전성검증은아니다.
