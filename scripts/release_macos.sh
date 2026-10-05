#!/usr/bin/env bash
# scripts/release_macos.sh — macOS .app 번들 + tar.gz 생성.
#
# 사용법:
#   ./scripts/release_macos.sh
#   BOT=1 ./scripts/release_macos.sh
#   RELAY_ENDPOINT=wss://relay.example.com:8443/play META_URL=https://api.example.com ./scripts/release_macos.sh
#   DEBUG_UI=1 NET_TRACE=1 ./scripts/release_macos.sh
#
# 의존성: cmake, SDL2, OpenSSL, Boost(기본 WSS=1), Xcode command-line tools.
# 기본은 호스트 아키텍처. ARCHS="arm64;x86_64"는 의존 dylib도 universal일 때만.
# 산출물: dist/tetris-macos.tar.gz (내부에 Tetris.app)
set -euo pipefail

if [ "$(uname -s)" != "Darwin" ]; then
    echo >&2 "[release_macos] Run this script on macOS."
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"
APP="$DIST/Tetris.app"
BOT="${BOT:-0}"
WSS="${WSS:-1}"
RELAY_ENDPOINT="${RELAY_ENDPOINT:-127.0.0.1:7777}"
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
    "-DCMAKE_OSX_ARCHITECTURES=${ARCHS:-$(uname -m)}"
)
CMAKE_ARGS+=(
    "-DTETRIS_BUILD_BOT=$BOT"
    "-DTETRIS_BUILD_WSS=$WSS"
    "-DTETRIS_ENABLE_DEBUG_UI=$DEBUG_UI"
    "-DTETRIS_ENABLE_NET_TRACE=$NET_TRACE"
    "-DTETRIS_DEFAULT_RELAY_ENDPOINT=$RELAY_ENDPOINT"
    "-DTETRIS_DEFAULT_META_URL=$META_URL"
    -DTETRIS_BUILD_PY=OFF
    -DTETRIS_ENABLE_HTTPS=ON
)

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
cp -R "$ROOT/Font/."   "$APP/Contents/Resources/Font/"
cp -R "$ROOT/Sounds/." "$APP/Contents/Resources/Sounds/"
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
    chmod u+w "$APP/Contents/Frameworks/$SDL2_NAME"
    install_name_tool -id "@rpath/$SDL2_NAME" "$APP/Contents/Frameworks/$SDL2_NAME"
    install_name_tool -change "$SDL2_DYLIB" \
        "@executable_path/../Frameworks/$SDL2_NAME" \
        "$APP/Contents/MacOS/tetris"
fi

# ── 프레임워크 — ONNX Runtime (BOT 빌드 시) ────────────────────────────────
if [ "$BOT" = "1" ]; then
    ORT_DYLIB="$ROOT/third_party/onnxruntime/lib/osx-universal2/libonnxruntime.dylib"
    if [ -f "$ORT_DYLIB" ]; then
        ORT_REFERENCE="$(otool -L "$APP/Contents/MacOS/tetris" | awk '$1 ~ /\/libonnxruntime[^/]*\.dylib$/ {print $1}')"
        if [ -z "$ORT_REFERENCE" ]; then
            echo >&2 "[release_macos] ONNX Runtime link reference missing."
            exit 1
        fi
        ORT_NAME="$(basename "$ORT_REFERENCE")"
        cp -L "$ORT_DYLIB" "$APP/Contents/Frameworks/$ORT_NAME"
        chmod u+w "$APP/Contents/Frameworks/$ORT_NAME"
        install_name_tool -id "@rpath/$ORT_NAME" "$APP/Contents/Frameworks/$ORT_NAME"
        install_name_tool -change "$ORT_REFERENCE" \
            "@executable_path/../Frameworks/$ORT_NAME" "$APP/Contents/MacOS/tetris"
    else
        echo >&2 "[release_macos] Required ONNX Runtime dylib missing."
        exit 1
    fi
fi

