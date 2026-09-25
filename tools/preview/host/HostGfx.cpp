#include "HostGfx.h"
#include "Board.h"
#include "fonts/FontSet.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <string>

// ---------------------------------------------------------------------------------------
// pixels
// ---------------------------------------------------------------------------------------

// The screen: PanelGfx::begin() calls init() on it, exactly as it does on a board.
void HostCanvas::initScreen()
{
    createSprite(Board::PANEL_W, Board::PANEL_H);
}

void *HostCanvas::createSprite(int16_t w, int16_t h)
{
    _w = w;
    _h = h;
    _buf.assign((size_t)w * h, 0);
    return _buf.data();
}

void HostCanvas::deleteSprite()
{
    _buf.clear();
    _w = _h = 0;
}

void HostCanvas::drawPixel(int32_t x, int32_t y, uint16_t c)
{
    if (x < 0 || y < 0 || x >= _w || y >= _h) return;
    _buf[(size_t)y * _w + x] = c;
}

void HostCanvas::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (w <= 0 || h <= 0) return;
    for (int32_t j = y; j < y + h; ++j)
        for (int32_t i = x; i < x + w; ++i) drawPixel(i, j, c);
}

void HostCanvas::pushSprite(int32_t x, int32_t y)
{
    if (!_parent) return;
    for (int32_t j = 0; j < _h; ++j)
        for (int32_t i = 0; i < _w; ++i) _parent->drawPixel(x + i, y + j, _buf[(size_t)j * _w + i]);
}

// RGB565 blend, matching Theme::blend()'s intent: alpha 0..255 of `colour` over what is
// already there.
void HostCanvas::blend(int32_t x, int32_t y, uint16_t colour, uint8_t alpha)
{
    if (x < 0 || y < 0 || x >= _w || y >= _h || alpha == 0) return;
    if (alpha == 255) {
        drawPixel(x, y, colour);
        return;
    }
    const uint16_t dst = _blendToBg ? _bg : _buf[(size_t)y * _w + x];
    const int32_t  sr = (colour >> 11) & 0x1F, sg = (colour >> 5) & 0x3F, sb = colour & 0x1F;
    const int32_t  dr = (dst >> 11) & 0x1F, dg = (dst >> 5) & 0x3F, db = dst & 0x1F;
    const int32_t  r = (sr * alpha + dr * (255 - alpha) + 127) / 255;
    const int32_t  g = (sg * alpha + dg * (255 - alpha) + 127) / 255;
    const int32_t  b = (sb * alpha + db * (255 - alpha) + 127) / 255;
    drawPixel(x, y, (uint16_t)((r << 11) | (g << 5) | b));
}

// ---------------------------------------------------------------------------------------
// shapes
// ---------------------------------------------------------------------------------------

void HostCanvas::drawCircle(int32_t x0, int32_t y0, int32_t r, uint16_t c)
{
    int32_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    drawPixel(x0, y0 + r, c);
    drawPixel(x0, y0 - r, c);
    drawPixel(x0 + r, y0, c);
    drawPixel(x0 - r, y0, c);
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++;
        ddF_x += 2;
        f += ddF_x;
        drawPixel(x0 + x, y0 + y, c); drawPixel(x0 - x, y0 + y, c);
        drawPixel(x0 + x, y0 - y, c); drawPixel(x0 - x, y0 - y, c);
        drawPixel(x0 + y, y0 + x, c); drawPixel(x0 - y, y0 + x, c);
        drawPixel(x0 + y, y0 - x, c); drawPixel(x0 - y, y0 - x, c);
    }
}

void HostCanvas::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    if (w <= 0 || h <= 0) return;
    const int32_t rr = std::min<int32_t>(r, std::min(w, h) / 2);
    drawFastHLine(x + rr, y, w - 2 * rr, c);
    drawFastHLine(x + rr, y + h - 1, w - 2 * rr, c);
    drawFastVLine(x, y + rr, h - 2 * rr, c);
    drawFastVLine(x + w - 1, y + rr, h - 2 * rr, c);
    int32_t f = 1 - rr, ddF_x = 1, ddF_y = -2 * rr, cx = 0, cy = rr;
    while (cx < cy) {
        if (f >= 0) { cy--; ddF_y += 2; f += ddF_y; }
        cx++;
        ddF_x += 2;
        f += ddF_x;
        drawPixel(x + w - rr + cx - 1, y + rr - cy, c);
        drawPixel(x + rr - cx, y + rr - cy, c);
        drawPixel(x + w - rr + cy - 1, y + rr - cx, c);
        drawPixel(x + rr - cy, y + rr - cx, c);
        drawPixel(x + w - rr + cx - 1, y + h - rr + cy - 1, c);
        drawPixel(x + rr - cx, y + h - rr + cy - 1, c);
        drawPixel(x + w - rr + cy - 1, y + h - rr + cx - 1, c);
        drawPixel(x + rr - cy, y + h - rr + cx - 1, c);
    }
}

void HostCanvas::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    if (w <= 0 || h <= 0) return;
    const int32_t rr = std::min<int32_t>(r, std::min(w, h) / 2);
    fillRect(x + rr, y, w - 2 * rr, h, c);
    for (int32_t j = 0; j < rr; ++j) {
        // the widest run of pixels whose centre is inside the corner radius
        const double dy   = rr - j - 0.5;
        const int32_t inset = rr - (int32_t)std::floor(std::sqrt((double)rr * rr - dy * dy) + 0.5);
        fillRect(x + inset, y + j, rr - inset, 1, c);
        fillRect(x + w - rr, y + j, rr - inset, 1, c);
        fillRect(x + inset, y + h - 1 - j, rr - inset, 1, c);
        fillRect(x + w - rr, y + h - 1 - j, rr - inset, 1, c);
    }
    fillRect(x, y + rr, rr, h - 2 * rr, c);
    fillRect(x + w - rr, y + rr, rr, h - 2 * rr, c);
}

