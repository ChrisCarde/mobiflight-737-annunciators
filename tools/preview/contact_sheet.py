#!/usr/bin/env python3
"""Assemble panel previews into sheets, and refresh the images the README uses.

    python3 tools/preview/contact_sheet.py                 # one sheet of all ten panels
    python3 tools/preview/contact_sheet.py --scene all
    python3 tools/preview/contact_sheet.py --docs          # refresh docs/previews/
    python3 tools/preview/contact_sheet.py --compare door  # 320 beside 480, true relative size

Reads what render_all.py produced in tools/preview/out. --compare is the view that answers
"are the lamps actually bigger on the 3.5in screen?": the two screens have different pixel
pitches (4.90 and 6.54 px/mm), so it scales each image by its own pitch. Side by side at
equal pixel size, the 480 panel would look misleadingly larger.
"""

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
OUT = ROOT / "tools" / "preview" / "out"
DOCS = ROOT / "docs" / "previews"

PANELS = ["door", "irs", "elec", "mcs_l", "mcs_r", "mcs_both", "mcs_both_afds",
          "air", "fctl", "isdu"]

# Active area / pixel count for each screen. Measure these on the real panels before
# trusting --compare to the millimetre; they come from the standard sizes for a 3.2in
# 240x320 and a 3.5in 320x480 module.
PX_PER_MM = {320: 4.90, 480: 6.54}

BG = (24, 24, 28)
GAP = 8


def load(panel, scene, res):
    path = OUT / f"{panel}-{scene}-{res}.png"
    if not path.exists():
        sys.exit(f"missing {path.relative_to(ROOT)} -- run render_all.py first")
    from PIL import Image
    return Image.open(path)


def sheet(scene, res, columns=3):
    from PIL import Image
    tiles = [load(p, scene, res) for p in PANELS]
    w, h = tiles[0].size
    rows = (len(tiles) + columns - 1) // columns
    out = Image.new("RGB", (columns * w + (columns + 1) * GAP, rows * h + (rows + 1) * GAP), BG)
    for i, tile in enumerate(tiles):
        out.paste(tile, (GAP + (i % columns) * (w + GAP), GAP + (i // columns) * (h + GAP)))
    return out


def compare(panel, scene):
    """The same panel at both sizes, each scaled so a millimetre is a millimetre."""
    from PIL import Image
    small, large = load(panel, scene, 320), load(panel, scene, 480)
    scale = 3.0  # enough magnification to see glyph detail on a screen
    small = small.resize((round(small.width * scale), round(small.height * scale)), Image.NEAREST)
    factor = scale * PX_PER_MM[320] / PX_PER_MM[480]
    large = large.resize((round(large.width * factor), round(large.height * factor)), Image.LANCZOS)
    out = Image.new("RGB", (small.width + large.width + 3 * GAP,
                            max(small.height, large.height) + 2 * GAP), BG)
    out.paste(small, (GAP, GAP))
    out.paste(large, (2 * GAP + small.width, GAP))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--scene", default="demo", choices=["off", "demo", "all", "dim"])
    ap.add_argument("--res", type=int, default=320, choices=[320, 480])
    ap.add_argument("--compare", metavar="PANEL", choices=PANELS)
    ap.add_argument("--docs", action="store_true",
                    help="refresh docs/previews: the ten panels plus the README sheet")
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()

    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        sys.exit("Pillow is missing: .venv/bin/pip install pillow")

    if args.docs:
        DOCS.mkdir(parents=True, exist_ok=True)
        for panel in PANELS:
            load(panel, args.scene, args.res).save(DOCS / f"{panel}.png")
        sheet(args.scene, args.res).save(DOCS / "all-panels.png")
        print(f"{len(PANELS) + 1} images in {DOCS.relative_to(ROOT)}")
        return

    if args.compare:
        image = compare(args.compare, args.scene)
        path = args.out or OUT / f"_compare-{args.compare}-{args.scene}.png"
    else:
        image = sheet(args.scene, args.res)
        path = args.out or OUT / f"_sheet-{args.scene}-{args.res}.png"
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)
    rel = path.resolve()
    print(f"{rel.relative_to(ROOT) if rel.is_relative_to(ROOT) else rel}  "
          f"{image.width}x{image.height}")


if __name__ == "__main__":
    main()
