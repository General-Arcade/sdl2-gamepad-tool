#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
VERSION="$(grep 'project(gamepad-tool VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([^ )]*\).*/\1/')"
TARBALL_NAME="gamepad-tool-${VERSION}-linux-x86_64"
STAGING="$BUILD_DIR/$TARBALL_NAME"

if [ ! -f "$BUILD_DIR/src/gamepad-tool" ]; then
    echo "Error: binary not found. Run build first."
    exit 1
fi

echo "==> Staging tarball contents"
rm -rf "$STAGING"
mkdir -p "$STAGING/lib"

# Copy binary and data files
cp "$BUILD_DIR/src/gamepad-tool" "$STAGING/"
cp "$PROJECT_DIR/src/resources/gamecontrollerdb.txt" "$STAGING/"
cp "$PROJECT_DIR/src/platform/gamepad-tool.desktop" "$STAGING/"
cp "$PROJECT_DIR/src/resources/images/gamepad-tool.png" "$STAGING/"

# Bundle Qt and SDL2 shared libraries
copy_deps() {
    ldd "$1" | grep "=> /" | awk '{print $3}' | while read -r lib; do
        case "$lib" in
            */libQt5*|*/libSDL2*|*/libicu*|*/libxcb*|*/libxkb*|*/libdbus*)
                cp -n "$lib" "$STAGING/lib/" 2>/dev/null || true
                ;;
        esac
    done
}
copy_deps "$BUILD_DIR/src/gamepad-tool"

# Copy Qt platform plugin
QT_PLUGIN_PATH="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib/x86_64-linux-gnu/qt5/plugins)"
if [ -d "$QT_PLUGIN_PATH/platforms" ]; then
    mkdir -p "$STAGING/plugins/platforms"
    cp "$QT_PLUGIN_PATH/platforms/libqxcb.so" "$STAGING/plugins/platforms/" 2>/dev/null || true
fi

# Create launcher script
cat > "$STAGING/gamepad-tool.sh" << 'LAUNCHER'
#!/bin/sh
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins:$QT_PLUGIN_PATH"
exec "$SCRIPT_DIR/gamepad-tool" "$@"
LAUNCHER
chmod +x "$STAGING/gamepad-tool.sh"

echo "==> Creating tarball"
cd "$BUILD_DIR"
tar czf "$PROJECT_DIR/$TARBALL_NAME.tar.gz" "$TARBALL_NAME"

echo "==> Done: $PROJECT_DIR/$TARBALL_NAME.tar.gz"
