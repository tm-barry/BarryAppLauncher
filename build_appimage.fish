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

# Path to barryapplauncher bin
set APPBIN $APPDIR/usr/bin/barryapplauncher

# Function to extract needed libraries using ldd
function collect_used_libs --argument-names paths
    set -l libs
    for p in $paths
        if test -f $p
            for line in (ldd $p ^| awk '{print $1}' ^| grep '^lib')
                if test -n "$line"
                    set libs $libs $line
                end
            end
        end
    end
    printf "%s\n" $libs | sort -u
end

# Find all shared libs in usr/lib
set -l so_files (find $APPDIR/usr/lib -type f -name '*.so*')

# Collect needed libs from the app binary and all .so files in usr/lib
set -l used_libs (collect_used_libs $APPBIN $so_files)
set -l used_libs_list (string join " " $used_libs)

# Delete unused .so files in usr/lib
for libfile in $so_files
    set libname (basename $libfile)
    if not contains $libname $used_libs_list
        echo "Removing unused lib: $libname"
        rm $libfile
    end
end

# Delete additional unused
set folders \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/qml/+Imagine" \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/qml/+Material" \
    "$APPDIR/usr/qml/QtQuick/Dialogs/quickimpl/qml/+Universal" \
    "$APPDIR/usr/qml/QtQuick/Controls/designer" \
    "$APPDIR/usr/qml/QtQuick/Controls/FluentWinUI3" \
    "$APPDIR/usr/qml/QtQuick/Controls/Imagine" \
    "$APPDIR/usr/qml/QtQuick/Controls/Material" \
    "$APPDIR/usr/qml/QtQuick/Controls/Universal" \
    "$APPDIR/usr/qml/QtQuick/Pdf" \
    "$APPDIR/usr/qml/QtQuick/Timeline" \
    "$APPDIR/usr/qml/QtQuick/tooling" \
    "$APPDIR/usr/qml/QtQuick/VirtualKeyboard"

for dir in $folders
    if test -d $dir
        echo "Deleting $dir"
        rm -rf $dir
    else
        echo "Skipping missing $dir"
    end
end

echo "Running linuxdeploy build appimage..."
linuxdeploy --appdir=$APPDIR --output appimage || exit 1
