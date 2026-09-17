# A reusable display and touch foundation

Research date: September 17, 2026. Scope: reducing repeated integration work across
**different hardware models**, rather than calibrating each individual StopWatch.
Two research agents independently examined upstream drivers and framework choices.
This is source research; no new firmware build, flash, or physical test was performed.

## Recommendation

Use **LVGL for the UI, above a tested board-support layer**. A board-support package
(BSP) describes how a particular board's display, touch controller and other
peripherals work. Keep the same UI and diagnostic screen when changing hardware;
replace the board adapter or configuration.

LVGL provides reusable widgets, layout, scrolling and input handling across MCU
vendors and operating systems. It still needs correct display and input integration.
[LVGL overview](https://github.com/lvgl/lvgl/blob/master/README.md)

The missing boundary in this project is ownership of coordinate conversion:
sensor readings should become correctly oriented screen pixels **before** gesture
recognition or button hit testing. Application pages should never acquire their
own board-specific scale factors. This is an architectural recommendation, not a
claim that a framework can discover arbitrary hardware geometry automatically.

## Practical choices

| Project | Recommended starting point | Main limitation |
| --- | --- | --- |
| This Arduino/M5 badge | Validate M5Unified/M5GFX first; use that hardware layer beneath LVGL if migrating the UI | Existing StopWatch geometry needs investigation; adding LVGL alone cannot fix it |
| Other Arduino ESP32 displays | LVGL + ESP32_Display_Panel supported-board configuration | Unsupported boards/controllers need an adapter |
| New ESP-IDF projects | LVGL + ESP-BSP and esp_lcd_touch, using the board's compatible LVGL integration | Board coverage, display interface and pinned dependency versions determine support |
| Configurable dashboards/appliances | ESPHome + LVGL | Our profile/image portal and badge services would require custom work |

[ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel)
provides shared display/touch drivers and board definitions across vendors.
Its published lists include CST820 touch and several M5 boards, but do not list
StopWatch or its CO5300 display controller. Likewise,
[ESP-BSP](https://github.com/espressif/esp-bsp) lists several M5 boards but not
StopWatch. Neither is a verified drop-in replacement for this badge.

For a new ESP-IDF integration, evaluate the current
[esp_lvgl_adapter](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/tools/esp_lvgl_adapter.html).
It supports LVGL 8/9 and requires ESP-IDF 5.5 or later. Its QSPI/SPI path still
requires panel orientation and corresponding touch mapping; it does not
automatically provide our four-way rotation. Existing BSPs may use
[esp_lvgl_port](https://components.espressif.com/components/espressif/esp_lvgl_port/versions/2.9.0/readme),
which already supplies configurable touch scale and LVGL integration. Choose a
tested combination instead of swapping integration libraries solely for recency.

ESPHome demonstrates the desired reusable pattern particularly clearly: its
[touchscreen base](https://developers.esphome.io/architecture/components/touchscreen/)
applies normalization and mounting transforms centrally, and its
[LVGL rotation setting](https://esphome.io/components/lvgl/#display-rotation)
coordinates display and touch rotation. Its standard
[captive portal](https://esphome.io/components/captive_portal/) configures Wi-Fi
and firmware; it does not replace our name/social/image importer.

## What the StopWatch investigation established

### The factory application follows a different touch path

The factory application uses LVGL, Smooth UI Toolkit and Mooncake, with a custom
CST820 driver feeding coordinates directly into LVGL. M5GFX handles its display,
but its touch input does not pass through our M5Unified/M5GFX conversion path.
This provides a useful independent reference to compare on hardware.
[Factory input/display HAL](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/hal/hal_display.cpp),
[CST820 driver](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/hal/drivers/cst820/cst820.cpp)

At that factory revision, the display HAL uses 468×466 and the launcher uses a
466×466 container. Manufacturer-visible resolution is 466×466; our M5GFX
framebuffer is 468×468. These layers must be reconciled, rather than changing a
dimension based only on the product specification. See
[stock UI reference](stock-ui-reference.md) and [hardware findings](hardware.md).

### Upstream contains a coordinate inconsistency, but it is not the full explanation

Both pinned M5GFX 0.2.26 and inspected 0.2.29 declare a StopWatch touch maximum
of 233. The framebuffer initially attaches touch with default 240×240 geometry,
then copies the final display geometry. The retained conversion scales by
239/233, about 1.026.
[Touch configuration](https://github.com/m5stack/M5GFX/blob/0.2.29/src/M5GFX.cpp#L2060-L2067),
[framebuffer initialization](https://github.com/m5stack/M5GFX/blob/0.2.29/src/lgfx/v1/panel/Panel_AMOLED.hpp#L89-L112),
[conversion](https://github.com/m5stack/M5GFX/blob/0.2.29/src/lgfx/v1/panel/Panel_Device.cpp#L399-L521)

That 2.6% scale does **not** explain the much larger empirical correction from
our physical tests. Recalculating against the final display size while retaining
maximum 233 would approximately double coordinates and worsen the situation.
No reviewed release establishes a complete StopWatch alignment fix.

Our `scale-offset-2` follow-up improved median center-target error from about
22.5 to 7.8 pixels across the tested poses. It does not validate the edges, other
users or other boards; most follow-up holds were shorter than the test criterion.
The separate screen-relative offset remains empirical and may include aiming
behavior. Preserve it as a provisional result, not a universal hardware constant.
Detailed measurements and limitations remain in [hardware.md](hardware.md).

### The current library already has the required calibration math

M5GFX provides stored calibration and an affine transform: continuous scale,
offset and axis correction applied before rotation. M5Unified then recognizes
gestures. Our current application correction happens afterward. Moving a verified
hardware correction into the lower layer would make gestures and hit testing use
the same coordinates.
[Calibration API](https://docs.m5stack.com/en/arduino/m5gfx/m5gfx_touch),
[native transform and rotation](https://github.com/m5stack/M5GFX/blob/0.2.29/src/lgfx/v1/panel/Panel_Device.cpp#L418-L521),
[M5Unified input pipeline](https://github.com/m5stack/M5Unified/blob/0.2.19/src/utility/Touch_Class.cpp)

The built-in wizard targets rectangular corners outside our round screen. Reuse
the transformation API, with visible inset targets if calibration is needed.
[Wizard implementation](https://github.com/m5stack/M5GFX/blob/0.2.29/src/lgfx/v1/LGFXBase.cpp#L3670-L3719)

Capacitive controllers generally provide calibrated coordinates themselves;
driver geometry and mounting deserve investigation before routine manual
calibration. Resistive panels have different calibration requirements.
[Espressif touch guidance](https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/touch_panel.html)

## Reusable contract and next experiment

One board profile should record controller/pins/reset, visible and framebuffer
dimensions, offsets, sensor range, mounting transform and rotation ownership.
One adapter should expose pixel coordinates and press/release state to the UI.
Any exceptional saved calibration should be versioned against that board profile.

Carry one diagnostic screen between projects: raw, normalized and rotated
coordinates; center and visible edge targets; tap/drag/release checks in every
supported orientation. Host transform checks catch double rotation and incorrect
origins, but physical targets are still needed to validate a new hardware model.

For this board, the next experiment should compare a minimal factory-style raw
input path with our current path at the same precise visible targets and poses.
Establish the controller range and panel offsets together. Only replace the
working correction after the alternative passes independent center and edge
measurements. Keep this driver experiment separate from an eventual LVGL UI port.

The intended payoff is to qualify a board adapter once per hardware model and
reuse it across apps. Unsupported hardware still needs initial integration;
screen-by-screen touch tuning should disappear.
