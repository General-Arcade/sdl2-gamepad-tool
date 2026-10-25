#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
VERSION="$(grep 'project(gamepad-tool VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([^ )]*\).*/\1/')"
APPDIR="$BUILD_DIR/AppDir"

if [ ! -f "$BUILD_DIR/src/gamepad-tool" ]; then
    echo "Error: binary not found. Run build first."
    exit 1
fi

echo "==> Preparing AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/gamepad-tool"

cp "$BUILD_DIR/src/gamepad-tool" "$APPDIR/usr/bin/"
cp "$PROJECT_DIR/src/platform/gamepad-tool.desktop" "$APPDIR/usr/share/applications/"
cp "$PROJECT_DIR/src/resources/images/gamepad-tool.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
cp "$PROJECT_DIR/src/resources/gamecontrollerdb.txt" "$APPDIR/usr/share/gamepad-tool/"

# Top-level desktop and icon for linuxdeploy
cp "$PROJECT_DIR/src/platform/gamepad-tool.desktop" "$APPDIR/"
cp "$PROJECT_DIR/src/resources/images/gamepad-tool.png" "$APPDIR/"

echo "==> Downloading linuxdeploy"
LINUXDEPLOY="$BUILD_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$BUILD_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    wget -q -O "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_QT" ]; then
    wget -q -O "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT"
fi

echo "==> Building AppImage"
export OUTPUT="$PROJECT_DIR/SDL2_Gamepad_Tool-${VERSION}-x86_64.AppImage"
export QMAKE="$(which qmake 2>/dev/null || which qmake-qt5 2>/dev/null || echo qmake)"
export APPIMAGE_EXTRACT_AND_RUN=1

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

echo "==> Done: $OUTPUT"
