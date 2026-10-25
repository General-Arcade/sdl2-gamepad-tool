#!/bin/sh
# Windows build script — run from Git Bash or MSYS2
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

# Auto-detect Qt MinGW installation
QT_BASE="${QT_BASE:-C:/Qt}"
QT_VERSION="${QT_VERSION:-$(ls "$QT_BASE" 2>/dev/null | grep -E '^[0-9]+\.' | sort -V | tail -1)}"
MINGW_KIT="${MINGW_KIT:-$(ls "$QT_BASE/$QT_VERSION" 2>/dev/null | grep mingw | head -1)}"
QT_PREFIX="$QT_BASE/$QT_VERSION/$MINGW_KIT"
MINGW_DIR="${MINGW_DIR:-$(ls -d "$QT_BASE/Tools"/mingw* 2>/dev/null | sort -V | tail -1)/bin}"

if [ ! -d "$QT_PREFIX" ]; then
    echo "ERROR: Qt not found at $QT_PREFIX"
    echo "Set QT_BASE, QT_VERSION, MINGW_KIT, and MINGW_DIR to override detection."
    exit 1
fi

export PATH="$(cygpath -u "$MINGW_DIR"):$PATH"

echo "==> Using Qt $QT_VERSION at $QT_PREFIX"
echo "==> Using MinGW at $MINGW_DIR"

echo "==> Configuring"
mkdir -p "$BUILD_DIR"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -G "MinGW Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
    -DCMAKE_C_COMPILER="$MINGW_DIR/gcc.exe" \
    -DCMAKE_CXX_COMPILER="$MINGW_DIR/g++.exe" \
    -DCMAKE_MAKE_PROGRAM="$MINGW_DIR/mingw32-make.exe" \
    -DCMAKE_RC_COMPILER="$MINGW_DIR/windres.exe"

echo "==> Building"
"$MINGW_DIR/mingw32-make.exe" -C "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

# SDL2 built via FetchContent lands under _deps; put it next to the executable
SDL2_DIR="$BUILD_DIR/_deps/sdl2-build"
if [ -f "$SDL2_DIR/SDL2.dll" ]; then
    cp "$SDL2_DIR/SDL2.dll" "$BUILD_DIR/src/"
fi

echo "==> Running tests"
# Test executables load the Qt and SDL2 DLLs from PATH
PATH="$(cygpath -u "$QT_PREFIX")/bin:$SDL2_DIR:$PATH" \
    ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "==> Bundling Qt runtime"
"$QT_PREFIX/bin/windeployqt.exe" --release --no-translations --no-opengl-sw --no-system-d3d-compiler "$BUILD_DIR/src/gamepad-tool.exe"

echo "==> Done"
echo "Binary: $BUILD_DIR/src/gamepad-tool.exe"
