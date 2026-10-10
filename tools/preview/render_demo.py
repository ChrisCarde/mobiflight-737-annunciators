#!/usr/bin/env python3
"""Render the boot splash's demo scenes for review, without a board.

Each panel's scene is played by the firmware's own player and panel code (preview demo ...),
exactly as the board plays it, and comes out as:

    tools/preview/out/demo/<panel>-<res>.gif        the scene, 4 frames a second
    tools/preview/out/demo/<panel>-<res>-keys.png   every change, 300 and 800 ms after its step

    .venv/bin/python tools/preview/render_demo.py                 # every panel, both sizes
    .venv/bin/python tools/preview/render_demo.py --panel door --res 480
    .venv/bin/python tools/preview/render_demo.py --check         # against golden/demo.sha256
    .venv/bin/python tools/preview/render_demo.py --bless         # accept what was rendered

The golden is a manifest of keyframe hashes rather than images: there are hundreds of them,
and the GIFs and sheets are what a change is reviewed by.
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "tools" / "preview" / "build"
OUT = ROOT / "tools" / "preview" / "out" / "demo"
MANIFEST = ROOT / "tools" / "preview" / "golden" / "demo.sha256"

# In the order the board plays them (Annunciator/Demo/DemoScenes.cpp).
PANELS = ["door", "irs", "isdu", "elec", "fctl", "air", "mcs_l", "mcs_r", "mcs_both", "mcs_both_afds"]
FRAME_MS = 250


def pixels_sha(img):
    return hashlib.sha256(img.convert("RGB").tobytes()).hexdigest()


def render(binary, panel, res, Image, ImageDraw):
    work = OUT / f"{panel}-{res}"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    run = subprocess.run([str(binary), "demo", panel, str(work), str(FRAME_MS)],
                         capture_output=True, text=True)
    if run.returncode != 0 or run.stderr.strip():
        sys.exit(f"{panel} {res}: {run.stderr.strip() or 'failed'}")

    frames = [Image.open(p).convert("RGB") for p in sorted(work.glob("f*.ppm"))]
    frames[0].save(OUT / f"{panel}-{res}.gif", save_all=True, append_images=frames[1:],
                   duration=FRAME_MS, loop=0)

    # A tap's press, release and answer land within a few hundred ms of each other, so many
    # keyframes repeat the one before: the sheet keeps only those that changed.
    keys, imgs, last = [], [], None
    for path in sorted(work.glob("k*.ppm")):
        img = Image.open(path).convert("RGB")
        sha = pixels_sha(img)
        if sha != last:
            keys.append(path)
            imgs.append(img)
        last = sha
    cols, gap, label = 4, 8, 14
    w, h = imgs[0].size
    rows = (len(imgs) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (w + gap) + gap, rows * (h + label + gap) + gap), (40, 42, 46))
    draw = ImageDraw.Draw(sheet)
    hashes = {}
    for i, (path, img) in enumerate(zip(keys, imgs)):
        x = gap + (i % cols) * (w + gap)
        y = gap + (i // cols) * (h + label + gap)
        ms = int(path.stem[1:])
        draw.text((x, y), f"{ms / 1000:.1f} s", fill=(200, 200, 200))
        sheet.paste(img, (x, y + label))
        hashes[f"{panel}-{res}/{path.stem}"] = pixels_sha(img)
    sheet.save(OUT / f"{panel}-{res}-keys.png")
    shutil.rmtree(work)
    return hashes, len(frames), len(imgs)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--res", type=int, choices=[320, 480])
    ap.add_argument("--panel", choices=PANELS)
    ap.add_argument("--check", action="store_true", help="compare keyframes with the golden manifest")
    ap.add_argument("--bless", action="store_true", help="accept these keyframes as the golden manifest")
    args = ap.parse_args()
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        sys.exit("Pillow is missing: .venv/bin/pip install pillow")

    OUT.mkdir(parents=True, exist_ok=True)
    hashes = {}
    for res in [args.res] if args.res else [320, 480]:
        binary = BUILD / f"preview{res}"
        if not binary.exists():
            sys.exit(f"{binary.name} is not built: run tools/preview/build.py")
        for panel in [args.panel] if args.panel else PANELS:
            got, frames, keys = render(binary, panel, res, Image, ImageDraw)
            hashes.update(got)
            print(f"{panel:14} {res}: {frames} frames, {keys} keyframes")
    print(f"GIFs and keyframe sheets in {OUT.relative_to(ROOT)}")

    golden = {}
    if MANIFEST.exists():
        for line in MANIFEST.read_text().splitlines():
            name, sha = line.split()
            golden[name] = sha
    if args.bless:
        golden.update(hashes)
        MANIFEST.write_text("".join(f"{k} {golden[k]}\n" for k in sorted(golden)))
        print(f"blessed {len(hashes)} keyframes in {MANIFEST.relative_to(ROOT)}")
    elif args.check:
        changed = [k for k in sorted(hashes) if golden.get(k) != hashes[k]]
        gone = [k for k in sorted(golden) if k not in hashes and
                (not args.panel or k.startswith(args.panel + "-")) and
                (not args.res or k.split("/")[0].endswith(str(args.res)))]
        for k in changed:
            print(f"  {k}: {'changed' if k in golden else 'no golden'}")
        for k in gone:
            print(f"  {k}: no longer rendered")
        if changed or gone:
            sys.exit(f"{len(changed) + len(gone)} keyframe(s) differ -- look at the sheets, then --bless")
        print(f"unchanged against {MANIFEST.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
