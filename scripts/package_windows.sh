#!/bin/sh
# Windows packaging script — produces a distributable ZIP archive
# Run from Git Bash after running build_windows.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
SRC_DIR="$BUILD_DIR/src"
VERSION="$(grep 'project(gamepad-tool VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([^ ]*\).*/\1/')"
DIST_NAME="SDL2-Gamepad-Tool-${VERSION}-windows-x64"
DIST_DIR="$BUILD_DIR/$DIST_NAME"
ZIP_PATH="$PROJECT_DIR/${DIST_NAME}.zip"
SEVENZIP="${SEVENZIP:-/c/Program Files/7-Zip/7z.exe}"

if [ ! -f "$SRC_DIR/gamepad-tool.exe" ]; then
    echo "Error: $SRC_DIR/gamepad-tool.exe not found. Run build_windows.sh first."
    exit 1
fi

if [ ! -f "$SRC_DIR/Qt6Core.dll" ]; then
    echo "Error: Qt DLLs not found. Run windeployqt first, or run this script after build_windows.sh."
    exit 1
fi

echo "==> Assembling distribution folder: $DIST_DIR"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# Copy everything windeployqt placed in src/, excluding build artifacts
cp "$SRC_DIR/gamepad-tool.exe"     "$DIST_DIR/"
cp "$SRC_DIR"/*.dll                "$DIST_DIR/"
cp "$SRC_DIR/gamecontrollerdb.txt" "$DIST_DIR/" 2>/dev/null || true

for dir in generic iconengines imageformats networkinformation platforms styles tls translations; do
    if [ -d "$SRC_DIR/$dir" ]; then
        cp -r "$SRC_DIR/$dir" "$DIST_DIR/"
    fi
done

echo "==> Creating ZIP: $ZIP_PATH"
rm -f "$ZIP_PATH"
"$SEVENZIP" a -tzip "$ZIP_PATH" "$DIST_DIR" > /dev/null

echo "==> Done: $ZIP_PATH"
echo "    Contents: $(ls "$DIST_DIR" | wc -l) items"
