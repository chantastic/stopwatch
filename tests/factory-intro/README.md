# Native intro animation checks

Run `bash tests/factory-intro/run.sh` after fetching the pinned factory components.
This compiles the real LVGL decoder and embedded intro asset with address and
undefined-behavior sanitizers. Undefined behavior stops the test.

Every RGB565 pixel contributes to a deterministic fingerprint checked against
Pillow 12.3's independent decoding of the emitted GIF. The fixture covers all
60 frames and their exact delays across repeated ten-second loops, including
the last-frame delay. Three separate visits create/delete the whole parent tree
and verify that each GIF's timer is released. A second three-frame synthetic
fixture covers opaque frames, restore-to-background disposal and a subsequent
partial frame, with the same repeat/deletion checks.

The full loop uses transparent delta frames; these checks caught an upstream
RGB565 decoder branch painting its background color over unchanged pixels. The
sanitizers also caught signed shifts in the decoder's little-endian byte reader,
and the timing comparison caught a last-frame delay being ignored. The bounded
fixes are retained in `firmware/factory_badge/patches/lvgl.patch` and applied by
the existing dependency bootstrap. No alternate GIF decoder is included.

The supplied source recording and intermediate GIF remain outside Git. Update
the independent reference only when deliberately replacing the embedded asset:

```sh
python3 scripts/generate-intro-loop.py /path/to/shader-recording-1786483880341.mp4
python3 tests/factory-intro/generate-reference.py .build/design-loop/intro_loop.gif
```

Reference generation requires Pillow. The regular test needs only the usual
Clang/CMake factory host environment. Generated reference data contains frame
fingerprints, delays and a tiny synthetic GIF, with no source-video frames or
hardware captures. This is a decoder/lifecycle simulation; it does not establish
physical display refresh rate, input latency or battery use.
