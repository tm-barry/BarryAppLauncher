#!/usr/bin/env fish

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
rsync -a --delete $PROJECT_ROOT/AppDir/ $APPDIR/ || exit 1

# Create bin directory in AppDir
echo "Creating bin directory in AppDir..."
mkdir -p $APPDIR/usr/bin/ || exit 1

# Copy binary to AppDir
echo "Copying binary to AppDir..."
cp $BUILD_DIR/$BIN_NAME $APPDIR/usr/bin/ || exit 1

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
set -x DEPLOY_PLATFORM_THEMES true
set -x NO_STRIP 1

echo "Running linuxdeploy build AppDir..."
linuxdeploy --appdir=$APPDIR --plugin qt || exit 1

echo "Stripping ELF binaries in AppDir using system strip..."
find "$APPDIR" -type f -print0 | xargs -0 file | grep 'ELF 64-bit' | cut -d: -f1 | while read -l elf_file
    echo "Stripping $elf_file"
    strip --strip-all "$elf_file" ^/dev/null
end

echo "Running linuxdeploy build appimage..."
linuxdeploy --appdir=$APPDIR --output appimage || exit 1
