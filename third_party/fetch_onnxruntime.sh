#!/usr/bin/env bash
# third_party/fetch_onnxruntime.sh — 공식 ONNX Runtime CPU 릴리스 다운로드.
#
# 사용법:
#   ./third_party/fetch_onnxruntime.sh          # 현재 OS/아키텍처 자동 감지
#   ./third_party/fetch_onnxruntime.sh 1.18.1   # 특정 버전 지정
#
# 완료 후 third_party/onnxruntime/ 에 include/ 과 lib/<platform>/ 이 배치된다.
# CMake -DTETRIS_BUILD_BOT=ON 이 이 구조를 기대한다.
set -euo pipefail

ORT_VERSION="${1:-1.18.1}"
BASE_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}"
DEST="$(cd "$(dirname "$0")" && pwd)/onnxruntime"

detect_platform() {
    local os arch
    os="$(uname -s)"
    arch="$(uname -m)"

    case "$os" in
        Linux)
            case "$arch" in
                x86_64)  echo "linux-x64" ;;
                aarch64) echo "linux-aarch64" ;;
                *)       echo "linux-x64" ;;  # fallback
            esac ;;
        Darwin)
            # universal2 빌드가 arm64 + x86_64 모두 포함.
            echo "osx-universal2" ;;
        MINGW*|MSYS*|CYGWIN*|Windows_NT)
            echo "win-x64" ;;
        *)
            echo >&2 "[fetch_onnxruntime] Unknown OS: $os"; exit 1 ;;
    esac
}

PLATFORM="$(detect_platform)"

# 파일 이름 결정 — 공식 릴리스 네이밍 규칙.
case "$PLATFORM" in
    win-x64)
        ARCHIVE="onnxruntime-win-x64-${ORT_VERSION}.zip"
        EXTRACT="unzip -q" ;;
    osx-universal2)
        ARCHIVE="onnxruntime-osx-universal2-${ORT_VERSION}.tgz"
        EXTRACT="tar xzf" ;;
    linux-x64)
        ARCHIVE="onnxruntime-linux-x64-${ORT_VERSION}.tgz"
        EXTRACT="tar xzf" ;;
    linux-aarch64)
        ARCHIVE="onnxruntime-linux-aarch64-${ORT_VERSION}.tgz"
        EXTRACT="tar xzf" ;;
    *) echo >&2 "Unsupported platform: $PLATFORM"; exit 1 ;;
esac

URL="${BASE_URL}/${ARCHIVE}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "[fetch_onnxruntime] Downloading $URL ..."
if command -v curl &>/dev/null; then
    curl -fSL "$URL" -o "$TMPDIR/$ARCHIVE"
elif command -v wget &>/dev/null; then
    wget -q "$URL" -O "$TMPDIR/$ARCHIVE"
else
    echo >&2 "Neither curl nor wget found."; exit 1
fi

echo "[fetch_onnxruntime] Extracting ..."
cd "$TMPDIR"
$EXTRACT "$ARCHIVE"

# 추출된 폴더 이름 찾기 (아카이브에 따라 이름이 약간 다를 수 있음).
EXTRACTED="$(find . -maxdepth 1 -type d -name 'onnxruntime-*' | head -1)"
if [ -z "$EXTRACTED" ]; then
    echo >&2 "Extraction failed — no onnxruntime-* directory found."; exit 1
fi

# 대상 구조 생성.
mkdir -p "$DEST/include" "$DEST/lib/$PLATFORM"

echo "[fetch_onnxruntime] Installing to $DEST ..."
# include/ — 공통 헤더
cp -f "$EXTRACTED/include/"*.h "$DEST/include/" 2>/dev/null || true

# lib/ — 플랫폼 라이브러리
case "$PLATFORM" in
    win-x64)
        cp -f "$EXTRACTED/lib/"*.lib "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        cp -f "$EXTRACTED/lib/"*.dll "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        ;;
    osx-universal2)
        cp -f "$EXTRACTED/lib/"*.dylib "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        ;;
    linux-*)
        cp -f "$EXTRACTED/lib/"*.so* "$DEST/lib/$PLATFORM/" 2>/dev/null || true
        ;;
esac

echo "[fetch_onnxruntime] Done — $DEST ready for CMake -DTETRIS_BUILD_BOT=ON"
echo "  include/: $(ls "$DEST/include/" | wc -l) header(s)"
echo "  lib/$PLATFORM/: $(ls "$DEST/lib/$PLATFORM/" | wc -l) file(s)"
