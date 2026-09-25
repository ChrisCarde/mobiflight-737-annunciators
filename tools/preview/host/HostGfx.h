#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <vector>

/* A drawing surface for the host preview.

   HostCanvas implements the contract in Annunciator/Gfx.h -- the subset of the display
   library API that the renderer uses -- over a plain RGB565 buffer, so the real firmware
   sources render to a PNG on a desktop. It is both the screen and an off-screen sprite,
   because the firmware uses the same calls for both.

   What is identical to the board: every position, size and measurement. Layout comes from
   the panel tables and from fonts/FontMetrics.h, which both targets share, and text is
   drawn glyph for glyph out of the same .vlw blob the firmware ships.

   What is only close: anti-aliasing at the pixel level. Rounded corners and smooth circles
   are this file's own implementations rather than TFT_eSPI's, so an edge pixel here and
   there differs from the board. Good enough to judge a layout; not a bit-exact simulator,
   and it should not be described as one. */

#define L_BASELINE 1 // the only text datum the renderer uses

class HostCanvas
{
public:
    explicit HostCanvas(HostCanvas *parent = nullptr) : _parent(parent) {}

    // --- lifecycle -----------------------------------------------------------------
    void  init() { initScreen(); }
    void  initScreen();
    void  setRotation(uint8_t) {} // the host allocates the screen already in landscape
    void  setColorDepth(uint8_t) {}
    void *createSprite(int16_t w, int16_t h);
    void  deleteSprite();
    void  setSize(int16_t w, int16_t h) { createSprite(w, h); } // the screen itself
    int32_t width() const { return _w; }
    int32_t height() const { return _h; }

    // --- fills ---------------------------------------------------------------------
    void fillScreen(uint16_t c) { fillRect(0, 0, _w, _h, c); }
    void fillSprite(uint16_t c) { fillRect(0, 0, _w, _h, c); }
    void drawPixel(int32_t x, int32_t y, uint16_t c);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t c) { fillRect(x, y, w, 1, c); }
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t c) { fillRect(x, y, 1, h, c); }

    // --- outlines ------------------------------------------------------------------
    void drawCircle(int32_t x0, int32_t y0, int32_t r, uint16_t c);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c);
    void fillSmoothCircle(int32_t x, int32_t y, int32_t r, uint16_t fg, uint16_t bg);
    void drawWideLine(float ax, float ay, float bx, float by, float wd, uint16_t fg, uint16_t bg);

    // --- text ----------------------------------------------------------------------
    void loadFont(const uint8_t *blob);
    void unloadFont() { _font = nullptr; }
    void setTextDatum(uint8_t d) { _datum = d; }
    void setTextColor(uint16_t fg) { _fg = fg; _blendToBg = false; }
    void setTextColor(uint16_t fg, uint16_t bg) { _fg = fg; _bg = bg; _blendToBg = true; }
    void drawString(const char *s, int32_t x, int32_t y);

    // --- host only -----------------------------------------------------------------
    void pushSprite(int32_t x, int32_t y);
    bool writePpm(const char *path) const;
    const std::vector<uint16_t> &pixels() const { return _buf; }

private:
    struct Glyph {
        uint16_t       code;
        int32_t        w, h, advance, dY, dX;
        const uint8_t *coverage;
    };
    struct Font {
        const uint8_t     *blob = nullptr;
        std::vector<Glyph> glyphs;
        int32_t            pixelSize = 0, ascent = 0, descent = 0, spaceWidth = 0;
    };

    const Glyph *glyph(char c) const;
    void         blend(int32_t x, int32_t y, uint16_t colour, uint8_t alpha);

    HostCanvas           *_parent = nullptr;
    std::vector<uint16_t> _buf;
    int32_t               _w = 0, _h = 0;
    const Font           *_font = nullptr;
    uint16_t              _fg = 0xFFFF, _bg = 0;
    bool                  _blendToBg = false;
    uint8_t               _datum = L_BASELINE;
};
