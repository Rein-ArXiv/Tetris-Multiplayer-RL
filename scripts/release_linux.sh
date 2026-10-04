#!/usr/bin/env bash
# scripts/release_linux.sh — Linux x64 tar.gz 번들 생성.
#
# 사용법:
#   ./scripts/release_linux.sh
#   BOT=1 ./scripts/release_linux.sh
#   RELAY_ENDPOINT=wss://relay.example.com:8443/play META_URL=https://api.example.com ./scripts/release_linux.sh
#   DEBUG_UI=1 NET_TRACE=1 ./scripts/release_linux.sh
#
# 의존성: cmake, g++, libsdl2-dev, libssl-dev, libboost-dev (기본 WSS=1).
#   OpenGL 3.3 렌더러: libgl1-mesa-dev도 필요하다.
# 산출물: dist/tetris-linux-x64.tar.gz
#   tetris-linux-x64/
#     tetris
#     lib/             ← SDL2 + ORT shared libs (rpath=$ORIGIN/lib)
#     Font/
#     Sounds/
#     assets/          (있으면)
#     model/           (있으면)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/release_linux_common.sh"
require_linux_x64
ORT_ROOT="${ORT_ROOT:-$ROOT/third_party/onnxruntime}"
case "$ORT_ROOT" in /*) ;; *) ORT_ROOT="$PWD/$ORT_ROOT" ;; esac
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"
BUNDLE="$DIST/tetris-linux-x64"
BOT="$(release_bool BOT "${BOT:-0}")"
WSS="$(release_bool WSS "${WSS:-1}")"
RELAY_ENDPOINT="${RELAY_ENDPOINT:-127.0.0.1:7777}"
META_URL="${META_URL:-}"
DEBUG_UI="$(release_bool DEBUG_UI "${DEBUG_UI:-0}")"
NET_TRACE="$(release_bool NET_TRACE "${NET_TRACE:-0}")"

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
)
CMAKE_ARGS+=(
    "-DTETRIS_BUILD_BOT=$BOT"
    "-DTETRIS_ORT_ROOT=$ORT_ROOT"
    "-DTETRIS_BUILD_WSS=$WSS"
    "-DTETRIS_ENABLE_DEBUG_UI=$DEBUG_UI"
    "-DTETRIS_ENABLE_NET_TRACE=$NET_TRACE"
    "-DTETRIS_DEFAULT_RELAY_ENDPOINT=$RELAY_ENDPOINT"
    "-DTETRIS_DEFAULT_META_URL=$META_URL"
    -DTETRIS_BUILD_PY=OFF
    -DTETRIS_ENABLE_HTTPS=ON
)

echo "[release_linux] CMake configure ..."
cmake "${CMAKE_ARGS[@]}"
echo "[release_linux] CMake build ..."
cmake --build "$BUILD" --config Release -j"$(nproc)" --target tetris

# ── 번들 구성 ────────────────────────────────────────────────────────────────
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/lib"

# 실행 파일
cp "$BUILD/tetris"        "$BUNDLE/"

# 에셋
cp -R "$ROOT/Font"   "$BUNDLE/Font"
cp -R "$ROOT/Sounds" "$BUNDLE/Sounds"
if [ -d "$ROOT/assets" ]; then
    cp -R "$ROOT/assets" "$BUNDLE/assets"
fi
if [ -d "$ROOT/model" ]; then
    cp -R "$ROOT/model" "$BUNDLE/model"
fi

# ── 공유 라이브러리 번들 ────────────────────────────────────────────────────
# SDL2
SDL2_SO="$(ldd "$BUNDLE/tetris" | grep -o '/.*libSDL2[^ ]*' | head -1 || true)"
if [ -n "$SDL2_SO" ] && [ -f "$SDL2_SO" ]; then
    cp "$SDL2_SO" "$BUNDLE/lib/"
fi

# ONNX Runtime (BOT 빌드 시)
if [ "$BOT" = "1" ]; then
    ORT_DIR="$ORT_ROOT/lib/linux-x64"
    if [ ! -f "$ORT_DIR/libonnxruntime.so" ]; then
        echo >&2 "[release_linux] Required ONNX Runtime missing: $ORT_DIR/libonnxruntime.so"
        exit 1
    fi
    cp -P "$ORT_DIR"/libonnxruntime.so* "$BUNDLE/lib/"
fi

# ── rpath 패치 (빌드 CMake 에서도 설정하지만 안전 장치) ────────────────────
if command -v patchelf &>/dev/null; then
    patchelf --set-rpath '$ORIGIN/lib' "$BUNDLE/tetris"
fi

# TLS shared libraries are runtime dependencies, including the crypto dependency
# of libssl. Keep the SONAME filenames from ldd, never bundle the system libc.
mkdir -p "$BUNDLE/lib"
for executable in "$BUNDLE"/tetris*; do
    [ -f "$executable" ] || continue
    while IFS= read -r library; do
        [ -f "$library" ] && cp -L "$library" "$BUNDLE/lib/"
    done < <(ldd "$executable" | awk '/lib(ssl|crypto)\.so/ && $2 == "=>" { print $3 }')
done

# ── tar.gz 생성 ──────────────────────────────────────────────────────────────
mkdir -p "$DIST"
TAR="$DIST/tetris-linux-x64.tar.gz"
tar -czf "$TAR" -C "$DIST" "tetris-linux-x64"
echo "[release_linux] Done: $TAR"
echo "  Bundle contents:"
find "$BUNDLE" -maxdepth 2 | sed -n '1,25p'
