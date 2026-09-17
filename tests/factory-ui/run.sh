#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FACTORY_COMPONENTS="${FACTORY_COMPONENTS:-$REPO_ROOT/.build/factory-components}"
BUILD_DIR="$REPO_ROOT/.build/tests/factory-ui"
for component in lvgl mooncake smooth_ui_toolkit; do
  if [[ ! -d "$FACTORY_COMPONENTS/$component" ]]; then
    echo 'Fetch the pinned factory components first (scripts/setup-factory.sh).' >&2
    exit 2
  fi
done
mkdir -p "$BUILD_DIR"
cmake -S "$REPO_ROOT/tests/factory-ui" -B "$BUILD_DIR" \
  -DFACTORY_COMPONENTS="$FACTORY_COMPONENTS" -DCMAKE_BUILD_TYPE=Debug > "$BUILD_DIR/configure.log" 2>&1 || {
  cat "$BUILD_DIR/configure.log" >&2; exit 1;
}
cmake --build "$BUILD_DIR" --parallel "${FACTORY_TEST_JOBS:-4}" > "$BUILD_DIR/build.log" 2>&1 || {
  tail -80 "$BUILD_DIR/build.log" >&2; exit 1;
}
ctest --test-dir "$BUILD_DIR" --output-on-failure -R '^factory_ui$'
