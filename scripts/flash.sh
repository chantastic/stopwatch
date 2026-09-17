#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
PYTHON="${PYTHON:-python3}"
ARTIFACT_DIR="$REPO_ROOT/.build/firmware"
BUILD=1
PORT=''
CLOCK_OFFSET=''
INITIALIZE_PROFILE_STORAGE=0
usage() {
  echo "Usage: $0 [--no-build | --artifact-dir DIR] [--offset-minutes MINUTES] [--initialize-profile-storage] SERIAL_PORT" >&2
  echo "  --initialize-profile-storage: explicit first-install opt-in; if storage cannot mount, erases all ffat filesystem data. Preserves NVS." >&2
}
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build) BUILD=0; shift ;;
    --initialize-profile-storage) INITIALIZE_PROFILE_STORAGE=1; shift ;;
    --artifact-dir)
      [[ $# -ge 2 && -n "$2" ]] || { usage; exit 2; }
      ARTIFACT_DIR="$2"; BUILD=0; shift 2 ;;
    --offset-minutes)
      [[ $# -ge 2 && "$2" =~ ^-?[0-9]+$ ]] || { usage; exit 2; }
      CLOCK_OFFSET="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    -*) usage; exit 2 ;;
    *) [[ -z "$PORT" ]] || { usage; exit 2; }; PORT="$1"; shift ;;
  esac
done
[[ -n "$PORT" ]] || { usage; exit 2; }
# Fail before writing flash if the provisioning tool/interpreter is unavailable.
"$PYTHON" "$REPO_ROOT/scripts/provision-clock.py" --help >/dev/null
if [[ -n "$CLOCK_OFFSET" ]]; then
  "$PYTHON" -c 'import sys; n=int(sys.argv[1]); sys.exit(0 if -840 <= n <= 840 else "Clock offset must be from -840 to 840 minutes")' "$CLOCK_OFFSET"
fi
FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,UploadSpeed=460800'
if [[ "$BUILD" == 1 ]]; then "$REPO_ROOT/scripts/build.sh"; fi
for component in devices_badge.ino.bin devices_badge.ino.bootloader.bin devices_badge.ino.partitions.bin; do
  [[ -f "$ARTIFACT_DIR/$component" ]] || { echo "Missing firmware component: $ARTIFACT_DIR/$component" >&2; exit 2; }
done
# Read the on-device partition table before any upload. A factory/other layout
# needs a separately approved backup/migration, never an implicit table rewrite.
mkdir -p "$REPO_ROOT/.build/partition-preflight"
PREFLIGHT_STATE="$(mktemp "$REPO_ROOT/.build/partition-preflight/target.XXXXXX")"
trap 'rm -f "$PREFLIGHT_STATE"' EXIT
PORT="$("$PYTHON" "$REPO_ROOT/scripts/check_flash_layout.py" \
  --port "$PORT" --fqbn "$FQBN" --arduino-cli "$ARDUINO_CLI" \
  --compiled "$ARTIFACT_DIR/devices_badge.ino.partitions.bin" \
  --sketch "$REPO_ROOT/firmware/devices_badge" \
  --output-dir "$REPO_ROOT/.build/partition-preflight" --state-file "$PREFLIGHT_STATE")"
[[ "$PORT" == /dev/* && "$PORT" != *$'\n'* ]] || { echo "Preflight did not return a verified device port; upload blocked." >&2; exit 1; }
"$ARDUINO_CLI" upload --fqbn "$FQBN" --port "$PORT" \
  --input-dir "$ARTIFACT_DIR" \
  "$REPO_ROOT/firmware/devices_badge"
PORT="$("$PYTHON" "$REPO_ROOT/scripts/check_flash_layout.py" --resolve-port \
  --arduino-cli "$ARDUINO_CLI" --state-file "$PREFLIGHT_STATE")"
[[ "$PORT" == /dev/* && "$PORT" != *$'\n'* ]] || { echo "Uploaded device port could not be verified; readiness blocked." >&2; exit 1; }
# No chip erase or filesystem/NVS image: saved state and the partition scheme
# stay intact. Every unit gets its own current time from the flashing computer.
CLOCK_COMMAND=("$PYTHON" "$REPO_ROOT/scripts/provision-clock.py" "$PORT")
if [[ -n "$CLOCK_OFFSET" ]]; then CLOCK_COMMAND+=(--offset-minutes "$CLOCK_OFFSET"); fi
if [[ "$INITIALIZE_PROFILE_STORAGE" == 1 ]]; then CLOCK_COMMAND+=(--initialize-profile-storage); fi
if ! "${CLOCK_COMMAND[@]}"; then
  echo "Firmware uploaded, but clock or readiness verification FAILED. Do not mark this unit ready." >&2
  exit 1
fi
