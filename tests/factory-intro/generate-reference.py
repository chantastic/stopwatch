#!/usr/bin/env python3
"""Regenerate native GIF pixel checks using Pillow's independent decoder.

Requires Pillow only when updating the fixture; normal host checks need no Pillow
or original recording. Input is the ignored GIF emitted by generate-intro-loop.py.
"""
import argparse
import hashlib
import io
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent


def fingerprint(image):
    value = 14695981039346656037
    for r, g, b in image.convert("RGB").get_flattened_data():
        pixel = ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)
        value = ((value ^ pixel) * 1099511628211) & ((1 << 64) - 1)
    return value


def reference(image):
    frames, delays = [], []
    for i in range(image.n_frames):
        image.seek(i)
        frames.append(fingerprint(image))
        delays.append(image.info["duration"])
    return frames, delays


def array(name, kind, values, render=str):
    lines = [f"static constexpr {kind} {name}[] = {{"]
    for start in range(0, len(values), 8):
        lines.append("    " + ", ".join(render(v) for v in values[start:start + 8]) + ",")
    return lines + ["};"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gif", type=Path)
    args = parser.parse_args()
    image = Image.open(args.gif)
    assert image.size == (468, 466) and image.n_frames == 60
    frames, delays = reference(image)
    assert sum(delays) == 10000
    # Opaque red, then opaque green with restore-background disposal, then a
    # partial blue corner. This verifies disposal 2 independent of transparent
    # delta frames in the supplied recording, which use keep-frame disposal.
    palette = [0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255] + [0] * 756
    synthetic = []
    for values in [[1] * 16, [2] * 16, [0] * 10 + [3, 3, 0, 0, 3, 3]]:
        im = Image.new("P", (4, 4))
        im.putpalette(palette)
        im.putdata(values)
        synthetic.append(im)
    output = io.BytesIO()
    synthetic[0].save(output, format="GIF", save_all=True, append_images=synthetic[1:],
                      duration=100, loop=0, disposal=[1, 2, 1], transparency=0,
                      background=0, optimize=False)
    fixture = output.getvalue()
    disposal_frames, disposal_delays = reference(Image.open(io.BytesIO(fixture)))
    generated = [
        "// Generated with Pillow's independent GIF decoder; see generate-reference.py.",
        "// GIF SHA-256: " + hashlib.sha256(args.gif.read_bytes()).hexdigest(),
        "#pragma once", "#include <cstdint>",
    ]
    generated += array("IntroFrames", "uint64_t", frames, lambda v: f"0x{v:016x}ULL")
    generated += array("IntroDelays", "uint16_t", delays)
    generated += array("DisposalGif", "uint8_t", fixture, lambda v: f"0x{v:02x}")
    generated += array("DisposalFrames", "uint64_t", disposal_frames, lambda v: f"0x{v:016x}ULL")
    generated += array("DisposalDelays", "uint16_t", disposal_delays)
    (HERE / "reference.h").write_text("\n".join(generated) + "\n")
    print(f"Independent reference: {len(frames)} source frames, {sum(delays)} ms, 3 restore-background fixture frames")


if __name__ == "__main__":
    main()
