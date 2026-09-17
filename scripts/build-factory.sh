#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${IDF_PATH:-$REPO_ROOT/.build/toolchains/esp-idf}"
if [[ ! -f "$SDK/export.sh" ]]; then
  echo 'Install ESP-IDF v5.5.4 with scripts/setup-factory.sh first.' >&2
  exit 2
fi
[[ "$(git -C "$SDK" describe --tags --exact-match HEAD)" == v5.5.4 ]] || {
  echo 'Factory build requires the pinned ESP-IDF v5.5.4.' >&2; exit 2;
}
python3 "$REPO_ROOT/scripts/bootstrap-factory.py"
source "$SDK/export.sh" >/dev/null
idf.py -C "$REPO_ROOT/firmware/factory_badge" -B "$REPO_ROOT/.build/factory" \
  -D SDKCONFIG="$REPO_ROOT/.build/factory-sdkconfig" reconfigure build
# The established flasher takes these three names. They are native ESP-IDF
# artifacts; naming them here preserves the reviewed preflight and batch flow.
mkdir -p "$REPO_ROOT/.build/firmware"
cp "$REPO_ROOT/.build/factory/conference_badge.bin" "$REPO_ROOT/.build/firmware/devices_badge.ino.bin"
cp "$REPO_ROOT/.build/factory/bootloader/bootloader.bin" "$REPO_ROOT/.build/firmware/devices_badge.ino.bootloader.bin"
cp "$REPO_ROOT/.build/factory/partition_table/partition-table.bin" "$REPO_ROOT/.build/firmware/devices_badge.ino.partitions.bin"
