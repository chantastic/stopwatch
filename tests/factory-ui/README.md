# Native UI host checks

Run `bash tests/factory-ui/run.sh` after fetching the pinned factory dependencies.
This compiles the production page files with real LVGL, Smooth container ownership
and Mooncake lifecycle, using a memory framebuffer and a simulated pointer input.
Address/undefined-behavior sanitizers are enabled by default.

Checks cover all six pages and centered indicators while the invitation is locked
or unlocked; the exact `tap code to reveal` prompt; native touch-Morse taps/holds,
complete-word callbacks and cancellation on drags, press loss, page/modal changes
and rotation; noninterrupting timed reveal during agenda scrolling/modals;
supplied-brand chrome and square page indicators after partial redraws; no redraws on idle
static pages; release-only buttons; drag-out-and-back, within-button drag and
long-hold cancellation for ordinary buttons (the code surface accepts holds);
real agenda text and wrapped titles/details; current/past/upcoming
styles, release-only bookmark toggles, invalid time, daily reset, entry focus and preserved schedule scroll; native
schedule/social scrolling; dynamic profile and
avatar/company updates, social-only profiles and configured faces without chrome; QR expansion; setup return paths; and Touch test lifecycle.
They do not validate the physical sensor, display transport, or orientation HAL.
The touch-code view uses the production `MorseUnlock` timings: dots 50–349 ms,
dashes 350–1400 ms, symbol gaps 50–599 ms, letter pauses 600–2999 ms, a three-second
reset and a 15-second attempt bound. Main owns clock policy and NVS persistence;
the UI fixture observes its callback without writing a device unlock.

Optional review images (PPM, outside Git):

```
.build/tests/factory-ui/factory_ui_test .build/factory-ui-captures
```

`FACTORY_COMPONENTS` may point to an alternate directory containing the same
pinned `lvgl`, `mooncake`, and `smooth_ui_toolkit` component sources.
