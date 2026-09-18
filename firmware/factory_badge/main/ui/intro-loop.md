# Intro loop source

The intro uses the supplied `shader-recording-1786483880341.mp4`, a silent
3836×2156 H.264 recording with 600 frames over ten seconds. The original remains
outside Git. Its SHA-256 is
`0f8d8810a6d83dd553116e0f64f645df467281d6a2962f50307d97d2e527dbd2`.

Regenerate the embedded asset with:

```sh
python3 scripts/generate-intro-loop.py /path/to/shader-recording-1786483880341.mp4
```

The generator verifies that exact source, center-crops it to the display aspect,
scales to native 468×466 pixels, and samples the complete ten-second sequence at
6 frames per second. A shared palette preserves seven dark shades, reserving
one entry for unchanged pixels. No dithering, colorization or generated motion
is added. The source's motion is retained, with reduced frame rate and palette
to fit the existing application partition. The central event mark remains a
separate LVGL image over this background.

Generated with FFmpeg 9.0.1 on September 17, 2026: **60 frames, 10.000 seconds,
586,699 GIF bytes**, SHA-256
`c3e99e61b4f24b349e9f64cbc58f4ef4e57c83488396aa803da5955a9bde7cb4`.
The generated C array is the flash asset; intermediate GIF/frames stay under
`.build/design-loop/`. The generator rejects outputs larger than 800 KiB.
Native resolution was retained because the half-size trial lost the small
cross shapes visible in the supplied recording.

The prior 12-fps asset (`5d04fb5083fa6201d15802b2850f8358703b8b90751a4697ac70ca49ab7e9d54`)
exceeded the development board's render budget in the September 17 device run:
the measured maximum main-loop gap was 141 ms, with 15 UI ticks over two seconds.
Sampling six frames per second gives 160–170 ms frame intervals while retaining
the complete ten-second source sequence. This lowers redraw frequency without
adding a decoder or changing the runtime. The 16-entry palette trial at six fps
required 1,377,635 bytes, exceeding the 800 KiB asset budget, so the seven-shade
palette is retained. The final six-fps device sample measured 79 main-loop ticks
over two seconds and a 133-ms maximum gap. These counters establish more input
processing opportunities, not a physical-panel playback-duration measurement;
see the [design verification](../../../../docs/design-verification-2026-09-17.md).

`page_init.cpp` uses the existing LVGL 9.5 GIF widget. LVGL owns the decoder,
frame delays and infinite loop. RGB565 decoding needs one 436,176-byte display
buffer plus the library's bounded decoder state; compressed bytes stay in flash.
The page tree deletes its GIF decoder/buffer/timer on navigation or modal entry.
No filesystem media install, task, audio initialization or video player is added.
Encoded duration is not a measurement of achieved hardware playback speed.

The existing dependency-patch mechanism applies three small LVGL 9.5 fixes:
unsigned little-endian byte assembly, RGB565 transparent-delta preservation, and
respecting the final frame's delay before repeating. The native regression at
`tests/factory-intro/` compares all 60 frames and their timing to independently
decoded Pillow references, and covers restore-background disposal and parent
deletion releasing the GIF timer. No replacement decoder is maintained here.
