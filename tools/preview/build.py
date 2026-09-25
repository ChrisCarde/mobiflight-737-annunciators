#!/usr/bin/env python3
"""Build the host preview: the firmware's own panel code, rendering to an image.

    python3 tools/preview/build.py            # both resolutions
    python3 tools/preview/build.py --res 480  # just one

Produces tools/preview/build/preview320 and preview480. Building both is also the layout
gate: every static_assert in the nine panels is evaluated against each screen size, so a
480 layout that does not fit fails here rather than on the board.
"""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
HOST = ROOT / "tools" / "preview" / "host"
BUILD = ROOT / "tools" / "preview" / "build"
ANN = ROOT / "Annunciator"

SOURCES = [
    HOST / "main.cpp",
    HOST / "HostGfx.cpp",
    HOST / "HostStubs.cpp",
    ANN / "PanelGfx.cpp",
    ANN / "Theme.cpp",
    ANN / "LampPanel.cpp",
    ANN / "TouchZones.cpp",
    ANN / "DoorPanel" / "DoorPanel.cpp",
    ANN / "IrsPanel" / "IrsPanel.cpp",
    ANN / "ElecPanel" / "ElecPanel.cpp",
    ANN / "McsPanel" / "McsPanel.cpp",
    ANN / "AirPanel" / "AirPanel.cpp",
    ANN / "FctlPanel" / "FctlPanel.cpp",
    ANN / "IsduPanel" / "IsduPanel.cpp",
]

INCLUDES = [HOST, HOST / "shim", ANN, ANN / "DoorPanel", ANN / "IrsPanel", ANN / "ElecPanel",
            ANN / "McsPanel", ANN / "AirPanel", ANN / "FctlPanel", ANN / "IsduPanel"]


def build(res):
    BUILD.mkdir(parents=True, exist_ok=True)
    out = BUILD / f"preview{res}"
    cmd = ["c++", "-std=c++17", "-O1", "-Wall", "-Wno-unused-function",
           "-DANNUN_BOARD_HOST", f"-DANNUN_PANEL_RES={res}"]
    for inc in INCLUDES:
        cmd += ["-I", str(inc)]
    cmd += [str(s) for s in SOURCES] + ["-o", str(out)]
    print(f"building {out.name} ...")
    result = subprocess.run(cmd, cwd=ROOT)
    return result.returncode == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--res", type=int, choices=[320, 480], help="build only this screen")
    args = ap.parse_args()

    targets = [args.res] if args.res else [320, 480]
    failed = [res for res in targets if not build(res)]
    if failed:
        sys.exit("failed: " + ", ".join(str(r) for r in failed))
    print("ok: " + ", ".join(f"preview{r}" for r in targets))


if __name__ == "__main__":
    main()
