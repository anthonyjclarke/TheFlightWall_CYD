"""Pre-build script: copy the splash JPEG matching the active environment
into data/splash.jpg so the LittleFS image picks up the correct resolution.

Wired in via:
    [env:cyd_320x240]
    extra_scripts = pre:scripts/copy_splash.py

The single data/ dir is shared across environments; whichever env is being
built rewrites data/splash.jpg first. data/splash.jpg is gitignored.
"""
Import("env")  # noqa: F821 — provided by PlatformIO

import shutil
from pathlib import Path

SPLASH_MAP = {
    "cyd_320x240": "assets/branding/splash_320x240.jpg",
    "cyd_480x320": "assets/branding/splash_480x320.jpg",
}

env_name = env["PIOENV"]
project_dir = Path(env["PROJECT_DIR"])

rel_src = SPLASH_MAP.get(env_name)
if rel_src is None:
    print(f"[copy_splash] env={env_name!r} has no splash mapping; skipping")
else:
    src = project_dir / rel_src
    dst = project_dir / "data" / "splash.jpg"
    if not src.exists():
        print(f"[copy_splash] WARNING: source missing: {src}")
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        print(f"[copy_splash] {rel_src} -> data/splash.jpg ({dst.stat().st_size} bytes)")
