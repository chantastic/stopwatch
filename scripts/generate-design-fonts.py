#!/usr/bin/env python3
"""Regenerate the badge's LVGL fonts from checksum-verified upstream sources."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import urllib.request
import zipfile


ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / ".build/design-fonts"
UI = ROOT / "firmware/factory_badge/main/ui"
VERSION = "1.5.3"
ROBOTO_REV = "895ec691990d041dd727c7b5afa3ce56525d98e6"
ROBOTO_URL = f"https://raw.githubusercontent.com/googlefonts/RobotoMono/{ROBOTO_REV}"
INPUTS = {
    "RobotoMono-Regular.ttf": (
        f"{ROBOTO_URL}/fonts/ttf/RobotoMono-Regular.ttf",
        "af0bff7599c3df3831755c16e39b3c496df74b8c8d8a1161b14dc8461be17cb4",
    ),
    "RobotoMono-OFL.txt": (
        f"{ROBOTO_URL}/OFL.txt",
        "50ab8dd54680d3473f649c9db86fece88434d097c7834475c1c72d2f8c429215",
    ),
    "Inter-4.1.zip": (
        "https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip",
        "9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e",
    ),
}
INTER_FILES = {
    "extras/ttf/Inter-Regular.ttf": (
        "Inter-Regular.ttf",
        "40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82",
    ),
    "LICENSE.txt": (
        "Inter-OFL.txt",
        "262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a",
    ),
}


def verify(data, expected, label):
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected:
        raise SystemExit(f"SHA-256 mismatch for {label}: {actual}")


def download(name, url, digest):
    target = CACHE / name
    if target.exists():
        data = target.read_bytes()
    else:
        print(f"Downloading {name}", flush=True)
        with urllib.request.urlopen(url, timeout=120) as response:
            data = response.read()
    verify(data, digest, name)
    target.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--converter", type=Path,
        help=f"Use an installed lv_font_conv.js (must be version {VERSION}).",
    )
    args = parser.parse_args()
    if args.converter:
        command = ["node", str(args.converter.resolve())]
    else:
        command = ["npx", "--yes", f"--package=lv_font_conv@{VERSION}", "lv_font_conv"]
    version = subprocess.check_output(command + ["--version"], cwd=ROOT, text=True).strip()
    if version != VERSION:
        raise SystemExit(f"Expected lv_font_conv {VERSION}, got {version}")

    CACHE.mkdir(parents=True, exist_ok=True)
    for name, (url, digest) in INPUTS.items():
        download(name, url, digest)
    with zipfile.ZipFile(CACHE / "Inter-4.1.zip") as archive:
        for entry, (name, digest) in INTER_FILES.items():
            data = archive.read(entry)
            verify(data, digest, entry)
            (CACHE / name).write_bytes(data)

    licenses = UI / "fonts"
    licenses.mkdir(parents=True, exist_ok=True)
    for name in ("RobotoMono-OFL.txt", "Inter-OFL.txt"):
        # Preserve the license text while removing upstream trailing spaces.
        (licenses / name).write_text("\n".join(
            line.rstrip() for line in (CACHE / name).read_text().splitlines()
        ).rstrip() + "\n")

    for family, source, sizes in (
        ("mono", "RobotoMono-Regular.ttf", (12, 18, 20, 24)),
        ("sans", "Inter-Regular.ttf", (14, 16, 20, 24, 32)),
    ):
        for size in sizes:
            name = f"font_{family}_{size}"
            output = UI / f"{name}.c"
            subprocess.run(command + [
                "--font", str((CACHE / source).relative_to(ROOT)),
                "--range", "0x20-0x7e,0xa0-0xff",
                "--size", str(size), "--bpp", "4", "--format", "lvgl",
                "--no-compress", "--lv-include", "lvgl.h",
                "--lv-font-name", name,
                "--output", str(output.relative_to(ROOT)),
            ], cwd=ROOT, check=True)
            notice = (
                "/* SPDX-License-Identifier: OFL-1.1\n"
                " * Derived font data; upstream copyright and license: "
                f"fonts/{'RobotoMono' if family == 'mono' else 'Inter'}-OFL.txt.\n"
                " * Regenerate with scripts/generate-design-fonts.py.\n */\n\n"
            )
            output.write_text(notice + output.read_text().rstrip() + "\n")
            print(f"Generated {name}", flush=True)


if __name__ == "__main__":
    main()
