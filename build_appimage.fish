#!/usr/bin/env fish
set -e  # Exit on error

# Check if project root argument is given
if test (count $argv) -lt 1
    echo "Usage: $argv[0] <project_root>"
    exit 1
end

# Use the first argument as project root
set PROJECT_ROOT $argv[1]
set BUILD_DIR $PROJECT_ROOT/build/Desktop-Release
set APPDIR $BUILD_DIR/AppDir
set BIN_NAME barryapplauncher

echo "Using project root: $PROJECT_ROOT"

# Sync AppDir
echo "Syncing AppDir..."
rsync -a --delete $PROJECT_ROOT/AppDir/ $APPDIR/

# Create bin directory in AppDir
echo "Creating bin directory in AppDir..."
mkdir -p $APPDIR/usr/bin/

# Copy binary to AppDir
echo "Copying binary to AppDir..."
cp $BUILD_DIR/$BIN_NAME $APPDIR/usr/bin/

# Find qmake6 executable path
set QMAKE (which qmake6)
if test -z "$QMAKE"
    echo "Error: qmake6 not found in PATH."
    exit 1
end

# Set environment variables
set -x QMAKE $QMAKE
set -x EXTRA_PLATFORM_PLUGINS libqwayland-generic.so
set -x EXTRA_QT_MODULES waylandcompositor
set -x QML_SOURCES_PATHS $PROJECT_ROOT/qml
set -x QT_PLUGIN_PATH /usr/lib/qt6/plugins
set -x NO_STRIP 1

echo "Running linuxdeploy..."
linuxdeploy --appdir=$APPDIR --plugin qt --output appimage
