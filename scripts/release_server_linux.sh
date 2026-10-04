#!/usr/bin/env bash
# scripts/release_server_linux.sh — Linux x64 서버 번들 생성.
#
# 사용법:
#   ./scripts/release_server_linux.sh
#
# 의존성: cmake, g++, pthread, OpenSSL 개발 패키지, Boost 헤더(기본 WSS=1).
# 산출물: dist/tetris-server-linux-x64.tar.gz
#   tetris-server-linux-x64/
#     tetris_relay_reactor   (배포 대상 — 이벤트 루프)
#     tetris_meta
#     web/ranking/index.html
#     deploy/
#     scripts/backup_meta_db.sh
#
# 배포는 이벤트 루프 모델(tetris_relay_reactor)을 선택한다. systemd 유닛도
# 같은 실행 파일을 기동하며, 연결당 스레드 모델은 비교용 빌드로 유지한다.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/release_linux_common.sh"
require_linux_x64
ORT_ROOT="${ORT_ROOT:-$ROOT/third_party/onnxruntime}"
case "$ORT_ROOT" in /*) ;; *) ORT_ROOT="$PWD/$ORT_ROOT" ;; esac
BUILD="$ROOT/build-server-release"
BOT="$(release_bool BOT "${BOT:-0}")"
WSS="$(release_bool WSS "${WSS:-1}")"
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
    "-DTETRIS_BUILD_BOT=$BOT"
    "-DTETRIS_ORT_ROOT=$ORT_ROOT"
    "-DTETRIS_BUILD_WSS=$WSS"
    -DTETRIS_BUILD_RELAY=ON
    -DTETRIS_BUILD_REACTOR=ON
    -DTETRIS_BUILD_PY=OFF
    -DTETRIS_BUILD_META=ON
    -DTETRIS_BUILD_TEST=OFF
    -DTETRIS_ENABLE_HTTPS=ON
)

echo "[release_server_linux] CMake configure ..."
cmake "${CMAKE_ARGS[@]}"
echo "[release_server_linux] CMake build ..."
TARGETS=(tetris_relay_reactor tetris_meta)
if [ "$WSS" = "1" ]; then TARGETS+=(tetris_wss_gateway); fi
cmake --build "$BUILD" --config Release -j"$JOBS" --target "${TARGETS[@]}"

rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/scripts"

cp "$BUILD/tetris_relay_reactor" "$BUNDLE/"
cp "$BUILD/tetris_meta"          "$BUNDLE/"
if [ "$WSS" = "1" ]; then cp "$BUILD/tetris_wss_gateway" "$BUNDLE/"; fi

if [ -d "$ROOT/web" ]; then
    cp -R "$ROOT/web" "$BUNDLE/web"
fi
if [ -d "$ROOT/deploy" ]; then
    cp -R "$ROOT/deploy" "$BUNDLE/deploy"
fi
cp "$ROOT/scripts/backup_meta_db.sh" "$BUNDLE/scripts/"
cp "$ROOT/scripts/backup_meta_db.py" "$BUNDLE/scripts/"

# PvE verifier must receive the same character catalog and policies as clients.
cp -R "$ROOT/assets" "$BUNDLE/assets"
if [ -d "$ROOT/model" ]; then cp -R "$ROOT/model" "$BUNDLE/model"; fi
if [ "$BOT" = "1" ]; then
    mkdir -p "$BUNDLE/lib"
    if [ ! -f "$ORT_ROOT/lib/linux-x64/libonnxruntime.so" ]; then
        echo >&2 "[release_server_linux] Required ONNX Runtime missing: $ORT_ROOT/lib/linux-x64/libonnxruntime.so"
        exit 1
    fi
    cp -P "$ORT_ROOT/lib/linux-x64/"libonnxruntime.so* "$BUNDLE/lib/"
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

mkdir -p "$DIST"
TAR="$DIST/tetris-server-linux-x64.tar.gz"
tar -czf "$TAR" -C "$DIST" "tetris-server-linux-x64"

echo "[release_server_linux] Done: $TAR"
echo "  Bundle contents:"
find "$BUNDLE" -maxdepth 3 | sed -n '1,40p'
