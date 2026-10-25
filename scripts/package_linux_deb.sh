#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

if [ ! -f "$BUILD_DIR/src/gamepad-tool" ]; then
    echo "Error: binary not found. Run build first."
    exit 1
fi

echo "==> Reconfiguring CMake with install prefix"
cd "$BUILD_DIR"
cmake .. -DCMAKE_INSTALL_PREFIX=/usr

echo "==> Building .deb package with CPack"
cpack -G DEB

# Move .deb to project root
DEB_FILE="$(ls "$BUILD_DIR"/gamepad-tool-*.deb 2>/dev/null | head -1)"
if [ -n "$DEB_FILE" ]; then
    mv "$DEB_FILE" "$PROJECT_DIR/"
    echo "==> Done: $PROJECT_DIR/$(basename "$DEB_FILE")"
else
    echo "Error: .deb file not found after cpack"
    exit 1
fi
