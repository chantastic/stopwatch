#!/usr/bin/env python3
"""Compile native clock logic and response field builder with host adapters."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / ".build/tests/factory-clock"
OUT.mkdir(parents=True, exist_ok=True)

clock = (ROOT / "firmware/factory_badge/main/clock_service.cpp").read_text()
clock = clock[clock.index("namespace badge_clock {"):]
services = (ROOT / "firmware/factory_badge/main/services.cpp").read_text()
begin = services.index("    ClockSnapshot snapshot;", services.index("  if (clock) {"))
end = services.index("\n  }", begin)
response = services[begin:end]
prefix = (HERE / "fixture-prefix.cpp").read_text().replace("@@ROOT@@", str(ROOT))
tests = (HERE / "fixture-tests.cpp").read_text().replace("// PRODUCTION_CLOCK_RESPONSE_HERE", response)
(OUT / "check.cpp").write_text(prefix + clock + tests)
subprocess.run(["clang++", "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", str(OUT / "check.cpp"), "-o", str(OUT / "check")], check=True)
subprocess.run([str(OUT / "check")], check=True, timeout=30)
