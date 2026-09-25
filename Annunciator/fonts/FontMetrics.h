#pragma once

#include <Arduino.h>

/* **********************************************************************************
    Glyph metrics for the embedded DM Sans Bold fonts, generated alongside each .vlw
    blob by tools/make_vlw.py.

    Why carry our own table when the blob already holds these numbers: the renderer used
    to read them straight out of TFT_eSPI (gFont, gdX[], gdY[], gxAdvance[],
    getUnicodeIndex()). Those are library internals, and no other library exposes an
    equivalent -- LovyanGFX's VLWfont has no gdY at all -- so a second display backend
    could not reproduce the layout. Our own table measures text identically on every
    backend and on the host preview, and does it at compile time, which is what lets a
    panel assert that a legend still fits its lamp.

    Everything here reproduces exactly what TFT_eSPI derived from the same blob, quirks
    included -- see textWidthOf(). The fonts are ASCII capitals, digits and a little
    punctuation; there is no space glyph, so a real word gap is carried separately.
********************************************************************************** */

enum PanelFont : uint8_t {
    FONT_SMALL = 0,
    FONT_LARGE,
    FONT_TINY,
    PANEL_FONT_COUNT,
};

struct GlyphMetric {
    uint8_t code;    // ASCII code point; the table is sorted by it
    uint8_t w, h;    // ink box, in pixels
    uint8_t advance; // cursor step
    int8_t  dX;      // ink left edge relative to the cursor (left side bearing)
    int16_t dY;      // baseline up to ink top
};

struct FontMetrics {
    const GlyphMetric *glyphs;
    uint16_t           count;
    uint8_t            pixelSize;
    uint8_t            spaceWidth; // real word gap: the blob carries no space glyph
    uint8_t            capHeight;  // dY of 'H' -- legends are all capitals, so this is
                                   // what gets centred, not the line height
    uint8_t            ascent;
    uint8_t            descent;
};

constexpr const GlyphMetric *findGlyph(const FontMetrics &f, char c)
{
    for (uint16_t i = 0; i < f.count; ++i)
        if (f.glyphs[i].code == (uint8_t)c) return &f.glyphs[i];
    return nullptr;
}

// Horizontal advance of one character, matching what the renderer does when drawing.
// A missing glyph falls back to spaceWidth + 1, which is what TFT_eSPI itself substitutes.
constexpr int16_t advanceOf(const FontMetrics &f, char c)
{
    if (c == ' ') return f.spaceWidth;
    const GlyphMetric *g = findGlyph(f, c);
    return g ? (int16_t)g->advance : (int16_t)(f.spaceWidth + 1);
}

/* Width of a string as TFT_eSPI::textWidth() measures it. Two quirks are deliberate,
   because the panel layouts were tuned against them:
     - a negative left bearing on the FIRST glyph is added back, so the string starts at
       its ink edge rather than overhanging the cursor;
     - the LAST glyph contributes its ink right edge (dX + w), not its advance.
   Between them the result is already a true ink-to-ink extent, which is why inkWidthOf()
   below only has to subtract the leading bearing. */
constexpr int16_t textWidthOf(const FontMetrics &f, const char *s)
{
    int32_t w = 0;
    for (; *s; ++s) {
        if (*s == ' ') {
            w += f.spaceWidth;
            continue;
        }
        const GlyphMetric *g = findGlyph(f, *s);
        if (!g) {
            w += f.spaceWidth + 1;
            continue;
        }
        if (w == 0 && g->dX < 0) w -= g->dX;
        w += s[1] ? (int32_t)g->advance : (int32_t)(g->dX + g->w);
    }
    return (int16_t)w;
}

// Where ink starts relative to the text cursor: the first glyph's left side bearing.
constexpr int16_t inkLeadOf(const FontMetrics &f, const char *s)
{
    if (!s || !*s || *s == ' ') return 0;
    const GlyphMetric *g = findGlyph(f, *s);
    return g ? (int16_t)g->dX : (int16_t)0;
}

// Width of the ink itself, for centring a legend on what you can actually see.
constexpr int16_t inkWidthOf(const FontMetrics &f, const char *s)
{
    const int16_t lead = inkLeadOf(f, s);
    const int16_t tw   = textWidthOf(f, s);
    return lead > 0 ? (int16_t)(tw - lead) : tw;
}

// Width of letter-spaced text -- panel headers are drawn a character at a time, since no
// library has a concept of tracking.
constexpr int16_t spacedWidthOf(const FontMetrics &f, const char *s, int16_t extraPx)
{
    if (!s || !*s) return 0;
    int32_t w = 0;
    for (const char *p = s; *p; ++p) w += advanceOf(f, *p) + extraPx;
    return (int16_t)(w - extraPx);
}
