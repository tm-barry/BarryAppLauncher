#!/bin/bash
set -euo pipefail  # Safer: exit on error, undefined vars, or pipe failures

if [ $# -lt 1 ]; then
  echo "Usage: $0 <project_root>"
  exit 1
fi

PROJECT_ROOT="$(realpath "$1")"
BUILD_DIR="$PROJECT_ROOT/build/AppImage-Release"
APPDIR="$BUILD_DIR/AppDir"
BIN_NAME="barryapplauncher"

echo "Using project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "AppDir: $APPDIR"

# Sync AppDir structure
echo "Syncing AppDir..."
mkdir -p "$APPDIR"
rsync -a --delete "$PROJECT_ROOT/AppDir/" "$APPDIR/"

# Copy app binary
echo "Copying binary to AppDir..."
mkdir -p "$APPDIR/usr/bin/"
cp "$BUILD_DIR/$BIN_NAME" "$APPDIR/usr/bin/"

# Set required environment variables for linuxdeploy
QMAKE=$(command -v qmake6 || true)
if [ -z "$QMAKE" ]; then
  echo "Error: qmake6 not found in PATH."
  exit 1
fi

export QMAKE
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so"
export EXTRA_QT_MODULES="waylandcompositor"
export QML_SOURCES_PATHS="$PROJECT_ROOT/qml"
export QT_PLUGIN_PATH="/usr/lib/qt6/plugins"
export DEPLOY_PLATFORM_THEMES=true

# First pass of linuxdeploy to populate AppDir
echo "Running linuxdeploy build AppDir..."
linuxdeploy --appdir="$APPDIR" --plugin qt

# Prune unneeded QML modules
echo "Removing unused QML modules..."
folders=(
    "QtQuick/Dialogs/quickimpl/qml/+Imagine"
    "QtQuick/Dialogs/quickimpl/qml/+Material"
    "QtQuick/Dialogs/quickimpl/qml/+Universal"
    "QtQuick/Controls/designer"
    "QtQuick/Controls/FluentWinUI3"
    "QtQuick/Controls/Imagine"
    "QtQuick/Controls/Material"
    "QtQuick/Controls/Universal"
    "QtQuick/Pdf"
    "QtQuick/Timeline"
    "QtQuick/tooling"
    "QtQuick/VirtualKeyboard"
)

for dir in "${folders[@]}"; do
    full="$APPDIR/usr/qml/$dir"
    if [ -d "$full" ]; then
        echo "Deleting $full"
        rm -rf "$full"
    else
        echo "Skipping missing $full"
    fi
done

# Generate final AppImage in $BUILD_DIR
echo "Running linuxdeploy build AppImage..."
cd "$BUILD_DIR"
linuxdeploy --appdir="$APPDIR" --output appimage

echo "✅ AppImage created in $BUILD_DIR"
