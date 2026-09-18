# Native UI host checks

Run `bash tests/factory-ui/run.sh` after fetching the pinned factory dependencies.
This compiles the production page files with real LVGL, Smooth container ownership
and Mooncake lifecycle, using a memory framebuffer and a simulated pointer input.
Address/undefined-behavior sanitizers are enabled by default.

Checks cover hidden invitation traversal and five/six centered dots, noninterrupting
reveal during agenda scrolling/modals, Morse-success page entry, all six pages;
supplied-brand chrome and square page indicators after partial redraws; no redraws on idle
static pages; release-only buttons; drag-out-and-back, within-button drag and
long-hold cancellation; real agenda text and wrapped titles/details; current/past/upcoming
styles, release-only bookmark toggles, invalid time, daily reset, entry focus and preserved schedule scroll; native
schedule/social scrolling; dynamic profile and
avatar/company updates, social-only profiles and configured faces without chrome; QR expansion; setup return paths; and Touch test lifecycle.
They do not validate the physical sensor, display transport, or orientation HAL.

Optional review images (PPM, outside Git):

```
.build/tests/factory-ui/factory_ui_test .build/factory-ui-captures
```

`FACTORY_COMPONENTS` may point to an alternate directory containing the same
pinned `lvgl`, `mooncake`, and `smooth_ui_toolkit` component sources.
