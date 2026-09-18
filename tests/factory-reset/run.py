#!/usr/bin/env python3
"""Compile the production reset coordinator against bounded device/NVS doubles."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / ".build/tests/factory-reset"
OUT.mkdir(parents=True, exist_ok=True)
COMPONENTS = Path(os.environ.get("FACTORY_COMPONENTS", ROOT / ".build/factory-components"))
source = (ROOT / "firmware/factory_badge/main/main.cpp").read_text()
production = source[source.index("void persist(uint32_t now)"):source.index("void pollOrientation(uint32_t now)")]
fixture = (HERE / "fixture.cpp").read_text().replace("// PRODUCTION_RESET_COORDINATOR", production)
(OUT / "coordinator.cpp").write_text(fixture)
subprocess.run([
    "clang++", "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined",
    "-fno-omit-frame-pointer", "-DLV_KCONFIG_IGNORE", "-DLV_LVGL_H_INCLUDE_SIMPLE",
    '-DLV_CONF_PATH="' + str(ROOT / "tests/factory-ui/lv_conf.h") + '"',
    "-I" + str(ROOT / "firmware/factory_badge/main"),
    "-I" + str(ROOT / "firmware/devices_badge"),
    "-I" + str(COMPONENTS / "lvgl"),
    OUT / "coordinator.cpp", "-o", OUT / "coordinator"
], cwd=ROOT, check=True)
subprocess.run([OUT / "coordinator"], cwd=ROOT, check=True,
               env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
