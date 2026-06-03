# scripts/release_win.ps1 — Windows x64 Release 번들 생성.
#
# 사용법 (PowerShell):
#   .\scripts\release_win.ps1                        # 기본: BOT=OFF
#   .\scripts\release_win.ps1 -Bot                   # BOT=ON (ORT 필요)
#   .\scripts\release_win.ps1 -Sdl2                  # SDL2 백엔드
#   .\scripts\release_win.ps1 -RelayEndpoint "relay.example.com:7777" -MetaUrl "https://api.example.com"
#
# 산출물: dist\tetris-win-x64.zip
#   tetris-win-x64\
#     tetris.exe
#     Font\
#     Sounds\
#     assets\          (있으면)
#     model\           (있으면)
#     onnxruntime.dll  (BOT 모드일 때)
param(
    [switch]$Bot,
    [switch]$Sdl2,
    [string]$RelayEndpoint = "",
    [string]$MetaUrl = "",
    [switch]$DebugUi,
    [switch]$NetTrace
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path "$Root\CMakeLists.txt")) {
    $Root = Split-Path -Parent $PSScriptRoot
}
if (-not (Test-Path "$Root\CMakeLists.txt")) {
    Write-Error "Cannot locate repo root (CMakeLists.txt not found)."
    exit 1
}

$BuildDir = "$Root\build-release"
$DistDir  = "$Root\dist\tetris-win-x64"

# ── CMake 구성 ──────────────────────────────────────────────────────────────
$cmakeArgs = @(
    "-B", $BuildDir,
    "-S", $Root,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DTETRIS_BUILD_GAME=ON",
    "-DTETRIS_BUILD_RELAY=OFF",
    "-DTETRIS_BUILD_META=OFF",
    "-DTETRIS_BUILD_TEST=OFF"
)

if ($Sdl2) {
    $cmakeArgs += "-DTETRIS_USE_SDL2=ON"
} else {
    $cmakeArgs += "-DTETRIS_USE_SDL2=OFF"
}

if ($Bot) {
    $cmakeArgs += "-DTETRIS_BUILD_BOT=ON"
} else {
    $cmakeArgs += "-DTETRIS_BUILD_BOT=OFF"
}
if ($RelayEndpoint -ne "") {
    $cmakeArgs += "-DTETRIS_DEFAULT_RELAY_ENDPOINT=$RelayEndpoint"
}
if ($MetaUrl -ne "") {
    $cmakeArgs += "-DTETRIS_DEFAULT_META_URL=$MetaUrl"
}
if ($DebugUi) {
    $cmakeArgs += "-DTETRIS_ENABLE_DEBUG_UI=ON"
}
if ($NetTrace) {
    $cmakeArgs += "-DTETRIS_ENABLE_NET_TRACE=ON"
}

# Visual Studio 가 있으면 자동 감지. 명시적으로 지정하고 싶으면 -G 인자 추가.
Write-Host "[release_win] CMake configure ..."
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed."; exit 1 }

Write-Host "[release_win] CMake build (Release) ..."
cmake --build $BuildDir --config Release -j --target tetris
if ($LASTEXITCODE -ne 0) { Write-Error "CMake build failed."; exit 1 }

# ── 산출물 수집 ──────────────────────────────────────────────────────────────
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

$Rel = "$BuildDir\Release"
Copy-Item "$Rel\tetris.exe"         "$DistDir\"

# Font + Sounds
Copy-Item -Recurse "$Root\Font"   "$DistDir\Font"
Copy-Item -Recurse "$Root\Sounds" "$DistDir\Sounds"

# assets (아이콘 등 — 없으면 무시)
if (Test-Path "$Root\assets") {
    Copy-Item -Recurse "$Root\assets" "$DistDir\assets"
}

# model (ONNX — 없으면 무시)
if (Test-Path "$Root\model") {
    Copy-Item -Recurse "$Root\model" "$DistDir\model"
}

# SDL2.dll (SDL2 빌드 시)
if ($Sdl2) {
    $sdlDll = Get-ChildItem -Recurse "$BuildDir" -Filter "SDL2.dll" | Select-Object -First 1
    if ($sdlDll) { Copy-Item $sdlDll.FullName "$DistDir\" }
}

# onnxruntime.dll (BOT 빌드 시)
if ($Bot) {
    $ortDll = "$Root\third_party\onnxruntime\lib\win-x64\onnxruntime.dll"
    if (Test-Path $ortDll) {
        Copy-Item $ortDll "$DistDir\"
    } else {
        Write-Warning "onnxruntime.dll not found at $ortDll — bundling without it."
    }
}

# ── ZIP 생성 ──────────────────────────────────────────────────────────────
$ZipPath = "$Root\dist\tetris-win-x64.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath }
Compress-Archive -Path $DistDir -DestinationPath $ZipPath
Write-Host "[release_win] Done: $ZipPath"
Write-Host "  Contents:"
Get-ChildItem -Recurse $DistDir | ForEach-Object {
    Write-Host ("    " + $_.FullName.Replace($DistDir, ""))
}
