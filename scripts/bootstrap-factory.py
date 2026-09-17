#!/usr/bin/env python3
"""Fetch the factory framework's pinned sources into ignored build storage."""
import concurrent.futures
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "firmware/factory_badge"
DEST = ROOT / ".build/factory-components"

def install(item):
    path = DEST / item["name"]
    if not path.exists():
        if item["name"] == "lvgl":
            # LVGL's demo/test media is large and not part of this application.
            subprocess.run(["git", "clone", "--quiet", "--depth", "1", "--filter=blob:none", "--no-checkout",
                            "--branch", item["ref"], item["url"], str(path)], check=True)
            subprocess.run(["git", "-C", str(path), "sparse-checkout", "set", "src", "include", "env_support"], check=True)
            subprocess.run(["git", "-C", str(path), "checkout"], check=True)
        else:
            subprocess.run(["git", "clone", "--quiet", "--depth", "1", "--branch", item["ref"],
                            "--recurse-submodules", "--shallow-submodules", item["url"], str(path)], check=True)
    actual = subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
    if item["name"] == "lvgl":
        # Its IDF component registers these include directories even with the
        # demos/examples disabled. No demonstration assets are compiled.
        (path / "examples").mkdir(exist_ok=True)
        (path / "demos").mkdir(exist_ok=True)
    expected = item.get("commit")
    if expected and actual != expected:
        raise RuntimeError(f'{item["name"]}: dependency revision mismatch')
    patch = PROJECT / "patches" / (item["name"] + ".patch")
    if patch.exists():
        reverse = subprocess.run(["git", "-C", str(path), "apply", "--reverse", "--check", str(patch)], capture_output=True)
        if reverse.returncode:
            subprocess.run(["git", "-C", str(path), "apply", "--check", str(patch)], check=True)
            subprocess.run(["git", "-C", str(path), "apply", str(patch)], check=True)
    return item["name"], actual

def main():
    DEST.mkdir(parents=True, exist_ok=True)
    deps = json.loads((PROJECT / "frameworks.json").read_text())
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        resolved = dict(pool.map(install, deps))
    print("Factory framework dependencies verified: " + ", ".join(resolved))

if __name__ == "__main__":
    main()
