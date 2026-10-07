#!/usr/bin/env python3
"""Generate TFT_eSPI smooth (anti-aliased) fonts as PROGMEM C arrays.

TFT_eSPI's smooth fonts use Processing's .vlw layout, normally produced by the Create_font
sketch in the Processing IDE. This does the same job from a TTF with FreeType, so the fonts
can be regenerated without Processing. The output headers are committed, so this is only
needed to change a size or the character set.

    python3 -m venv .venv && .venv/bin/pip install freetype-py
    .venv/bin/python tools/make_vlw.py

Layout, as TFT_eSPI 2.5.x parses it (Extensions/Smooth_font.cpp loadMetrics):
    header   6 x int32 big-endian: glyph count, version (11), size, 0, ascent, descent
    records  7 x int32 big-endian per glyph, sorted by code point:
             code, height, width, xAdvance, dY (baseline up to bitmap top), dX, 0
    bitmaps  8-bit coverage, row-major, top row first, concatenated in record order
    trailer  name/psname (2-byte length each) + smooth flag -- never read by the parser
The parser keeps no per-glyph offsets -- it sums w*h over earlier records -- so one wrong
bitmap length silently shifts every later glyph. Hence the range checks and the independent
re-parse in verify() before anything is written.

Space is never a glyph: TFT_eSPI derives its width as (ascent + descent) * 2 / 7, which is
2px at 11px and makes "DC FAIL" read as "DCFAIL". Each header therefore also exports a real
word gap, applied after loadFont() by PanelGfx::useFont().

Font: DM Sans 9pt Bold, googlefonts/dm-fonts @ d0520ba03bd780f5dccb3024854463d44f699b78
(the commit Google Fonts' METADATA.pb pins for v4.004), fonts/DMSans9pt-Bold.ttf.
Copyright 2014 The DM Sans Project Authors. SIL Open Font License 1.1, see fonts/OFL.txt.
"""

import argparse
import hashlib
import struct
import sys
from pathlib import Path

try:
    import freetype
except ImportError:
    sys.exit("freetype-py is missing: python3 -m venv .venv && .venv/bin/pip install freetype-py")

ROOT = Path(__file__).resolve().parent.parent
TTF = ROOT / "fonts" / "DMSans9pt-Bold.ttf"
TTF_SHA256 = "14c74f014dbe7538cb8dce12eeabcf387f6d63e5ac61bd82a461689560196dc7"
OUT_DIR = ROOT / "Annunciator" / "fonts"

# Every legend and header is capitals; digits and a little punctuation are cheap insurance.
# 'H' must stay in: the renderer takes cap height from it. '_' is for the boot splash's
# credits, which name TFT_eSPI and Arduino_GFX.
CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./:()+_"

# One size triple per screen resolution, in PanelFont order. The renderer names roles, never
# pixel sizes, so a second board is a row here plus a regenerated FontSet header.
#
# 320x240 (E32R32P): 11px door legends and every header -- width-bound, RIGHT FWD is 61px of
# ink against 65 available in a 71px lamp, 66 at 12px. 14px for the IRS lamps, which have far
# more room. 8px for small print, like PUSH TO RESET under MASTER CAUTION -- 61px on a 68px
# face.
#
# 480x320 (C3248W535): x1.36, not x1.5. The 3.5in screen is the same height in millimetres as
# the 3.2in one, so 15px there is the same physical size as 11px here, drawn with 11 rows of
# pixels instead of 8 -- which is the "sharper text" this board is for. Going to 16px would be
# 14% physically larger, a different design rather than the same one rendered better. 11px is
# shared by both sets: the small print on the big screen is the body text on the small one.
FONT_SETS = {
    320: {"FONT_SMALL": 11, "FONT_LARGE": 14, "FONT_TINY": 8},
    480: {"FONT_SMALL": 15, "FONT_LARGE": 19, "FONT_TINY": 11},
}
PANEL_FONT_ORDER = ["FONT_SMALL", "FONT_LARGE", "FONT_TINY"]
SIZES = sorted({px for roles in FONT_SETS.values() for px in roles.values()})

# DM Sans' hinted space is 3px at both sizes; at 14px that reads cramped between bold caps.
WORD_GAP_EM = 0.28


def trim(rows, w, dx, dy):
    """Crop a glyph to its ink box, as Processing does, adjusting the offsets to match."""
    while rows and not any(rows[0]):
        rows.pop(0)
        dy -= 1
    while rows and not any(rows[-1]):
        rows.pop()
    if not rows:
        return [], 0, dx, dy
    left = min(next((i for i, v in enumerate(r) if v), w) for r in rows)
    right = max(max((i for i, v in enumerate(r) if v), default=-1) for r in rows) + 1
    rows = [r[left:right] for r in rows]
    return rows, right - left, dx + left, dy


