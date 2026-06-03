#!/usr/bin/env bash
# scripts/release_macos.sh — macOS .app 번들 + tar.gz 생성.
#
# 사용법:
#   ./scripts/release_macos.sh
#   BOT=1 ./scripts/release_macos.sh
#   RELAY_ENDPOINT=relay.example.com:7777 META_URL=https://api.example.com ./scripts/release_macos.sh
#   DEBUG_UI=1 NET_TRACE=1 ./scripts/release_macos.sh
#
# 의존성: cmake, SDL2 (brew install sdl2), Xcode command-line tools.
# 산출물: dist/tetris-macos.tar.gz (내부에 Tetris.app)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"
APP="$DIST/Tetris.app"
BOT="${BOT:-0}"
RELAY_ENDPOINT="${RELAY_ENDPOINT:-}"
META_URL="${META_URL:-}"
DEBUG_UI="${DEBUG_UI:-0}"
NET_TRACE="${NET_TRACE:-0}"

# ── CMake 구성 ──────────────────────────────────────────────────────────────
CMAKE_ARGS=(
    -B "$BUILD"
    -S "$ROOT"
    -DCMAKE_BUILD_TYPE=Release
    -DTETRIS_BUILD_GAME=ON
    -DTETRIS_BUILD_RELAY=OFF
    -DTETRIS_BUILD_META=OFF
    -DTETRIS_BUILD_TEST=OFF
    -DTETRIS_USE_SDL2=ON
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
)
if [ "$BOT" = "1" ]; then
    CMAKE_ARGS+=(-DTETRIS_BUILD_BOT=ON)
fi
if [ -n "$RELAY_ENDPOINT" ]; then
    CMAKE_ARGS+=("-DTETRIS_DEFAULT_RELAY_ENDPOINT=$RELAY_ENDPOINT")
fi
if [ -n "$META_URL" ]; then
    CMAKE_ARGS+=("-DTETRIS_DEFAULT_META_URL=$META_URL")
fi
if [ "$DEBUG_UI" = "1" ]; then
    CMAKE_ARGS+=(-DTETRIS_ENABLE_DEBUG_UI=ON)
fi
if [ "$NET_TRACE" = "1" ]; then
    CMAKE_ARGS+=(-DTETRIS_ENABLE_NET_TRACE=ON)
fi

echo "[release_macos] CMake configure ..."
cmake "${CMAKE_ARGS[@]}"
echo "[release_macos] CMake build ..."
cmake --build "$BUILD" --config Release -j"$(sysctl -n hw.ncpu)" --target tetris

# ── .app 번들 구성 ──────────────────────────────────────────────────────────
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
mkdir -p "$APP/Contents/Frameworks"
mkdir -p "$APP/Contents/Resources/Font"
mkdir -p "$APP/Contents/Resources/Sounds"

# Info.plist (CMake @변수@ 는 단순 sed 치환)
VERSION="1.0.0"
sed "s/@PROJECT_VERSION@/$VERSION/g" \
    "$ROOT/platform/macos/Info.plist.in" \
    > "$APP/Contents/Info.plist"

# 실행 파일
cp "$BUILD/tetris"          "$APP/Contents/MacOS/"

# 에셋
cp -R "$ROOT/Font/"   "$APP/Contents/Resources/Font/"
cp -R "$ROOT/Sounds/" "$APP/Contents/Resources/Sounds/"
if [ -d "$ROOT/assets" ]; then
    cp -R "$ROOT/assets" "$APP/Contents/Resources/assets"
fi
if [ -d "$ROOT/model" ]; then
    cp -R "$ROOT/model" "$APP/Contents/Resources/model"
fi

# ── 프레임워크 — SDL2 ──────────────────────────────────────────────────────
# Homebrew SDL2 dylib 위치 — arm64 / x86_64 에 따라 달라질 수 있음.
SDL2_DYLIB="$(otool -L "$APP/Contents/MacOS/tetris" \
    | grep -o '/.*libSDL2[^ ]*\.dylib' | head -1 || true)"
if [ -n "$SDL2_DYLIB" ] && [ -f "$SDL2_DYLIB" ]; then
    cp "$SDL2_DYLIB" "$APP/Contents/Frameworks/"
    SDL2_NAME="$(basename "$SDL2_DYLIB")"
    install_name_tool -change "$SDL2_DYLIB" \
        "@executable_path/../Frameworks/$SDL2_NAME" \
        "$APP/Contents/MacOS/tetris"
fi

# ── 프레임워크 — ONNX Runtime (BOT 빌드 시) ────────────────────────────────
if [ "$BOT" = "1" ]; then
    ORT_DYLIB="$ROOT/third_party/onnxruntime/lib/osx-universal2/libonnxruntime.dylib"
    if [ -f "$ORT_DYLIB" ]; then
        cp "$ORT_DYLIB" "$APP/Contents/Frameworks/"
        install_name_tool -change "@rpath/libonnxruntime.dylib" \
            "@executable_path/../Frameworks/libonnxruntime.dylib" \
            "$APP/Contents/MacOS/tetris" 2>/dev/null || true
    else
        echo "[release_macos] WARNING: $ORT_DYLIB not found — bundling without ORT."
    fi
fi

# rpath 추가 (중복 방지)
install_name_tool -add_rpath "@executable_path/../Frameworks" \
    "$APP/Contents/MacOS/tetris" 2>/dev/null || true

# ── tar.gz 생성 ──────────────────────────────────────────────────────────────
mkdir -p "$DIST"
TAR="$DIST/tetris-macos.tar.gz"
tar -czf "$TAR" -C "$DIST" "Tetris.app"
echo "[release_macos] Done: $TAR"
echo "  .app layout:"
find "$APP" -maxdepth 4 | head -30
