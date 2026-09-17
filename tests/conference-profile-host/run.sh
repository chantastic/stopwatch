#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/.build/tests/conference"
mkdir -p "$OUT"
python3 - "$OUT" <<'PY'
import pathlib,sys
out=pathlib.Path(sys.argv[1])
for name,side in [('synthetic',16),('oversize',513)]:
    (out/f'{name}.ppm').write_bytes(f'P6\n{side} {side}\n255\n'.encode()+bytes([255,20,0])*(side*side))
PY
for name in synthetic oversize; do
  sips -s format jpeg "$OUT/$name.ppm" --out "$OUT/$name.jpg" >/dev/null
done
"${CXX:-clang++}" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wno-deprecated-declarations \
  -I"$ROOT/tests/conference-profile-host/include" -I"$ROOT/tests/profile-store-test/include" \
  "$ROOT/tests/conference-profile-host/check.cpp" -o "$OUT/check"
"$OUT/check" "$OUT/synthetic.jpg" "$OUT/oversize.jpg"
"${CXX:-clang++}" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wno-deprecated-declarations \
  -I"$ROOT/tests/conference-profile-host/include" -I"$ROOT/tests/profile-store-test/include" \
  -I"${ARDUINOJSON_INCLUDE:-$HOME/Documents/Arduino/libraries/ArduinoJson/src}" \
  "$ROOT/tests/conference-profile-host/portal.cpp" -o "$OUT/portal"
"$OUT/portal" "$OUT/synthetic.jpg" "$OUT/setup.html"
python3 - "$OUT" <<'PY'
import pathlib,re,sys
out=pathlib.Path(sys.argv[1])
html=(out/'setup.html').read_text()
script=re.search(r'<script>(.*?)</script>',html,re.S)
assert script, 'Rendered setup page has no client script'
(out/'setup.js').write_text(script[1])
PY
node --check "$OUT/setup.js"
