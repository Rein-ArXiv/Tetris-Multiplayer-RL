# Tetris Multiplayer with Lockstep Networking

C++와 Raylib로 구현한 P2P Lockstep 네트워킹 기반 멀티플레이어 테트리스.

## 빠른 시작

### Windows (w64devkit + raylib)
```bash
mingw32-make PLATFORM=PLATFORM_DESKTOP RAYLIB_PATH=C:/raylib/raylib
```

### Linux/Mac (CMake)
```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j
./tetris
```

## 멀티플레이어 실행

```bash
# 호스트
./tetris --host 7777

# 클라이언트
./tetris --connect 192.168.1.100:7777
```

## 핵심 기능

- ✅ 결정론적 P2P Lockstep 동기화
- ✅ 고정 60Hz 틱 시스템
- ✅ 플랫폼 독립적 네트워킹 (Windows/Linux)
- ✅ 리플레이 시스템 (F5: 기록, F6: 저장)
- ✅ 상태 해시 검증 (H: 해시 출력)
- ✅ 멀티스레드 I/O

## 핫키

- **화살표**: 이동/회전
- **Space**: 하드 드롭
- **F5/F6**: 리플레이 기록/저장
- **H**: 상태 해시 출력
- **R** (게임 오버): 재시작
- **ESC** (게임 오버): 타이틀

## 문서

**완전 가이드**: [`DOCUMENTATION.md`](DOCUMENTATION.md) - 모든 기술 문서 통합본

**개발 지침**: [`CLAUDE.md`](CLAUDE.md) - Claude Code용 프로젝트 가이드

## 프로젝트 구조

```
src/     게임 로직, UI (raylib)
core/    결정론 시스템 (틱, RNG, 해시, 리플레이)
net/     네트워킹 계층 (Socket → Framing → Session)
```

## 요구사항

- **Windows**: raylib (C:/raylib), w64devkit
- **Linux**: raylib (pkg-config 또는 /usr/local)
- **공통**: C++17
