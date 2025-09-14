# C++와 Raylib로 만든 테트리스

## Windows 빌드 방법 (w64devkit + raylib)

- 준비물
  - raylib 공식 Windows 배포본 설치: `C:/raylib` 폴더에 w64devkit과 raylib 소스가 있다고 가정합니다.
  - `C:/raylib/w64devkit/w64devkit.exe`로 셸을 실행하면 컴파일러 PATH가 자동 설정됩니다.

- 빌드
  - 프로젝트 루트에서 아래 명령을 실행하세요.
    - `mingw32-make PLATFORM=PLATFORM_DESKTOP RAYLIB_PATH=C:/raylib/raylib`
  - 빌드가 성공하면 루트 디렉터리에 `tetris.exe`가 생성됩니다.

- 실행
  - `tetris.exe`를 프로젝트 루트(리소스 경로 유지)에서 실행하세요.
  - 필요 시 `lib/libstdc++-6.dll`, `lib/libgcc_s_dw2-1.dll`을 실행 파일과 같은 폴더에 두거나 PATH에 w64devkit을 추가하세요.

참고: VS Code를 사용할 경우 `.vscode/tasks.json`의 "build release" 또는 F5(런) 구성을 사용하면 자동으로 빌드/실행이 가능합니다.
