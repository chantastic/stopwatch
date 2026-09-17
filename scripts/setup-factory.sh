#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="$REPO_ROOT/.build/toolchains/esp-idf"
mkdir -p "$REPO_ROOT/.build/toolchains"
if [[ ! -d "$SDK" ]]; then
  git clone --depth 1 --branch v5.5.4 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git "$SDK"
fi
[[ "$(git -C "$SDK" describe --tags --exact-match HEAD)" == v5.5.4 ]] || {
  echo 'Existing SDK is not the pinned ESP-IDF v5.5.4.' >&2; exit 2;
}
"$SDK/install.sh" esp32s3
python3 "$REPO_ROOT/scripts/bootstrap-factory.py"
