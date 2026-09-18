# Native clock host checks

Run `python3 tests/factory-clock/run.py`. Clang and the C++ standard library are
required. Generated files remain in `.build/tests/factory-clock`.

The fixture compiles the actual native clock implementation and the actual
phone-response field builder. Host adapters replace RTC, system-clock, NVS, and
cJSON output calls. All RTC/NVS/system-clock access asserts main-thread ownership;
phone calls run on a separate worker and reach hardware only through `poll()`.

Checks cover retained startup time, 12-hour display (midnight/noon, unpadded
hours, invalid placeholders), timezone/day/year rollover with UTC unchanged,
fractional-hour offsets, epoch/offset bounds,
unchanged-offset write avoidance, request exclusion, actual readback fields
(including `rtc_epoch`), and RTC-write, final-readback, NVS, and system-clock
failures. The response test uses a readback one second after the request to catch
accidental request echoes. Address/undefined-behavior sanitizers are enabled.

These tests do not access hardware, run the HTTP server, or prove phone Wi-Fi
association. Browser-script checks remain in `tests/factory-services`.
