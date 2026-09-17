# Factory HAL sources

`cst820/`, `rx8130/`, and the display/power portions of `../board.cpp` derive
from M5Stack's MIT-licensed [StopWatch UserDemo at 6b4aa125](https://github.com/m5stack/M5StopWatch-UserDemo/tree/6b4aa125288b6fe9dca661f10159f6e1e5ee785c).
The license is retained in `LICENSE-M5Stack`.

The touch controller's coordinates are passed directly to LVGL. The copied
CST820 register decoder has bounded I2C waits and no empirical scale/offset.
LVGL owns input rotation. Board observations call the same LVGL point transform
on a separate copy; transformed observations are never fed back into LVGL.

The RX8130 helper retains the factory register protocol but adds checked I/O,
calendar validation, one-hot weekday encoding compatible with the prior
firmware, UTC-only conversion, and verified writes. Reading never repairs or
rewrites the RTC and does not change the global timezone.

The board layer omits factory NVS erasure, filesystem initialization, audio,
vibration tasks, network startup, and independent LVGL/power-reader tasks.
