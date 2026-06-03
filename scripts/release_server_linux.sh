#!/usr/bin/env bash
# scripts/release_server_linux.sh — Linux x64 서버 번들 생성.
#
# 사용법:
#   ./scripts/release_server_linux.sh
#
# 의존성: cmake, g++, pthread, OpenSSL 개발 패키지(https meta client 사용 시).
# 산출물: dist/tetris-server-linux-x64.tar.gz
#   tetris-server-linux-x64/
#     tetris_relay
#     tetris_meta
#     web/ranking/index.html
#     deploy/
#     scripts/backup_meta_db.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-server-release"
DIST="$ROOT/dist"
BUNDLE="$DIST/tetris-server-linux-x64"

if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
else
    JOBS=2
fi

CMAKE_ARGS=(
    -B "$BUILD"
    -S "$ROOT"
    -DCMAKE_BUILD_TYPE=Release
    -DTETRIS_BUILD_GAME=OFF
    -DTETRIS_BUILD_RELAY=ON
    -DTETRIS_BUILD_META=ON
    -DTETRIS_BUILD_TEST=OFF
    -DTETRIS_ENABLE_HTTPS=ON
)

echo "[release_server_linux] CMake configure ..."
cmake "${CMAKE_ARGS[@]}"
echo "[release_server_linux] CMake build ..."
cmake --build "$BUILD" --config Release -j"$JOBS" --target tetris_relay tetris_meta

rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/scripts"

cp "$BUILD/tetris_relay" "$BUNDLE/"
cp "$BUILD/tetris_meta"  "$BUNDLE/"

if [ -d "$ROOT/web" ]; then
    cp -R "$ROOT/web" "$BUNDLE/web"
fi
if [ -d "$ROOT/deploy" ]; then
    cp -R "$ROOT/deploy" "$BUNDLE/deploy"
fi
cp "$ROOT/scripts/backup_meta_db.sh" "$BUNDLE/scripts/"

mkdir -p "$DIST"
TAR="$DIST/tetris-server-linux-x64.tar.gz"
tar -czf "$TAR" -C "$DIST" "tetris-server-linux-x64"

echo "[release_server_linux] Done: $TAR"
echo "  Bundle contents:"
find "$BUNDLE" -maxdepth 3 | head -40
