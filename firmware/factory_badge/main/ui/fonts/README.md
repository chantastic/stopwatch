# Design fonts

The `font_mono_*.c` and `font_sans_*.c` files in the parent directory are generated
LVGL bitmap subsets. Both upstream families are licensed under the SIL Open Font
License 1.1; the complete copyright notices and license texts are retained here.
The converted font data remains under that license.

Families were selected by comparison with the supplied raster mockups. The
source Figma text styles have not been verified; the separate logo/arrow marks
use the exact supplied pixels and do not depend on these fonts.

| Assets | Upstream | Pinned source |
| --- | --- | --- |
| `font_mono_12`, `18`, `20`, `24` | [Roboto Mono Regular](https://github.com/googlefonts/RobotoMono) | Revision `895ec691990d041dd727c7b5afa3ce56525d98e6`, `fonts/ttf/RobotoMono-Regular.ttf` |
| `font_sans_14`, `16`, `20`, `24`, `32` | [Inter Regular](https://github.com/rsms/inter) | [Inter 4.1 release](https://github.com/rsms/inter/releases/tag/v4.1), `extras/ttf/Inter-Regular.ttf` |

Each number is the converter's requested pixel size, not the complete line
height: ascenders, descenders and accented Latin letters can make the line taller.
All fonts contain printable ASCII and Latin-1 (`U+0020–007E`, `U+00A0–00FF`),
with four-bit antialiasing and uncompressed LVGL bitmap data. Kerning is retained
where supplied upstream. This does not add support for other writing systems.

## Regeneration

From the repository root, with Python 3 and Node.js/npm installed:

```sh
python3 scripts/generate-design-fonts.py
```

The script pins [LVGL's `lv_font_conv`](https://github.com/lvgl/lv_font_conv)
to version **1.5.3**, which bundles its runtime dependencies. Its npm tarball
integrity is
`sha512-0xJQThBOw2iptFccSXrKDIUTQAwr/2zhKjCI1lATIRgZo8uvYRTmenKafW9yTw6G0y5AyW00tqGpUtYuTuBIbQ==`.
The converter is an MIT-licensed development tool; it is not linked into firmware.
To use an existing installation, including when local package policy prevents
automatic installation, pass `--converter /path/to/lv_font_conv.js`. The script
checks its version before use and does not change package-manager policy.

Fonts and the upstream release archive are downloaded into `.build/design-fonts/`
and verified against fixed SHA-256 checksums before conversion. Only the generated
C files, declarations, provenance and licenses belong in Git. Source and output
paths in converter headers are relative to the repository, so regeneration does
not embed a workstation path. Normal firmware and host builds use the checked-in
C files and do not download fonts or need Node.js.
Generated files and license copies omit trailing whitespace; the checksums below
identify the unmodified upstream inputs.

| Source file | SHA-256 |
| --- | --- |
| `RobotoMono-Regular.ttf` | `af0bff7599c3df3831755c16e39b3c496df74b8c8d8a1161b14dc8461be17cb4` |
| `RobotoMono` upstream `OFL.txt` | `50ab8dd54680d3473f649c9db86fece88434d097c7834475c1c72d2f8c429215` |
| `Inter-4.1.zip` | `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e` |
| `Inter-Regular.ttf` | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` |
| `Inter` upstream `LICENSE.txt` | `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a` |