def load(face, ch):
    face.load_char(ch, freetype.FT_LOAD_DEFAULT | freetype.FT_LOAD_RENDER)
    g = face.glyph
    bm = g.bitmap
    if bm.rows and bm.pixel_mode != freetype.FT_PIXEL_MODE_GRAY:
        sys.exit("glyph %r did not render as 8-bit grey" % ch)
    buf = bytes(bm.buffer)
    rows = [bytearray(buf[r * bm.pitch: r * bm.pitch + bm.width]) for r in range(bm.rows)]
    if g.advance.x % 64:
        sys.exit("glyph %r has a fractional advance -- is the font hinted?" % ch)
    return rows, bm.width, g.bitmap_left, g.bitmap_top, g.advance.x >> 6


def bake(px):
    face = freetype.Face(str(TTF))
    face.set_pixel_sizes(0, px)

    glyphs = []
    for ch in sorted(set(CHARSET), key=ord):
        rows, w, dx, dy, adv = load(face, ch)
        rows, w, dx, dy = trim(rows, w, dx, dy)
        glyphs.append(dict(code=ord(ch), w=w, h=len(rows), adv=adv, dY=dy, dX=dx,
                           bitmap=b"".join(bytes(r) for r in rows)))

    # Processing's convention: ascent is the top of 'd', descent the bottom of 'p'. Neither
    # is embedded. Clamped: a negative descent wraps inside TFT_eSPI's uint16 maxDescent
    # and silently discards every descender.
    rows, w, dx, dy, _ = load(face, "d")
    ascent = max(0, trim(rows, w, dx, dy)[3])
    rows, w, dx, dy, _ = load(face, "p")
    rows, w, dx, dy = trim(rows, w, dx, dy)
    descent = max(0, len(rows) - dy)

    space = max(load(face, " ")[4], round(px * WORD_GAP_EM))
    cap_h = next(g["dY"] for g in glyphs if g["code"] == ord("H"))
    return glyphs, ascent, descent, space, cap_h


def check_ranges(glyphs, ascent, descent):
    """The parser narrows every field with a cast; out-of-range values corrupt silently."""
    codes = [g["code"] for g in glyphs]
    assert codes == sorted(set(codes)), "codes must be unique and sorted"
    assert len(glyphs) <= 0xFFFF
    assert 0 <= ascent <= 255 and 0 <= descent <= 0x7FFF
    for g in glyphs:
        c = chr(g["code"])
        assert 0 <= g["code"] <= 0xFFFF, c
        assert 0 <= g["w"] <= 255 and 0 <= g["h"] <= 255, c        # uint8
        assert 0 <= g["adv"] <= 255, c                              # uint8
        assert -128 <= g["dX"] <= 127, c                            # int8
        assert -32768 <= g["dY"] <= 32767, c                        # int16
        assert len(g["bitmap"]) == g["w"] * g["h"], c


def write_vlw(glyphs, px, ascent, descent, name):
    out = bytearray(struct.pack(">6i", len(glyphs), 11, px, 0, ascent, descent))
    for g in glyphs:
        out += struct.pack(">7i", g["code"], g["h"], g["w"], g["adv"], g["dY"], g["dX"], 0)
    for g in glyphs:
        out += g["bitmap"]
    for s in (name, name.replace(" ", "")):
        b = s.encode("ascii")
        out += struct.pack(">H", len(b)) + b
    out += b"\x01"
    return bytes(out)


def verify(blob, glyphs, ascent, descent):
    """Re-parse the bytes the way Smooth_font.cpp does and compare with what we baked."""
    count, version, _, _, asc, desc = struct.unpack_from(">6i", blob, 0)
    assert (count, version, asc, desc) == (len(glyphs), 11, ascent, descent)
    offset = 24 + 28 * count
    for i, g in enumerate(glyphs):
        code, h, w, adv, dy, dx, pad = struct.unpack_from(">7i", blob, 24 + 28 * i)
        # the same narrowing casts the parser applies
        parsed = (code & 0xFFFF, h & 0xFF, w & 0xFF, adv & 0xFF,
                  struct.unpack("<h", struct.pack("<H", dy & 0xFFFF))[0],
                  struct.unpack("<b", struct.pack("<B", dx & 0xFF))[0])
        assert parsed == (g["code"], g["h"], g["w"], g["adv"], g["dY"], g["dX"]), chr(g["code"])
        assert pad == 0
        assert blob[offset: offset + w * h] == g["bitmap"], chr(g["code"])
        offset += w * h


def c_header(symbol, blob, px, space, cap_h, name):
    guard = symbol.upper()
    lines = [
        "// Generated by tools/make_vlw.py -- do not edit.",
        "//",
        "// %s, %dpx, as a TFT_eSPI smooth font (Processing .vlw layout)." % (name, px),
        "// Source: googlefonts/dm-fonts @ d0520ba, DMSans9pt-Bold.ttf, sha256 %s..." % TTF_SHA256[:16],
        "// Copyright 2014 The DM Sans Project Authors. SIL Open Font License 1.1, see fonts/OFL.txt.",
        "//",
        "// Safe to include anywhere: inline, so the linker keeps exactly one copy in flash",
        "// however many translation units pull it in.",
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "#define %s_SPACE %d      // word gap in px -- the .vlw carries no space glyph" % (guard, space),
        "#define %s_CAP_HEIGHT %d" % (guard, cap_h),
        "",
        "inline constexpr uint8_t %s[] = {" % symbol,
    ]
    for i in range(0, len(blob), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in blob[i:i + 16]) + ",")
    lines += ["};", ""]
    return "\n".join(lines)


