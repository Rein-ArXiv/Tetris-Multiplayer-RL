#!/usr/bin/env bash
# scripts/release_linux.sh — Linux x64 tar.gz 번들 생성.
#
# 사용법:
#   ./scripts/release_linux.sh            # 기본: BOT=OFF
#   BOT=1 ./scripts/release_linux.sh      # BOT=ON (ORT 필요)
#
# 의존성: cmake, g++, libsdl2-dev, libgl1-mesa-dev.
# 산출물: dist/tetris-linux-x64.tar.gz
#   tetris-linux-x64/
#     tetris
#     tetris_relay
#     sim_hash_dump
#     lib/             ← SDL2 + ORT shared libs (rpath=$ORIGIN/lib)
#     Font/
#     Sounds/
#     assets/          (있으면)
#     model/           (있으면)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"
BUNDLE="$DIST/tetris-linux-x64"
BOT="${BOT:-0}"

# ── CMake 구성 ──────────────────────────────────────────────────────────────
CMAKE_ARGS=(
    -B "$BUILD"
    -S "$ROOT"
    -DCMAKE_BUILD_TYPE=Release
    -DTETRIS_BUILD_GAME=ON
    -DTETRIS_BUILD_RELAY=ON
    -DTETRIS_BUILD_TEST=ON
    -DTETRIS_USE_SDL2=ON
)
if [ "$BOT" = "1" ]; then
    CMAKE_ARGS+=(-DTETRIS_BUILD_BOT=ON)
fi

echo "[release_linux] CMake configure ..."
cmake "${CMAKE_ARGS[@]}"
echo "[release_linux] CMake build ..."
cmake --build "$BUILD" --config Release -j"$(nproc)"

# ── 번들 구성 ────────────────────────────────────────────────────────────────
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/lib"

# 실행 파일
cp "$BUILD/tetris"        "$BUNDLE/"
cp "$BUILD/tetris_relay"  "$BUNDLE/" 2>/dev/null || true
cp "$BUILD/sim_hash_dump" "$BUNDLE/" 2>/dev/null || true

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
    ORT_DIR="$ROOT/third_party/onnxruntime/lib/linux-x64"
    if [ -d "$ORT_DIR" ]; then
        cp "$ORT_DIR"/libonnxruntime.so* "$BUNDLE/lib/" 2>/dev/null || true
    else
        echo "[release_linux] WARNING: $ORT_DIR not found — bundling without ORT."
    fi
fi

# ── rpath 패치 (빌드 CMake 에서도 설정하지만 안전 장치) ────────────────────
if command -v patchelf &>/dev/null; then
    patchelf --set-rpath '$ORIGIN/lib' "$BUNDLE/tetris" 2>/dev/null || true
fi

# ── tar.gz 생성 ──────────────────────────────────────────────────────────────
mkdir -p "$DIST"
TAR="$DIST/tetris-linux-x64.tar.gz"
tar -czf "$TAR" -C "$DIST" "tetris-linux-x64"
echo "[release_linux] Done: $TAR"
echo "  Bundle contents:"
find "$BUNDLE" -maxdepth 2 | head -25
