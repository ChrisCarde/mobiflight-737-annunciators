#!/usr/bin/env python3
"""Render every panel, in every scene, at each screen size, as PNGs.

    python3 tools/preview/render_all.py                  # everything that is built
    python3 tools/preview/render_all.py --res 320
    python3 tools/preview/render_all.py --panel isdu --scene demo

Output lands in tools/preview/out/<panel>-<scene>-<res>.png. Run tools/preview/build.py
first; this only drives the binaries it finds.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD = ROOT / "tools" / "preview" / "build"
OUT = ROOT / "tools" / "preview" / "out"
GOLDEN = ROOT / "tools" / "preview" / "golden"

PANELS = ["door", "irs", "elec", "mcs_l", "mcs_r", "mcs_both", "mcs_both_afds",
          "air", "fctl", "isdu"]
SCENES = ["off", "demo", "all", "dim", "press"]
# The boot splash is not a panel and has scenes of its own: the splash, and what a board
# with no panel shows after it. Rendered with every full run; --panel and --scene leave it
# out unless they name it.
SPLASH_SCENES = ["on", "nopanel"]


def diff_pixels(a, b):
    """How many pixels differ -- the number that says whether a change is a nudge or a rewrite."""
    from PIL import Image, ImageChops
    with Image.open(a) as ia, Image.open(b) as ib:
        if ia.size != ib.size:
            return ia.size[0] * ia.size[1]
        return sum(1 for p in ImageChops.difference(ia.convert("RGB"), ib.convert("RGB"))
                   .getdata() if p != (0, 0, 0))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--res", type=int, choices=[320, 480])
    ap.add_argument("--panel", choices=PANELS + ["splash"])
    ap.add_argument("--scene", choices=SCENES + SPLASH_SCENES)
    ap.add_argument("--check", action="store_true",
                    help="compare against tools/preview/golden and fail on any difference")
    ap.add_argument("--bless", action="store_true",
                    help="accept what was just rendered as the new golden set")
    args = ap.parse_args()

    try:
        from PIL import Image
    except ImportError:
        sys.exit("Pillow is missing: .venv/bin/pip install pillow")

    OUT.mkdir(parents=True, exist_ok=True)
    rendered = []
    resolutions = [args.res] if args.res else [320, 480]
    if args.panel == "splash":
        if args.scene and args.scene not in SPLASH_SCENES:
            sys.exit(f"the splash scenes are {', '.join(SPLASH_SCENES)}")
        targets = [("splash", sc) for sc in ([args.scene] if args.scene else SPLASH_SCENES)]
    else:
        if args.panel and args.scene and args.scene not in SCENES:
            sys.exit(f"the panel scenes are {', '.join(SCENES)}")
        panels = [args.panel] if args.panel else PANELS
        scenes = [args.scene] if args.scene else SCENES
        targets = [(pn, sc) for pn in panels for sc in scenes if sc in SCENES]
        if not args.panel:
            targets += [("splash", sc) for sc in SPLASH_SCENES if args.scene in (None, sc)]

    made = 0
    for res in resolutions:
        binary = BUILD / f"preview{res}"
        if not binary.exists():
            print(f"skipping {res}: {binary.name} not built")
            continue
        for panel, scene in targets:
            ppm = OUT / f"{panel}-{scene}-{res}.ppm"
            run = subprocess.run([str(binary), panel, scene, str(ppm)],
                                 capture_output=True, text=True)
            if run.returncode != 0:
                sys.exit(f"{panel} {scene} {res}: {run.stderr.strip()}")
            png = ppm.with_suffix(".png")
            Image.open(ppm).save(png)
            ppm.unlink()
            made += 1
            if args.check or args.bless:
                rendered.append(png)
    print(f"{made} images in {OUT.relative_to(ROOT)}")

    if args.bless:
        GOLDEN.mkdir(parents=True, exist_ok=True)
        for png in rendered:
            shutil.copyfile(png, GOLDEN / png.name)
        print(f"blessed {len(rendered)} images in {GOLDEN.relative_to(ROOT)}")
    elif args.check:
        differed, absent = [], []
        for png in rendered:
            ref = GOLDEN / png.name
            if not ref.exists():
                absent.append(png.name)
            elif ref.read_bytes() != png.read_bytes():
                differed.append((png.name, diff_pixels(ref, png)))
        for name in absent:
            print(f"  no golden for {name}")
        for name, n in differed:
            print(f"  {name}: {n} pixels differ")
        if differed:
            sys.exit(f"{len(differed)} image(s) changed -- look at them, then --bless if intended")
        print(f"unchanged against {GOLDEN.relative_to(ROOT)}" + (
            f" ({len(absent)} not yet blessed)" if absent else ""))


if __name__ == "__main__":
    main()
