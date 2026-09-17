#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LVGL_SOURCE_DIR="${LVGL_SOURCE_DIR:-$REPO_ROOT/.build/factory-components/lvgl}"
M5GFX_SOURCE_DIR="${M5GFX_SOURCE_DIR:-$REPO_ROOT/.build/factory-components/M5GFX}"
BUILD_DIR="$REPO_ROOT/.build/tests/factory-board"
if [[ ! -f "$LVGL_SOURCE_DIR/CMakeLists.txt" ]]; then
  echo 'Fetch the pinned factory components first, or set LVGL_SOURCE_DIR to LVGL 9.5.0.' >&2
  exit 2
fi
if [[ ! -f "$M5GFX_SOURCE_DIR/src/lgfx/v1/panel/Panel_FrameBufferBase.cpp" ]]; then
  echo 'Fetch the pinned factory components first, or set M5GFX_SOURCE_DIR to M5GFX 0.2.19.' >&2
  exit 2
fi
mkdir -p "$BUILD_DIR"
cmake -S "$REPO_ROOT/tests/factory-board" -B "$BUILD_DIR" \
  -DLVGL_SOURCE_DIR="$LVGL_SOURCE_DIR" -DM5GFX_SOURCE_DIR="$M5GFX_SOURCE_DIR" \
  -DCMAKE_BUILD_TYPE=Debug > "$BUILD_DIR/configure.log" 2>&1 || {
  cat "$BUILD_DIR/configure.log" >&2; exit 1;
}
cmake --build "$BUILD_DIR" --parallel "${FACTORY_TEST_JOBS:-4}" > "$BUILD_DIR/build.log" 2>&1 || {
  tail -80 "$BUILD_DIR/build.log" >&2; exit 1;
}
ctest --test-dir "$BUILD_DIR" --output-on-failure
