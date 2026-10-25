#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
APP_BUNDLE="$BUILD_DIR/src/gamepad-tool.app"
VERSION="$(grep 'project(gamepad-tool VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([^ ]*\).*/\1/')"
DMG_NAME="SDL2-Gamepad-Tool-${VERSION}.dmg"
DMG_PATH="$PROJECT_DIR/$DMG_NAME"

# Source .env if present (for APPLE_ID, APPLE_PASSWORD, APPLE_TEAM_ID)
if [ -f "$PROJECT_DIR/.env" ]; then
    set -a
    . "$PROJECT_DIR/.env"
    set +a
fi

if [ ! -d "$APP_BUNDLE" ]; then
    echo "Error: $APP_BUNDLE not found. Run build_macos.sh first."
    exit 1
fi

echo "==> Verifying Qt is bundled"
if [ ! -d "$APP_BUNDLE/Contents/Frameworks/QtCore.framework" ]; then
    echo "Error: Qt frameworks not bundled. macdeployqt may have failed during build."
    exit 1
fi

echo "==> Creating DMG"
hdiutil create \
    -volname "SDL2 Gamepad Tool" \
    -srcfolder "$APP_BUNDLE" \
    -ov -format UDZO \
    "$DMG_PATH"

# Notarize if credentials are available
if [ -n "$APPLE_ID" ] && [ -n "$APPLE_PASSWORD" ] && [ -n "$APPLE_TEAM_ID" ]; then
    echo "==> Notarizing DMG"
    xcrun notarytool submit "$DMG_PATH" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_PASSWORD" \
        --team-id "$APPLE_TEAM_ID" \
        --wait

    echo "==> Stapling notarization ticket"
    xcrun stapler staple "$DMG_PATH"

    echo "==> Verifying notarization"
    spctl --assess --verbose "$APP_BUNDLE"
    echo "==> Notarization complete"
else
    echo "==> Skipping notarization (set APPLE_ID, APPLE_PASSWORD, APPLE_TEAM_ID to enable)"
fi

echo "==> Done: $DMG_PATH"
