# 비공개 파일 게시의 관찰

누적 저장 함수를 그대로 두고 tools/private_publish.cpp와 STORAGE 빌드 역할을 추가한다.
가짜 A/B 레코드를 실제 파일에 쓰며, 완료 확인과 파일 가시성의 차이를 관찰한다.
계정 자격 증명을 입력하거나 로그로 출력하는 도구가 아니다.

## 만들고 검사하기

```sh
cmake -S docs/learn/checkpoints/171-private-publication/roles -B out/study171 -DSTUDY_ROLE=STORAGE -DCMAKE_BUILD_TYPE=Release
cmake --build out/study171 --config Release --target private_publish
python3 scripts/check_learning_private_publication.py --binary out/study171/private_publish
```

Windows의 Visual Studio 생성기는 `out/study171/Release/private_publish.exe`를 전달한다.
Windows는 advapi32를 링크하고 넓은 인자를 UTF-8로 변환한다. MinGW는 -municode를 사용한다.
검사기는 임시 폴더를 직접 소유한다. Linux에서는 단계별 실패 주입도 수행한다.

## 결과를 읽는 순서

- write-old/write-new는 이 실습의 일정 길이 A/B 레코드를 생성한다.
- 종료0은 write-confirmed, 종료2는 write-not-confirmed다. 인자 오류64, 예외3을 구별한다.
- POSIX에서 열린 옛 FD는 교체 뒤에도 옛 파일을, 새 open은 새 파일을 읽는다.
- 일부 쓰기/EINTR 재시도, ENOSPC, 파일 fsync, rename, 부모 fsync의 결과를 비교한다.
- 부모 fsync 실패는 새 파일이 보인 뒤에도 일어나므로 false는 롤백 계약이 아니다.
- 파일별 게시가 여러 파일의 트랜잭션이나 읽기·수정 전체의 직렬화를 제공하지 않는다.

부모 폴더는 앱이 관리하는 신뢰 경로여야 한다. POSIX의 권한/FD 관찰과 Linux 실패
주입을 Windows ACL·파일 공유 모드·전원 손실 검증으로 확대하지 않는다.
저장 함수는 직접 부모만 fsync하며 새 조상 폴더까지 재귀적으로 동기화하지 않는다.
