#!/bin/bash
set -e  # Exit on error

if [ $# -lt 1 ]; then
  echo "Usage: $0 <project_root>"
  exit 1
fi

PROJECT_ROOT="$1"
BUILD_DIR="$PROJECT_ROOT/build/Desktop-Release"
APPDIR="$BUILD_DIR/AppDir"
BIN_NAME="barryapplauncher"

echo "Using project root: $PROJECT_ROOT"

echo "Syncing AppDir..."
rsync -a --delete "$PROJECT_ROOT/AppDir/" "$APPDIR/"

echo "Creating bin directory in AppDir..."
mkdir -p "$APPDIR/usr/bin/"

echo "Copying binary to AppDir..."
cp "$BUILD_DIR/$BIN_NAME" "$APPDIR/usr/bin/"

QMAKE=$(which qmake6)
if [ -z "$QMAKE" ]; then
  echo "Error: qmake6 not found in PATH."
  exit 1
fi

export QMAKE
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so"
export EXTRA_QT_MODULES="waylandcompositor"
export QML_SOURCES_PATHS="$PROJECT_ROOT/qml"
export QT_PLUGIN_PATH="/usr/lib/qt6/plugins"
export NO_STRIP=1

echo "Running linuxdeploy..."
linuxdeploy --appdir="$APPDIR" --plugin qt --output appimage
