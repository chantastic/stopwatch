# Badge views

This directory renders the badge with native LVGL widgets. It does not read
hardware, rotate touch coordinates, access storage, or start network workers.

- `badge_ui.h`: plain model snapshots, callbacks, navigation and modal API.
- `badge_ui.cpp`: Mooncake application lifecycle; Smooth owns the root LVGL
  container. One page is alive at a time. Pages rebuild on navigation, and update
  existing labels/properties when their model values change.
- `ui_internal.h`: shared page interface and session context.
- `widgets.*`: theme colors, labels, buttons, QR codes and branding. All actions
  share a release-only tap binding that permanently cancels a dragged or held
  press, including a drag that leaves and re-enters a button. LVGL owns widget
  hit testing and scroll recognition.
- `chrome.*`: clock, left/right controls, page name and dots on the LVGL top layer.
  Chrome is hidden during setup and Touch test.
- `page_*.cpp`: the six page implementations. Edit a page here without changing
  input drivers or services. Schedule and social cards use native scroll views.
- `modal_*.cpp`: setup credentials and diagnostic touch targets.
- `brand_assets.*`: compact alpha masks derived from the existing official init()
  wordmark and GitHub mark. Attendee photos remain dynamic.

The public runtime supplies a `UiModel` and retains its avatar memory while that
model is displayed. Callbacks request work from the runtime; workers must never
mutate LVGL objects. All `ui_*` calls run on the single UI task. `ui_tick` owns the
Mooncake update; the board port supplies the LVGL tick, and the runtime calls
the LVGL timer handler on the same main task.

The layout is centered in a 468×466 logical frame. Rotation is entirely the board
port's responsibility; `ui_rotation_changed()` only recenters the view. Touch
test displays the coordinates delivered by that port without additional mapping.

Static pages have no periodic full redraw. Animation uses LVGL animations, and
scrolling uses LVGL's regular invalidation. The headless checks in
`tests/factory-ui` exercise production views, real input events, partial rendering
and idle behavior without a physical board.
