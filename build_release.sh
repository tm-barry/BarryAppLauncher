#!/bin/bash
set -e  # Exit on error

if [ $# -lt 1 ]; then
  echo "Usage: $0 <project_root>"
  exit 1
fi

PROJECT_ROOT="$1"
BUILD_DIR="$PROJECT_ROOT/build/AppImage-Release"

cmake -G Ninja \
  -S "$PROJECT_ROOT" \
  -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/tmp \
  -DCMAKE_PREFIX_PATH=/usr

cmake --build "$BUILD_DIR" -- -j$(nproc)