# OpenSSL dylibs found by CMake. Rewrite both the executable and libssl's
# reference to libcrypto so a target machine does not need Homebrew installed.
for component in SSL CRYPTO; do
    library="$(sed -n "s|^OPENSSL_${component}_LIBRARY:FILEPATH=||p" "$BUILD/CMakeCache.txt")"
    case "$library" in
        *.dylib)
            # Use the actual install name (libssl.3.dylib), even when CMake
            # discovered an unversioned libssl.dylib symlink.
            install_id="$(otool -D "$library" | tail -1)"
            name="$(basename "$install_id")"
            cp -L "$library" "$APP/Contents/Frameworks/$name"
            chmod u+w "$APP/Contents/Frameworks/$name"
            install_name_tool -id "@rpath/$name" "$APP/Contents/Frameworks/$name"
            for binary in "$APP/Contents/MacOS/tetris" "$APP/Contents/Frameworks/"*.dylib; do
                while IFS= read -r reference; do
                    install_name_tool -change "$reference" "@rpath/$name" "$binary"
                done < <(otool -L "$binary" | awk -v name="$name" '$1 ~ ("/" name "$") {print $1}')
            done
            ;;
    esac
done

# rpath 추가 (중복 방지)
install_name_tool -add_rpath "@executable_path/../Frameworks" \
    "$APP/Contents/MacOS/tetris" 2>/dev/null || true

# Reject dependencies that would work only on the build host. Inspect every
# bundled dylib as well as the executable so transitive dependencies are checked.
shopt -s nullglob
APP_REAL="$(cd "$APP" && pwd -P)"
BINARIES=("$APP/Contents/Frameworks/"*.dylib "$APP/Contents/MacOS/tetris")
for binary in "${BINARIES[@]}"; do
    # CMake may also retain absolute build-host library directories as rpaths.
    rpaths="$(otool -l "$binary" | awk '/cmd LC_RPATH/ {rpath=1; next} rpath && $1 == "path" {print $2; rpath=0}')"
    while IFS= read -r rpath; do
        case "$rpath" in
            /System/Library/*|/usr/lib/*) ;;
            /*) install_name_tool -delete_rpath "$rpath" "$binary" ;;
        esac
    done <<< "$rpaths"
    dependencies="$(otool -L "$binary" | tail -n +2 | awk '{print $1}')"
    while IFS= read -r reference; do
        case "$reference" in
            /System/Library/*|/usr/lib/*) continue ;;
            @rpath/*) resolved="$APP/Contents/Frameworks/${reference#@rpath/}" ;;
            @executable_path/*) resolved="$APP/Contents/MacOS/${reference#@executable_path/}" ;;
            @loader_path/*) resolved="$(dirname "$binary")/${reference#@loader_path/}" ;;
            *) echo >&2 "[release_macos] Non-bundled dependency: $reference"; exit 1 ;;
        esac
        if [ ! -f "$resolved" ]; then
            echo >&2 "[release_macos] Missing bundled dependency: $reference"
            exit 1
        fi
        resolved_dir="$(cd "$(dirname "$resolved")" && pwd -P)"
        case "$resolved_dir/" in
            "$APP_REAL/"*) ;;
            *) echo >&2 "[release_macos] Dependency escapes bundle: $reference"; exit 1 ;;
        esac
    done <<< "$dependencies"
done

# Mach-O edits invalidate existing signatures. Sign from the inside out; this
# is an ad-hoc development bundle, without an Apple distribution identity.
for binary in "${BINARIES[@]}"; do
    codesign --force --sign - "$binary"
    codesign --verify --strict "$binary"
done
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"

# ── tar.gz 생성 ──────────────────────────────────────────────────────────────
mkdir -p "$DIST"
TAR="$DIST/tetris-macos.tar.gz"
tar -czf "$TAR" -C "$DIST" "Tetris.app"
echo "[release_macos] Done: $TAR"
echo "  .app layout:"
find "$APP" -maxdepth 4 | sed -n '1,30p'
