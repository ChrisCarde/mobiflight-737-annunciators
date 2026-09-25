#include "IsduPanel.h"
#include "IsduLayout.h"
#include "Panel.h"
#include "PanelGfx.h"
#include "Theme.h"
#include "TouchZones.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

namespace IsduPanel
{

using namespace IsduLayout;

/* ---- Colours ---------------------------------------------------------------------
   Photos of real units come out anywhere from cream to orange, depending on the camera's
   white balance -- the panel's white legends take the same cast. The one frame with an
   amber lamp beside the display shows the digits a little paler and yellower than the
   lamp, so that is what this is: amber, towards yellow. */
static const uint32_t SEG_ON     = 0xFFC44E;
static const uint32_t SEG_ON_DIM = 0xB8893A;
static const uint32_t CELL_BG    = 0x0B0E14; // each digit's own dark tube
static const uint32_t CELL_EDGE  = 0x161B24;
static const uint32_t WIN_FILL   = 0x05070B;
static const uint8_t  GHOST      = 16; // unlit segments, just visible

static const uint32_t KEY_CAP    = 0x25282C;
static const uint32_t KEY_EDGE   = 0x3A3E44;
static const uint32_t LEGEND     = 0xDCE1E6; // white panel lighting
static const uint32_t LEGEND_DIM = 0x9AA0A6;
static const uint32_t CUE_ON     = 0xF4F7F1;
static const uint32_t CUE_OFF    = 0x34373C;

static const uint32_t KNOB       = 0x3A3E44;
static const uint32_t KNOB_EDGE  = 0x60666E;

/* ---- State ------------------------------------------------------------------------ */
static char    s_text[13]; // one character per cell, left display then right
static bool    s_dots     = false;
static uint8_t s_sel      = 1; // TK/GS: where the knob rests
static bool    s_sysR     = false;
static bool    s_cue[2]   = {false, false}; // ENT, CLR
static bool    s_dim      = false;
static bool    s_lampTest = false;

// Dirty bits: 13 cells, then these.
static const uint32_t DIRTY_DOTS  = 1u << 13;
static const uint32_t DIRTY_KNOBS = 1u << 14;
static const uint32_t DIRTY_KEYS  = 1u << 15;
static const uint32_t DIRTY_ALL   = 0xFFFFu;
static uint32_t       s_dirty     = 0;

static Gfx::Canvas &cellSpr()
{
    static Gfx::Canvas spr = Gfx::Canvas(&PanelGfx::tft());
    return spr;
}
static Gfx::Canvas &keySpr()
{
    static Gfx::Canvas spr = Gfx::Canvas(&PanelGfx::tft());
    return spr;
}
static bool      s_sprReady = false;
static PanelFont s_keyFont  = FONT_SMALL;

/* ---- Self test ---------------------------------------------------------------------
   From real readouts where there are any: the FCOM's PPOS example, a keyboard longitude
   entry photographed on a real unit, the all-segments test. The heading and speed states
   are laid out as the PMDG fills its display, from the left. */
struct Demo {
    const char *left, *right;
    bool        dots;
    uint8_t     sel;
    bool        ent;
};
static const Demo DEMO[] = {
    {"N47324", "W122123", true, 2, false}, // PPOS, FCOM 11.10.17
    {"285", "452", false, 1, false},       // TK/GS
    {"", "W171580", true, 2, true},        // keyboard longitude entry, ENT cue lit
    {"341", "15", false, 4, false},        // HDG/STS during alignment: 15 minutes to go
    {"888888", "8888888", true, 0, false}, // TEST
};
static const uint8_t  DEMO_COUNT    = sizeof(DEMO) / sizeof(DEMO[0]);
static const uint32_t DEMO_DWELL_MS = 2500;
static bool           s_selfTest    = false;
static uint8_t        s_demoIndex   = 0;
static uint32_t       s_demoAt      = 0;

/* ---- Touch: the keypad and the knobs ---------------------------------------------
   Keys first, then DSPL SEL's five legends, then SYS DSPL's two. The legends' rectangles
   depend on the text, so they are measured at init. */
static const uint8_t ZONE_SEL = KEYS, ZONE_SYS = KEYS + SEL_POSITIONS;
static const uint8_t ZONE_COUNT = ZONE_SYS + 2;
static const char *const ZONE_NAME[ZONE_COUNT] = {
    "ISDU 1", "ISDU 2", "ISDU 3", "ISDU 4", "ISDU 5", "ISDU 6",
    "ISDU 7", "ISDU 8", "ISDU 9", "ISDU ENT", "ISDU 0", "ISDU CLR",
    "ISDU DSPL TEST", "ISDU DSPL TK GS", "ISDU DSPL PPOS", "ISDU DSPL WIND", "ISDU DSPL HDG STS",
    "ISDU SYS DSPL L", "ISDU SYS DSPL R",
};
static TouchZones::Zone s_zones[ZONE_COUNT];
static int8_t           s_pressedLegend = -1; // zone index, shown lit while held

// Where a knob legend goes: left of the knob it ends at its anchor, right of it it starts
// there, above it is centred on it. Shared by the drawing and the touch rectangles.
static void legendBox(int16_t cx, int16_t cy, int16_t r, int16_t angle, const char *text,
                      int16_t &x, int16_t &yMid, int16_t &w)
{
    const float   a  = angle * (float)M_PI / 180.0f;
    const int16_t ax = (int16_t)lroundf(cx + r * cosf(a));
    const float   c  = cosf(a);
    w    = PanelGfx::spacedTextWidth(text, 1);
    yMid = (int16_t)lroundf(cy - r * sinf(a));
    x    = c < -0.3f ? (int16_t)(ax - w) : c > 0.3f ? ax : (int16_t)(ax - w / 2);
}

static void buildZones()
{
    for (uint8_t k = 0; k < KEYS; ++k) s_zones[k] = {keyX(k), keyY(k), KEY_W, KEY_H, ZONE_NAME[k]};
    int16_t x, y, w;
    for (uint8_t p = 0; p < SEL_POSITIONS; ++p) {
        legendBox(SEL_CX, SEL_CY, SEL_LEGEND_R, SEL_ANGLE[p], SEL_NAME[p], x, y, w);
        s_zones[ZONE_SEL + p] = {(int16_t)(x - 1), (int16_t)(y - 6), (int16_t)(w + 2), 13, ZONE_NAME[ZONE_SEL + p]};
    }
    for (uint8_t p = 0; p < 2; ++p) {
        legendBox(SYS_CX, SYS_CY, SYS_LEGEND_R, SYS_ANGLE[p], SYS_NAME[p], x, y, w);
        s_zones[ZONE_SYS + p] = {(int16_t)(x - 4), (int16_t)(y - 6), (int16_t)(w + 8), 13, ZONE_NAME[ZONE_SYS + p]};
    }
}

// A press round a knob picks the position nearest its direction from the centre.
static int8_t resolve(int16_t x, int16_t y)
{
    int32_t dx = x - SEL_CX, dy = SEL_CY - y;
    if (dx * dx + dy * dy <= (int32_t)SEL_REACH * SEL_REACH && dy >= -SEL_R) {
        float a = atan2f((float)dy, (float)dx) * 180.0f / (float)M_PI;
        if (a < -90.0f) a += 360.0f; // just below the left side still counts as TEST
        uint8_t best = 0;
        for (uint8_t p = 1; p < SEL_POSITIONS; ++p)
            if (fabsf(a - SEL_ANGLE[p]) < fabsf(a - SEL_ANGLE[best])) best = p;
        return (int8_t)(ZONE_SEL + best);
    }
    dx = x - SYS_CX;
    dy = SYS_CY - y;
    if (dx * dx + dy * dy <= (int32_t)SYS_REACH * SYS_REACH && dy >= -SYS_R)
        return (int8_t)(ZONE_SYS + (dx >= 0 ? 1 : 0));
    return -1;
}

// Legends show a press by lighting up; a ring round one would clip its neighbours.
static bool feedback(int8_t zone, bool pressed)
{
    if (zone < (int8_t)ZONE_SEL) return false; // keys: the ring
    s_pressedLegend = pressed ? zone : -1;
    s_dirty |= DIRTY_KNOBS;
    return true;
}

static const TouchZones::Hooks HOOKS = {resolve, feedback};

/* ---- Drawing ---------------------------------------------------------------------- */
static uint32_t segColour() { return s_dim ? SEG_ON_DIM : SEG_ON; }

static void drawSegments(Gfx::Canvas &spr, uint16_t segs, uint32_t colour)
{
    const uint16_t c = Theme::to565(colour);
    for (uint8_t s = 0; s < 7; ++s) {
        if (!(segs & (1u << s))) continue;
        const Rect &r = SEG_RECT[s];
        spr.fillRoundRect(GLYPH_X + r.x, GLYPH_Y + r.y, r.w, r.h, 1, c);
    }
    for (uint8_t s = 0; s < 3; ++s) {
        if (!(segs & (1u << (7 + s)))) continue;
        const Line &l = SEG_LINE[s];
        Gfx::drawWideLine(spr, GLYPH_X + l.x0, GLYPH_Y + l.y0, GLYPH_X + l.x1, GLYPH_Y + l.y1,
                         SEG_T, c, Theme::to565(CELL_BG));
    }
}

static void drawCell(uint8_t i)
{
    Gfx::Canvas &spr = cellSpr();
    spr.fillSprite(Theme::to565(WIN_FILL));
    spr.fillRoundRect(0, 0, CELL_W, CELL_H, 2, Theme::to565(CELL_BG));
    spr.drawRoundRect(0, 0, CELL_W, CELL_H, 2, Theme::to565(CELL_EDGE));

    const uint16_t has = cellSegments(i);
    drawSegments(spr, has, Theme::blend(segColour(), CELL_BG, GHOST));
    const uint16_t lit = s_lampTest ? has : glyph(s_text[i]);
    drawSegments(spr, lit, segColour());

    PanelGfx::push(spr, cellX(i), CELL_Y);
}

static void drawDots()
{
    Gfx::Device      &t  = PanelGfx::tft();
    const uint32_t on = (s_dots || s_lampTest) ? segColour() : Theme::blend(segColour(), WIN_FILL, GHOST + 6);
    for (uint8_t d = 0; d < 6; ++d)
        Gfx::fillSmoothCircle(t, DOTS[d].x, DOTS[d].y, DOT_R, Theme::to565(on), Theme::to565(WIN_FILL));
}

static void drawKey(uint8_t k)
{
    Gfx::Canvas &spr = keySpr();
    LampStyle    st  = {KEY_CAP, s_dim ? LEGEND_DIM : LEGEND, KEY_EDGE, false, false, KIND_LENS};

    const bool cue = (k == KEY_ENT || k == KEY_CLR);
    LegendLine lines[2];
    uint8_t    n = 0;
    lines[n++]   = {KEY_TOP[k], FONT_SMALL};
    if (*KEY_BOT[k]) lines[n++] = {KEY_BOT[k], FONT_LARGE};
    // ENT and CLR: an invisible line of small print under the legend lifts it clear of the
    // cue lights, which go where that line would be.
    if (cue) lines[n++] = {" ", FONT_TINY};
    PanelGfx::renderLampLines(spr, st, lines, n, s_keyFont);

    if (cue) {
        const bool     lit = s_lampTest || s_cue[k == KEY_ENT ? 0 : 1];
        const uint16_t c   = Theme::to565(lit ? CUE_ON : CUE_OFF);
        Gfx::fillSmoothCircle(spr, KEY_W / 2 - CUE_DX, CUE_DY, CUE_R, c, Theme::to565(KEY_CAP));
        Gfx::fillSmoothCircle(spr, KEY_W / 2 + CUE_DX, CUE_DY, CUE_R, c, Theme::to565(KEY_CAP));
    }
    PanelGfx::push(spr, keyX(k), keyY(k));
}

static void knobLegend(int16_t cx, int16_t cy, int16_t r, int16_t angle, const char *text,
                       bool active, bool pressed)
{
    int16_t x, yMid, w;
    legendBox(cx, cy, r, angle, text, x, yMid, w);
    const uint32_t colour = pressed ? segColour()
                          : active  ? (s_dim ? LEGEND_DIM : LEGEND)
                                    : Theme::blend(Theme::Placard, Theme::PanelBg, 110);
    PanelGfx::drawSpacedText(x, yMid, text, colour, 1);
}

static void knob(int16_t cx, int16_t cy, int16_t r, int16_t angle)
{
    Gfx::Device &t = PanelGfx::tft();
    Gfx::fillSmoothCircle(t, cx, cy, r, Theme::to565(KNOB_EDGE), Theme::to565(Theme::PanelBg));
    Gfx::fillSmoothCircle(t, cx, cy, r - 1, Theme::to565(KNOB), Theme::to565(KNOB_EDGE));
    const float a = angle * (float)M_PI / 180.0f;
    Gfx::drawWideLine(t, cx, cy, cx + (r - 2) * cosf(a), cy - (r - 2) * sinf(a), 3,
                   Theme::to565(s_dim ? LEGEND_DIM : LEGEND), Theme::to565(KNOB));
}

static void centred(const char *text, int16_t cx, int16_t yMid, uint32_t colour)
{
    PanelGfx::drawSpacedText((int16_t)(cx - PanelGfx::spacedTextWidth(text, 2) / 2), yMid, text, colour, 2);
}

static void drawKnobs()
{
    PanelGfx::clearRect(KNOBS_X, LOWER_Y, KNOBS_W, LOWER_H);
    const uint8_t sel = s_sel < SEL_POSITIONS ? s_sel : 1;

    centred("DSPL SEL", SEL_CX, SEL_TITLE_Y, Theme::HeaderFg);
    for (uint8_t p = 0; p < SEL_POSITIONS; ++p)
        knobLegend(SEL_CX, SEL_CY, SEL_LEGEND_R, SEL_ANGLE[p], SEL_NAME[p], s_lampTest || p == sel,
                   s_pressedLegend == ZONE_SEL + p);
    knob(SEL_CX, SEL_CY, SEL_R, SEL_ANGLE[sel]);

    centred("SYS DSPL", SYS_CX, SYS_TITLE_Y, Theme::HeaderFg);
    for (uint8_t p = 0; p < 2; ++p)
        knobLegend(SYS_CX, SYS_CY, SYS_LEGEND_R, SYS_ANGLE[p], SYS_NAME[p], s_lampTest || p == (s_sysR ? 1 : 0),
                   s_pressedLegend == ZONE_SYS + p);
    knob(SYS_CX, SYS_CY, SYS_R, SYS_ANGLE[s_sysR ? 1 : 0]);
}

static void drawChrome()
{
    PanelGfx::clearPanel();
    Gfx::Device &t = PanelGfx::tft();

    // The placard, with rules either side as on the real unit's face.
    const int16_t w  = PanelGfx::spacedTextWidth("IRS DISPLAY", 2);
    const int16_t tx = (int16_t)((PANEL_W - w) / 2);
    PanelGfx::drawSpacedText(tx, HEADER_Y, "IRS DISPLAY", Theme::HeaderFg, 2);
    PanelGfx::drawRule(WIN_X, HEADER_Y, tx - 8 - WIN_X, Theme::Rule);
    PanelGfx::drawRule(tx + w + 8, HEADER_Y, WIN_X + WIN_W - (tx + w + 8), Theme::Rule);

    t.fillRoundRect(WIN_X, WIN_Y, WIN_W, WIN_H, 5, Theme::to565(WIN_FILL));
    t.drawRoundRect(WIN_X, WIN_Y, WIN_W, WIN_H, 5, Theme::to565(Theme::LedFrame));
    // The divider between the two displays.
    const int16_t dx = (int16_t)((cellX(LEFT_CELLS - 1) + CELL_W + RIGHT_X) / 2);
    t.drawFastVLine(dx, WIN_Y + 6, WIN_H - 12, Theme::to565(Theme::LedFrame));

    TouchZones::screenCleared();
    s_dirty = DIRTY_ALL;
}

/* ---- Messages --------------------------------------------------------------------- */
// Character n of the string goes in cell n of the display, as the PMDG shows it; a short
// string leaves the rest blank, and an empty one (or none -- MobiFlight sends nothing for
// an empty string) blanks the display.
static void setText(uint8_t first, uint8_t cells, const char *s)
{
    for (uint8_t i = 0; i < cells; ++i) {
        char c = (s && *s) ? *s++ : ' ';
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (s_text[first + i] != c) {
            s_text[first + i] = c;
            s_dirty |= 1u << (first + i);
        }
    }
}

static void applyDemo(uint8_t i)
{
    const Demo &d = DEMO[i];
    setText(0, LEFT_CELLS, d.left);
    setText(LEFT_CELLS, RIGHT_CELLS, d.right);
    s_dots   = d.dots;
    s_sel    = d.sel;
    s_cue[0] = d.ent;
    s_cue[1] = false;
    s_dirty |= DIRTY_DOTS | DIRTY_KNOBS | DIRTY_KEYS;
}

static void clearState()
{
    memset(s_text, ' ', sizeof(s_text));
    s_dots = false;
    s_sel  = 1;
    s_sysR = false;
    s_cue[0] = s_cue[1] = false;
    s_dirty = DIRTY_ALL;
}

void clear()
{
    clearState();
    s_dim      = false;
    s_lampTest = false;
    s_selfTest = false;
}

void init(uint8_t backlightPin)
{
    PanelGfx::begin(backlightPin);
    clear();

    if (!s_sprReady) {
        cellSpr().setColorDepth(16);
        keySpr().setColorDepth(16);
        s_sprReady = cellSpr().createSprite(CELL_W, CELL_H) != nullptr &&
                     keySpr().createSprite(KEY_W, KEY_H) != nullptr;
        if (s_sprReady) {
            PanelGfx::useFont(keySpr(), FONT_SMALL);
            s_keyFont = FONT_SMALL;
        }
    }
    buildZones();
    TouchZones::attach(s_zones, ZONE_COUNT, &HOOKS);
    drawChrome();
}

void stop()
{
    TouchZones::detach();
    if (s_sprReady) {
        keySpr().unloadFont();
        keySpr().deleteSprite();
        cellSpr().deleteSprite();
        s_sprReady = false;
    }
}

void set(int16_t messageID, char *payload)
{
    // Real traffic from MobiFlight ends a running self test.
    if (s_selfTest && messageID != MSG_SELF_TEST) {
        s_selfTest = false;
        clearState();
    }

    const int v = payload ? atoi(payload) : 0;
    switch (messageID) {
    case MSG_LEFT:
        setText(0, LEFT_CELLS, payload);
        break;
    case MSG_RIGHT:
        setText(LEFT_CELLS, RIGHT_CELLS, payload);
        break;
    case MSG_DOTS:
        s_dots = v != 0;
        s_dirty |= DIRTY_DOTS;
        break;
    case MSG_DSPL_SEL:
        s_sel = (uint8_t)constrain(v, 0, SEL_POSITIONS - 1);
        s_dirty |= DIRTY_KNOBS;
        break;
    case MSG_SYS_DSPL:
        s_sysR = v != 0;
        s_dirty |= DIRTY_KNOBS;
        break;
    case MSG_ENT_CUE:
    case MSG_CLR_CUE:
        s_cue[messageID == MSG_ENT_CUE ? 0 : 1] = payload && atof(payload) != 0;
        s_dirty |= DIRTY_KEYS;
        break;

    case MSG_ROTATION:
        PanelGfx::setRotation(payload ? (uint8_t)v : 1);
        drawChrome();
        break;
    case MSG_DIM:
        s_dim   = v != 0;
        s_dirty = DIRTY_ALL;
        break;
    case MSG_LAMP_TEST:
        s_lampTest = v != 0;
        s_dirty    = DIRTY_ALL;
        break;
    case MSG_SELF_TEST:
        if (v != 0) {
            s_selfTest  = true;
            s_demoIndex = 0;
            s_demoAt    = millis();
            applyDemo(0);
        }
        break;
    case MSG_TOUCH_CAL:
        if (v != 0) TouchZones::startCalibration();
        break;
    default:
        break;
    }
}

void update()
{
    if (!s_sprReady) return;

    bool redraw;
    if (TouchZones::poll(redraw)) return; // calibrating: leave the screen alone
    if (redraw) drawChrome();

    const uint32_t now = millis();
    if (s_selfTest && (now - s_demoAt) >= DEMO_DWELL_MS) {
        s_demoAt    = now;
        s_demoIndex = (uint8_t)((s_demoIndex + 1) % DEMO_COUNT);
        applyDemo(s_demoIndex);
    }

    // Keeps drawing while dark, so the panel reappears instantly showing current state.
    if (!s_dirty) return;
    for (uint8_t i = 0; i < LEFT_CELLS + RIGHT_CELLS; ++i)
        if (s_dirty & (1u << i)) drawCell(i);
    if (s_dirty & DIRTY_DOTS) drawDots();
    if (s_dirty & DIRTY_KNOBS) drawKnobs();
    if (s_dirty & DIRTY_KEYS)
        for (uint8_t k = 0; k < KEYS; ++k) drawKey(k);
    s_dirty = 0;
}

} // namespace IsduPanel