void HostCanvas::fillSmoothCircle(int32_t cx, int32_t cy, int32_t r, uint16_t fg, uint16_t bg)
{
    const bool saved = _blendToBg;
    const uint16_t savedBg = _bg;
    _blendToBg = true;
    _bg        = bg;
    for (int32_t y = cy - r - 1; y <= cy + r + 1; ++y) {
        for (int32_t x = cx - r - 1; x <= cx + r + 1; ++x) {
            const double d = std::sqrt((double)(x - cx) * (x - cx) + (double)(y - cy) * (y - cy));
            const double cov = std::min(1.0, std::max(0.0, r + 0.5 - d));
            if (cov > 0) blend(x, y, fg, (uint8_t)std::lround(cov * 255));
        }
    }
    _blendToBg = saved;
    _bg        = savedBg;
}

void HostCanvas::drawWideLine(float ax, float ay, float bx, float by, float wd,
                              uint16_t fg, uint16_t bg)
{
    const bool saved = _blendToBg;
    const uint16_t savedBg = _bg;
    _blendToBg = true;
    _bg        = bg;

    const float dx = bx - ax, dy = by - ay;
    const float len2 = dx * dx + dy * dy;
    const float half = wd / 2.0f;
    const int32_t x0 = (int32_t)std::floor(std::min(ax, bx) - half - 1);
    const int32_t x1 = (int32_t)std::ceil(std::max(ax, bx) + half + 1);
    const int32_t y0 = (int32_t)std::floor(std::min(ay, by) - half - 1);
    const int32_t y1 = (int32_t)std::ceil(std::max(ay, by) + half + 1);
    for (int32_t y = y0; y <= y1; ++y) {
        for (int32_t x = x0; x <= x1; ++x) {
            float t = len2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0.0f;
            t = std::min(1.0f, std::max(0.0f, t));
            const float px = ax + t * dx - x, py = ay + t * dy - y;
            const double d = std::sqrt((double)px * px + (double)py * py);
            const double cov = std::min(1.0, std::max(0.0, half + 0.5 - d));
            if (cov > 0) blend(x, y, fg, (uint8_t)std::lround(cov * 255));
        }
    }
    _blendToBg = saved;
    _bg        = savedBg;
}

// ---------------------------------------------------------------------------------------
// text: the same .vlw blob the firmware ships, parsed the way the display library parses it
// ---------------------------------------------------------------------------------------

static int32_t be32(const uint8_t *p)
{
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]);
}

void HostCanvas::loadFont(const uint8_t *blob)
{
    static std::map<const uint8_t *, Font> cache;
    auto it = cache.find(blob);
    if (it == cache.end()) {
        Font f;
        f.blob            = blob;
        const int32_t n   = be32(blob);
        f.pixelSize       = be32(blob + 8);
        f.ascent          = be32(blob + 16);
        f.descent         = be32(blob + 20);
        const uint8_t *rec = blob + 24;
        const uint8_t *bmp = rec + 28 * (size_t)n;
        for (int32_t i = 0; i < n; ++i, rec += 28) {
            Glyph g;
            g.code     = (uint16_t)be32(rec);
            g.h        = be32(rec + 4);
            g.w        = be32(rec + 8);
            g.advance  = be32(rec + 12);
            g.dY       = be32(rec + 16);
            g.dX       = be32(rec + 20);
            g.coverage = bmp;
            bmp += (size_t)g.w * g.h;
            f.glyphs.push_back(g);
        }
        // The blob carries no space glyph, and the library's derived gap is too narrow. The
        // generated metrics for this pixel size hold the real one -- the firmware patches
        // the same value into the library in PanelGfx::useFont().
        f.spaceWidth = f.pixelSize;
        for (const FontMetrics &m : PANEL_FONTS)
            if (m.pixelSize == f.pixelSize) f.spaceWidth = m.spaceWidth;
        it = cache.emplace(blob, std::move(f)).first;
    }
    _font = &it->second;
}

const HostCanvas::Glyph *HostCanvas::glyph(char c) const
{
    if (!_font) return nullptr;
    for (const Glyph &g : _font->glyphs)
        if (g.code == (uint8_t)c) return &g;
    return nullptr;
}

void HostCanvas::drawString(const char *s, int32_t x, int32_t y)
{
    if (!_font || !s) return;
    int32_t cx = x;
    for (; *s; ++s) {
        if (*s == ' ') {
            cx += _font->spaceWidth;
            continue;
        }
        const Glyph *g = glyph(*s);
        if (!g) {
            cx += _font->spaceWidth + 1;
            continue;
        }
        // y is the baseline (L_BASELINE is the only datum the renderer uses); dY is the
        // baseline-to-ink-top distance.
        const int32_t left = cx + g->dX;
        const int32_t top  = y - g->dY;
        for (int32_t j = 0; j < g->h; ++j)
            for (int32_t i = 0; i < g->w; ++i)
                blend(left + i, top + j, _fg, g->coverage[(size_t)j * g->w + i]);
        cx += g->advance;
    }
}

// ---------------------------------------------------------------------------------------
// output
// ---------------------------------------------------------------------------------------

bool HostCanvas::writePpm(const char *path) const
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", (int)_w, (int)_h);
    for (uint16_t p : _buf) {
        // RGB565 -> RGB888 with the top bits replicated, so full-scale stays full-scale
        const uint8_t r = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
        const uint8_t g = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
        const uint8_t b = (uint8_t)((p & 0x1F) * 255 / 31);
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
    return true;
}
