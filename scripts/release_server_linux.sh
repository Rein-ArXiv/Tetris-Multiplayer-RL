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
# 연결당 스레드 모델(tetris_relay)은 번들에 넣지 않는다. 교재용 참조 구현이고
# 빈 방 256개로 서버가 멎는 알려진 결함이 있어 배포 대상이 아니다 — systemd 유닛도
# tetris_relay_reactor 를 기동한다.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-server-release"
BOT="${BOT:-0}"
WSS="${WSS:-1}"
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
    cp "$ROOT/third_party/onnxruntime/lib/linux-x64/"libonnxruntime.so* "$BUNDLE/lib/"
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
find "$BUNDLE" -maxdepth 3 | head -40
