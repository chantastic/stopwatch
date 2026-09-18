# Factory board host checks

Run `bash tests/factory-board/run.sh` after fetching factory dependencies.
Requires a C/C++ toolchain and CMake; uses pinned LVGL 9.5.0 and M5GFX 0.2.19
source trees in `.build/factory-components` (override with `LVGL_SOURCE_DIR`
and `M5GFX_SOURCE_DIR`). Build output and logs stay under
`.build/tests/factory-board`.

The rotation test compiles the production board adapter and the real LVGL core,
then checks every pixel through the inverse-input/forward-LVGL pipeline in all
four orientations of the non-square factory framebuffer. It does not duplicate
LVGL's transformation formulas and does not establish physical alignment.

The RTC test compiles the production RX8130 driver with a register-level I2C
fake. It covers epoch/calendar boundaries, leap years, invalid BCD and dates,
voltage-loss/stopped-clock rejection, read-only reads, weekday encoding, and
failure recovery. It does not establish electrical behavior or retained power.

The touch provenance test uses production sample-update helpers to verify that
idle sensor polls after an injected release keep the marker labeled simulated,
until a fresh physical contact supplies a real coordinate.

The vibration test runs the production input-feedback controller against a fake
motor transport. It checks startup OFF, one-time 5 kHz setup, 40% duty, contact
release/cancellation, the 1.4-second hold cap, timer wrap, no repeated held/idle
writes, ambiguous ON failure followed by immediate OFF, and rate-limited OFF
retries. It does not measure vibration strength, motor current, I2C latency or
physical shutoff. The deadline runs on the main task; it is not a hardware timer.

The flush test compiles the actual pinned M5GFX framebuffer driver and pixel
conversion implementation against memory, with only electrical/platform calls
stubbed. It reproduces the old streaming-write regression: at 180 degrees, RAM
changes while the driver's dirty rectangle remains empty, preventing AMOLED
refresh. It then exercises production `flushImageChunks` for full-screen and
partial edge rectangles in all four orientations. Every changed pixel must be
inside the driver's dirty rectangle and vice versa; driver readback must match
every source RGB565 pixel, and each image must remain within 8,192 pixels.
The tiny SDL headers select M5GFX host declarations; no SDL runtime is used.
These checks establish memory, rotation, and dirty tracking behavior, not
physical SPI transfer, panel scanout, or embedded SIMD performance.
