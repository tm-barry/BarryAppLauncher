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
export DEPLOY_PLATFORM_THEMES=true
export NO_STRIP=1

echo "Running linuxdeploy build AppDir..."
linuxdeploy --appdir="$APPDIR" --plugin qt

echo "Stripping ELF binaries in AppDir using system strip..."
find "$APPDIR" -type f -print0 | xargs -0 file | grep 'ELF 64-bit' | cut -d: -f1 | while read -r elf_file; do
    echo "Stripping $elf_file"
    strip --strip-all "$elf_file" 2>/dev/null
done

# Path to your AppDir and main binary
APPBIN="$APPDIR/usr/bin/barryapplauncher"

# Function to collect used libraries from ldd
collect_used_libs() {
    local files=("$@")
    local libs=()
    for f in "${files[@]}"; do
        if [[ -f "$f" ]]; then
            while IFS= read -r line; do
                libname=$(echo "$line" | awk '{print $1}' | grep '^lib')
                if [[ -n "$libname" ]]; then
                    libs+=("$libname")
                fi
            done < <(ldd "$f")
        fi
    done
    # Print unique libs
    printf "%s\n" "${libs[@]}" | sort -u
}

# Find all .so* files in usr/lib
mapfile -t so_files < <(find "$APPDIR/usr/lib" -type f -name '*.so*')

# Collect all used libraries from main binary and .so files in usr/lib
mapfile -t used_libs < <(collect_used_libs "$APPBIN" "${so_files[@]}")

# Convert to a searchable pattern (array would be safer, but string is simple here)
used_libs_set=$(printf "%s\n" "${used_libs[@]}" | sort -u)

# Delete any .so* file that isn't referenced by ldd
for libfile in "${so_files[@]}"; do
    libname=$(basename "$libfile")
    if ! grep -qx "$libname" <<< "$used_libs_set"; then
        echo "Removing unused lib: $libname"
        rm -f "$libfile"
    fi
done

echo "Running linuxdeploy build AppImage..."
linuxdeploy --appdir="$APPDIR" --output appimage
