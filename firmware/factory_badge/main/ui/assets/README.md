# Supplied conference marks

The logo, arrow and bookmark PNGs are the user's isolated September 17, 2026 exports from **init() conf
2026**. They retain their original pixels, dimensions and alpha coverage.
`logo_small.png` is deliberately 50% opaque; do not replace it with typed text.

| Source export | Asset | Native pixels |
| --- | --- | --- |
| `Vector.png` | `logo_small.png` | 57 × 13 |
| `Vector (2).png` | `logo_large.png` | 197 × 46 |
| `Vector (3).png` | `left.png` | 23 × 38 |
| `Vector (1).png` | `right.png` | 23 × 38 |
| `Frame 2147219405.png` | `bookmark.png` | 29 × 29 |
| Empty avatar region in `Profile.png` | `empty_portrait.png` | 160 × 160 |

`python3 scripts/generate-design-assets.py` (Pillow) packages the original alpha
planes into LVGL A8 descriptors, without resizing or tracing. The unselected
bookmark is the one-pixel inner edge of that same supplied silhouette. These
brand assets are separate from dynamic attendee photos. The source MP4 and
screen mockups are not copied into firmware; see `../intro-loop.md` for the
source-derived animation and `../fonts/README.md` for licensed typography.

The empty portrait mask is the exact monochrome region x=153, y=83, w=160,
h=160 of the supplied Profile.png (no resampling). Its luminance becomes alpha
over black. It contains no personal photo or identity.
