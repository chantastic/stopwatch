# Stock StopWatch UI reference

Inspected September 16, 2026. The official
[M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/6b4aa125288b6fe9dca661f10159f6e1e5ee785c)
revision was `6b4aa125288b6fe9dca661f10159f6e1e5ee785c`. This is a source
inspection, not a measurement of factory firmware on the connected device.
The temporary checkout and downloaded dependency licenses are under ignored
`.build/vendor-reference/`.

For the September 17 investigation into a reusable foundation across different
hardware models, see [touch framework research](touch-framework-research.md).
That recommendation is separate from the initial scaffold decision below.

## Frameworks and reuse

The stock application uses **LVGL**, with the **Smooth UI Toolkit** C++ wrappers
and **Mooncake** application lifecycle management. Its launcher arrangement is
custom application code. These are reusable libraries, but the complete factory
application is an **ESP-IDF 5.5.4** project with its own hardware abstraction,
RTOS tasks, display/touch integration, and component dependencies. It is not an
Arduino sketch that can be added unchanged to this project.

The versions below are the stock project's declarations in
[repos.json](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/repos.json),
not proposed replacements for this project's pinned dependencies.

| Component | Stock version | Purpose | Verified top-level license |
| --- | --- | --- | --- |
| [LVGL](https://github.com/lvgl/lvgl/tree/v9.5.0) | 9.5.0 | Widgets, layout, input, rendering | [MIT](https://github.com/lvgl/lvgl/blob/v9.5.0/LICENCE.txt) |
| [Smooth UI Toolkit](https://github.com/Forairaaaaa/smooth_ui_toolkit/tree/v2.12.1) | 2.12.1 | LVGL C++ wrappers and animation utilities | [MIT](https://github.com/Forairaaaaa/smooth_ui_toolkit/blob/v2.12.1/LICENSE) |
| [Mooncake](https://github.com/Forairaaaaa/mooncake/tree/v2.3.3) | 2.3.3 | App creation, opening, updating, closing | [MIT](https://github.com/Forairaaaaa/mooncake/blob/v2.3.3/LICENSE) |
| [M5GFX](https://github.com/m5stack/M5GFX/tree/0.2.19) | 0.2.19 | Panel/framebuffer operations beneath LVGL | [MIT](https://github.com/m5stack/M5GFX/blob/0.2.19/LICENSE) |

Mooncake and Smooth UI Toolkit both provide Arduino `library.properties` files.
Smooth's LVGL wrapper still requires a working LVGL port; those manifests do not
make the stock HAL or application Arduino-compatible. Mooncake's documented
manager APIs are not thread-safe. Introducing it would require preserving one
owner for lifecycle updates or explicit synchronization.

For this five-page scaffold, retain the established M5Unified/M5GFX rendering,
touch, PSRAM and orientation implementation. Reproduce the useful visual and
interaction patterns directly. A later move to LVGL is reasonable if the UI
grows enough to justify a new display/input port and its verification.

## Layout and controls

The user's reference image shows a black round face, a small clock near the top,
white side chevrons, centered content, a page label below, and dots near the
bottom. It also visibly places the yellow pusher on the left of the upright
stock face. The image is a layout reference, not a calibrated display capture.

The stock
[launcher view](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/apps/app_launcher/view/view.cpp)
uses a 466 × 466 panel and these center-relative positions:

| Element | Stock arrangement |
| --- | --- |
| Previous/next controls | Floating hit areas 52 × 160 at x = −200/+200, y = 0 |
| Content icon | Centered at y = −15 |
| Page label | Centered at y = +155 |
| Page dots | Centered at y = +200; active diameter 14, others 8, spacing 16 |
| Clock | Floating, top-center with a 4-pixel offset |

The stock launcher implements horizontal snap scrolling with repeated copies
to simulate a loop. Floating navigation stays fixed while content moves. Use
the pattern, while retaining this project's actual **468 × 468** framebuffer
and circular visibility constraints. No need to duplicate the stock icon-copy
mechanism for five directly rendered pages.

The [manufacturer pin map](https://docs.m5stack.com/en/core/StopWatch#pinmap)
identifies yellow A as GPIO 2 and blue B as GPIO 1. Stock
[key handling](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/apps/common/key_manager/key_manager.cpp)
maps completed A clicks to previous, B clicks to next, and a simultaneous hold
to Home. Stock display/touch handling does not remap those keys by orientation.

This project's previously measured loop-up orientation is rotation 2. Combining
that 180-degree calibration with the supplied photo implies **blue is the left
pusher when hanging**, and yellow is the right: swap previous/next at rotation
2. This is an inference from the image and existing calibration, not a fresh
physical-button test. At rotations 1/3 the pushers are above/below; keeping A as
previous and B as next is an explicit convention, not a claim of left/right
alignment. Preserve completed taps and rotation after touch release; factory
LVGL input code is not a replacement for the current calibrated touch filter.

## License scope

The factory [root license](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/LICENSE)
is MIT, copyright 2026 M5Stack Technology CO LTD; the inspected launcher, key,
display and IMU source files also carry MIT SPDX headers. Copies or substantial
adaptations of source must retain the applicable copyright and permission
notice. The library licenses above were checked at their declared tags.

This is not an audit of every transitive dependency, font, logo or image in the
factory tree. Check those assets' own provenance before importing them. The
conference scaffold can use original chevron/dot drawing and existing licensed
project assets without bundling the factory art or full framework stack.