def metrics_header(symbol, glyphs, px, space, cap_h, ascent, descent, name):
    """The same numbers as the blob, as a C table the renderer can read at compile time.

    The renderer used to take these from TFT_eSPI's own parsed font (gdX/gdY/gxAdvance).
    No other library exposes an equivalent, so measuring text from our own table is what
    lets a second display backend lay out identically -- and it makes legend widths
    available to static_assert."""
    rows = []
    for g in glyphs:
        rows.append("    {'%s', %3d, %3d, %3d, %3d, %3d}," % (
            chr(g["code"]), g["w"], g["h"], g["adv"], g["dX"], g["dY"]))
    return "\n".join([
        "// Generated by tools/make_vlw.py -- do not edit.",
        "//",
        "// %s glyph metrics: ink box, advance and bearings per character." % name,
        "// Safe to include anywhere -- inline constexpr, so the linker keeps one copy and a",
        "// measurement used only in a static_assert costs no flash at all.",
        "#pragma once",
        "",
        '#include "FontMetrics.h"',
        "",
        "inline constexpr GlyphMetric %s_glyphs[] = {" % symbol,
        "    // code    w    h  adv   dX   dY",
        *rows,
        "};",
        "",
        "inline constexpr FontMetrics %sMetrics = {" % symbol,
        "    %s_glyphs, %d, %d, %d, %d, %d, %d," % (
            symbol, len(glyphs), px, space, cap_h, ascent, descent),
        "};",
        "",
    ])


def font_set_header(res, roles, symbols):
    """Maps PanelFont roles to the metrics for one screen resolution."""
    includes = []
    for r in PANEL_FONT_ORDER:
        includes.append('#include "%s.h"' % symbols[roles[r]])
        includes.append('#include "%sMetrics.h"' % symbols[roles[r]])
    entries = ["    %sMetrics, // %s -- %dpx" % (symbols[roles[r]], r, roles[r])
               for r in PANEL_FONT_ORDER]
    blobs = ["    %s, // %s" % (symbols[roles[r]], r) for r in PANEL_FONT_ORDER]
    return "\n".join([
        "// Generated by tools/make_vlw.py -- do not edit.",
        "//",
        "// The %d-wide panel's font set. Include fonts/FontSet.h rather than this directly:" % res,
        "// it picks the set that matches the board being built.",
        "#pragma once",
        "",
        '#include "FontMetrics.h"',
        *includes,
        "",
        "inline constexpr FontMetrics PANEL_FONTS[PANEL_FONT_COUNT] = {",
        *entries,
        "};",
        "",
        "inline constexpr const FontMetrics &fontOf(PanelFont f) { return PANEL_FONTS[f]; }",
        "",
        "// The font data itself, in the .vlw layout the display libraries parse. Which blob",
        "// backs which role is generated here so no backend has to name a pixel size.",
        "inline constexpr const uint8_t *const PANEL_FONT_BLOBS[PANEL_FONT_COUNT] = {",
        *blobs,
        "};",
        "",
        "inline constexpr const uint8_t *blobOf(PanelFont f) { return PANEL_FONT_BLOBS[f]; }",
        "",
    ])


def emit(path, text, check, stale):
    if check:
        if not path.exists() or path.read_text() != text:
            stale.append(path.name)
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true",
                    help="fail if the committed headers differ from a fresh build")
    args = ap.parse_args()

    if hashlib.sha256(TTF.read_bytes()).hexdigest() != TTF_SHA256:
        sys.exit("%s does not match the pinned DM Sans 9pt Bold release" % TTF)

    stale = []
    symbols = {}
    for px in SIZES:
        glyphs, ascent, descent, space, cap_h = bake(px)
        check_ranges(glyphs, ascent, descent)
        name = "DM Sans Bold %d" % px
        blob = write_vlw(glyphs, px, ascent, descent, name)
        verify(blob, glyphs, ascent, descent)

        symbol = "DMSansBold%d" % px
        symbols[px] = symbol
        wrote = emit(OUT_DIR / (symbol + ".h"),
                     c_header(symbol, blob, px, space, cap_h, name), args.check, stale)
        emit(OUT_DIR / (symbol + "Metrics.h"),
             metrics_header(symbol, glyphs, px, space, cap_h, ascent, descent, name),
             args.check, stale)
        if wrote:
            print("%-18s %3d glyphs  %5d bytes  ascent %d  descent %d  cap %dpx  space %dpx"
                  % (symbol + ".h", len(glyphs), len(blob), ascent, descent, cap_h, space))

    for res, roles in FONT_SETS.items():
        emit(OUT_DIR / ("FontSet%d.h" % res), font_set_header(res, roles, symbols),
             args.check, stale)
        if not args.check:
            print("FontSet%d.h         %s" % (res, ", ".join(
                "%s %dpx" % (r, roles[r]) for r in PANEL_FONT_ORDER)))

    if stale:
        sys.exit("out of date: %s -- rerun tools/make_vlw.py" % ", ".join(stale))


if __name__ == "__main__":
    main()
