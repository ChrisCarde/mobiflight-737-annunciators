#include "ElecPanel.h"
#include "Layout.h"
#include "Font5x7.h"
#include "Panel.h"
#include "PanelGfx.h"
#include "Theme.h"
#include <string.h>
#include <stdlib.h>

namespace ElecPanel
{

/* ---- Geometry -------------------------------------------------------------------
   Follows the upper half of the real panel: placards above and below a wide LED window,
   then the three lamps in a row. The window is the full 300px content width: 12 characters
   of 5x7 dots on a 4px pitch (3px dots) are 284px across and 28px tall, and the two lines
   sit a character-height apart as on the real display. Each 4-character field is centred
   under its placard.
---------------------------------------------------------------------------------- */
static const int16_t CONTENT_X = LY::MARGIN;
static const int16_t CONTENT_W = LY::PANEL_W - 2 * LY::MARGIN; // 300 / 452

/* The LED window is the one element here that is genuinely resolution-limited: a 5x7 dot
   matrix drawn with 3px dots on a 4px pitch. The larger screen takes the full x1.5 -- 4px
   dots with 2px between them -- which is the biggest visible gain anywhere in the port. */
#if ANNUN_PANEL_RES == 480
static const int16_t DOT_PITCH = 6, DOT_SIZE = 4;
#else
static const int16_t DOT_PITCH = 4, DOT_SIZE = 3;
#endif
static const int16_t CHARS     = 12, FIELD_CHARS = 4;
static const int16_t CHAR_W    = 5 * DOT_PITCH;             // 20
static const int16_t CHAR_STEP = CHAR_W + DOT_PITCH;        // 24
static const int16_t LINE_W    = CHARS * CHAR_STEP - DOT_PITCH; // 284
static const int16_t LINE_H    = 7 * DOT_PITCH;             // 28
static const int16_t FIELD_W   = FIELD_CHARS * CHAR_STEP;   // 96

#if ANNUN_PANEL_RES == 480
static const int16_t TOP_LABEL_Y = 11, LABEL_H = 16;
static const int16_t WIN_Y = 32, WIN_H = 155;
static const int16_t LINE_GAP = 37;
static const int16_t LABEL_GAP = 4, LAMP_GAP_Y = 12, SEL_GAP = 16;
static const int16_t LAMP_W = 107, LAMP_H = 45, LAMP_GAP = 13;
static const int16_t SEL_H = 16;
#else
static const int16_t TOP_LABEL_Y = 8, LABEL_H = 12;
static const int16_t WIN_Y = 24, WIN_H = 116;
static const int16_t LINE_GAP = 28;
static const int16_t LABEL_GAP = 3, LAMP_GAP_Y = 9, SEL_GAP = 12;
static const int16_t LAMP_W = 80, LAMP_H = 34, LAMP_GAP = 10;
static const int16_t SEL_H = 12;
#endif

static const int16_t LINE_X      = CONTENT_X + (CONTENT_W - LINE_W) / 2;        // 18
static const int16_t LINE_Y0     = WIN_Y + (WIN_H - (2 * LINE_H + LINE_GAP)) / 2; // 40
static const int16_t LINE_Y1     = LINE_Y0 + LINE_H + LINE_GAP;                  // 96
static const int16_t BOT_LABEL_Y = WIN_Y + WIN_H + LABEL_GAP;                    // 143 / 191

static const int16_t LAMP_Y = BOT_LABEL_Y + LABEL_H + LAMP_GAP_Y;                // 164 / 219
static const int16_t LAMP_X = CONTENT_X + (CONTENT_W - (3 * LAMP_W + 2 * LAMP_GAP)) / 2; // 30 / 66

static const int16_t SEL_Y = LAMP_Y + LAMP_H + SEL_GAP;                          // 210 / 280

static const int16_t PANEL_W = LY::PANEL_W, PANEL_H = LY::PANEL_H;
static_assert(LINE_X >= CONTENT_X + 2 && LINE_X + LINE_W <= CONTENT_X + CONTENT_W - 2,
              "LED lines do not fit inside the window");
static_assert(LINE_Y0 >= WIN_Y + 2 && LINE_Y1 + LINE_H <= WIN_Y + WIN_H - 2,
              "LED lines do not fit inside the window");
static_assert(LAMP_X >= CONTENT_X, "meter lamps are wider than the panel");
static_assert(SEL_Y + SEL_H <= PANEL_H - 4, "selector row runs off the panel");
static_assert(CONTENT_X + CONTENT_W <= PANEL_W, "meter content is wider than the panel");
static_assert(LY::fits(FONT_SMALL, "DISCHARGE", LAMP_W, 3), "meter lamp too narrow for DISCHARGE");

static int16_t fieldCentreX(uint8_t field) { return LINE_X + field * FIELD_W + (FIELD_W - DOT_PITCH) / 2; }

struct Placard {
    const char *text;
    uint8_t     field;
};
static const Placard TOP_PLACARDS[] = {{"DC AMPS", 0}, {"CPS FREQ", 2}};
static const Placard BOTTOM_PLACARDS[] = {{"DC VOLTS", 0}, {"AC AMPS", 1}, {"AC VOLTS", 2}};

static const char *LAMP_L1[LAMP_COUNT] = {"BAT", "TR UNIT", "ELEC"};
static const char *LAMP_L2[LAMP_COUNT] = {"DISCHARGE", "", ""};

/* Selector positions as the placards around the real knobs read them. The order matches the
   PMDG SDK's ELEC_DCMeterSelector / ELEC_ACMeterSelector (0 = STBY PWR). */
static const char   *DC_POSITIONS[] = {"STBY PWR", "BAT BUS", "BAT", "AUX BAT", "TR1", "TR2", "TR3", "TEST"};
static const char   *AC_POSITIONS[] = {"STBY PWR", "GRD PWR", "GEN1", "APU GEN", "GEN2", "INV", "TEST"};
static const int8_t  DC_COUNT       = sizeof(DC_POSITIONS) / sizeof(DC_POSITIONS[0]);
static const int8_t  AC_COUNT       = sizeof(AC_POSITIONS) / sizeof(AC_POSITIONS[0]);

/* Demo states for the self test. The first two lines are the readings in reference photos of
   the real panel; the rest are plausible states for each lamp. */
struct ElecDemo {
    const char *top;
    const char *bottom;
    uint8_t     lamps; // bit per lamp
    int8_t      dc, ac;
};
static const ElecDemo DEMO[] = {
    {"   0     400", "  28  49 115", 0, 2, 0},        // b737.org.uk photo
    {"  20     400", "  28  45 115", 0, 4, 1},        // ground power, TR1 / GRD PWR
    {"  10     400", "  24   2 115", 1u << 0, 2, 0},  // battery only: BAT DISCHARGE
    {"   0     400", "   0  60 115", 1u << 1, 6, 4},  // TR3 failed: TR UNIT
    {"  22     400", "  28  58 115", 1u << 2, 5, 3},  // on the ground with a fault: ELEC
};
static const uint8_t  DEMO_COUNT    = sizeof(DEMO) / sizeof(DEMO[0]);
static const uint32_t DEMO_DWELL_MS = 2500;

// Constructed on first use rather than at static-init time: PanelGfx owns the TFT_eSPI
// instance in another translation unit and its construction order is not guaranteed.
static Gfx::Canvas &s_lineSpr()
{
    static Gfx::Canvas spr = Gfx::Canvas(&PanelGfx::tft());
    return spr;
}
static Gfx::Canvas &s_lampSpr()
{
    static Gfx::Canvas spr = Gfx::Canvas(&PanelGfx::tft());
    return spr;
}
static bool s_sprReady = false;

static char     s_line[2][CHARS + 1];
static uint8_t  s_lamp[LAMP_COUNT];
static int8_t   s_dcSel = -1, s_acSel = -1; // -1: never sent, so nothing is shown
static bool     s_dim      = false;
static bool     s_lampTest = false;

static uint8_t  s_dirtyLamps = 0;
static uint8_t  s_dirtyLines = 0;
static bool     s_dirtySel   = false;

static bool     s_blinkOn   = true;
static uint32_t s_blinkAt   = 0;
static bool     s_selfTest  = false;
static uint8_t  s_demoIndex = 0;
static uint32_t s_demoAt    = 0;

static void markAllDirty()
{
    s_dirtyLamps = (uint8_t)((1u << LAMP_COUNT) - 1);
    s_dirtyLines = 0x3;
    s_dirtySel   = true;
}

/* Character n of the line goes in column n. Nothing between the PMDG and the panel trims
   a line -- FSUIPC stops at the NUL, the Connector and the core pass the text through with
   its spaces -- so a short line can only have lost trailing blanks (an empty CPS FREQ field,
   say), and padding it on the right keeps every field under its placard. Anything outside
   printable ASCII shows as an unlit character. */
static void setLine(uint8_t which, const char *text)
{
    char       buf[CHARS + 1];
    const char *src = text ? text : "";
    size_t      n   = strlen(src);
    if (n > (size_t)CHARS) n = CHARS;
    memset(buf, ' ', CHARS);
    memcpy(buf, src, n);
    buf[CHARS] = 0;
    if (strcmp(buf, s_line[which]) != 0) {
        memcpy(s_line[which], buf, sizeof(buf));
        s_dirtyLines |= (uint8_t)(1u << which);
    }
}

static void drawDot(Gfx::Canvas &spr, int16_t x, int16_t y, uint32_t colour)
{
    // A 3x3 dot with its corners half-blended into the window, so it reads as round.
    const uint16_t c = Theme::to565(colour);
    const uint16_t k = Theme::to565(Theme::blend(colour, Theme::LedWindow, 90));
    spr.fillRect(x, y, DOT_SIZE, DOT_SIZE, c);
    spr.drawPixel(x, y, k);
    spr.drawPixel(x + DOT_SIZE - 1, y, k);
    spr.drawPixel(x, y + DOT_SIZE - 1, k);
    spr.drawPixel(x + DOT_SIZE - 1, y + DOT_SIZE - 1, k);
}

static void drawLine(uint8_t which)
{
    if (!s_sprReady) return;
    Gfx::Canvas &spr = s_lineSpr();
    spr.fillSprite(Theme::to565(Theme::LedWindow));

    const uint32_t on  = s_dim ? Theme::LedOnDim : Theme::LedOn;
    const uint32_t off = Theme::blend(Theme::LedOn, Theme::LedWindow, Theme::LedOffAlpha);

    for (int16_t c = 0; c < CHARS; ++c) {
        const uint8_t ch    = (uint8_t)s_line[which][c];
        const bool    shown = ch >= FONT5X7_FIRST && ch <= FONT5X7_LAST;
        for (int16_t col = 0; col < 5; ++col) {
            const uint8_t bits = shown ? pgm_read_byte(&FONT5X7[(ch - FONT5X7_FIRST) * 5 + col]) : 0;
            for (int16_t row = 0; row < 7; ++row) {
                const bool lit = (bits >> row) & 1u;
                drawDot(spr, (int16_t)(c * CHAR_STEP + col * DOT_PITCH), (int16_t)(row * DOT_PITCH),
                        lit ? on : off);
            }
        }
    }
    PanelGfx::push(spr, LINE_X, which == 0 ? LINE_Y0 : LINE_Y1);
}

static void drawLamp(uint8_t i)
{
    if (!s_sprReady) return;

    const uint8_t st = s_lampTest ? LS_ON : s_lamp[i];
    LampStyle style = Theme::lamp(st != LS_OFF ? LAMP_AMBER : LAMP_OFF, s_dim);
    if (st == LS_BLINK && !s_blinkOn) {
        style.fg   = Theme::blend(style.fg, style.bg, BLINK_OFF_ALPHA);
        style.glow = false;
    }
    PanelGfx::renderLamp(s_lampSpr(), style, FONT_SMALL, LAMP_L1[i], LAMP_L2[i], true, 0);
    PanelGfx::push(s_lampSpr(), (int16_t)(LAMP_X + i * (LAMP_W + LAMP_GAP)), LAMP_Y);
}

static void drawPlacards(const Placard *p, uint8_t n, int16_t y)
{
    for (uint8_t i = 0; i < n; ++i) {
        const int16_t w = PanelGfx::spacedTextWidth(p[i].text, 0);
        PanelGfx::drawSpacedText((int16_t)(fieldCentreX(p[i].field) - w / 2), (int16_t)(y + LABEL_H / 2),
                                 p[i].text, Theme::Placard, 0);
    }
}

static void drawChrome()
{
    PanelGfx::clearRect(0, 0, PANEL_W, LAMP_Y);
    drawPlacards(TOP_PLACARDS, 2, TOP_LABEL_Y);

    Gfx::Device &t = PanelGfx::tft();
    t.fillRoundRect(CONTENT_X, WIN_Y, CONTENT_W, WIN_H, 6, Theme::to565(Theme::LedWindow));
    t.drawRoundRect(CONTENT_X, WIN_Y, CONTENT_W, WIN_H, 6, Theme::to565(Theme::LedFrame));
    t.drawRoundRect(CONTENT_X + 1, WIN_Y + 1, CONTENT_W - 2, WIN_H - 2, 5,
                    Theme::to565(Theme::blend(Theme::LedFrame, Theme::LedWindow, 128)));

    drawPlacards(BOTTOM_PLACARDS, 3, BOT_LABEL_Y);
}

/* One selector: a muted "DC" / "AC" tag and the position name, centred in its half of the
   panel -- the half its knob sits under on the real unit. */
static void drawSelector(int16_t centreX, const char *tag, const char *name)
{
    const int16_t tagW  = PanelGfx::spacedTextWidth(tag, 1);
    const int16_t nameW = PanelGfx::spacedTextWidth(name, 0);
    const int16_t gap   = 8;
    const int16_t x     = (int16_t)(centreX - (tagW + gap + nameW) / 2);
    const int16_t y     = (int16_t)(SEL_Y + SEL_H / 2);
    PanelGfx::drawSpacedText(x, y, tag, Theme::HeaderFg, 1);
    PanelGfx::drawSpacedText((int16_t)(x + tagW + gap), y, name, Theme::Placard, 0);
}

static void drawSelectors()
{
    PanelGfx::clearRect(CONTENT_X, SEL_Y, CONTENT_W, SEL_H);
    if (s_dcSel >= 0 && s_dcSel < DC_COUNT) drawSelector(CONTENT_X + CONTENT_W / 4, "DC", DC_POSITIONS[s_dcSel]);
    if (s_acSel >= 0 && s_acSel < AC_COUNT) drawSelector(CONTENT_X + 3 * CONTENT_W / 4, "AC", AC_POSITIONS[s_acSel]);
    s_dirtySel = false;
}

static void setSelector(int8_t &slot, int8_t count, const char *payload)
{
    const int v = payload ? atoi(payload) : -1;
    const int8_t pos = (v >= 0 && v < count) ? (int8_t)v : -1;
    if (pos != slot) {
        slot       = pos;
        s_dirtySel = true;
    }
}

static void clearState()
{
    memset(s_lamp, LS_OFF, sizeof(s_lamp));
    for (uint8_t i = 0; i < 2; ++i) {
        memset(s_line[i], ' ', CHARS);
        s_line[i][CHARS] = 0;
    }
    s_dcSel = s_acSel = -1;
    markAllDirty();
}

static void applyDemo(uint8_t index)
{
    const ElecDemo &d = DEMO[index];
    setLine(0, d.top);
    setLine(1, d.bottom);
    for (uint8_t i = 0; i < LAMP_COUNT; ++i) {
        const uint8_t v = ((d.lamps >> i) & 1u) ? LS_ON : LS_OFF;
        if (s_lamp[i] != v) { s_lamp[i] = v; s_dirtyLamps |= (uint8_t)(1u << i); }
    }
    if (s_dcSel != d.dc) { s_dcSel = d.dc; s_dirtySel = true; }
    if (s_acSel != d.ac) { s_acSel = d.ac; s_dirtySel = true; }
}

void clear()
{
    s_dim      = false;
    s_lampTest = false;
    s_selfTest = false;
    clearState();
}

void init(uint8_t backlightPin)
{
    PanelGfx::begin(backlightPin);

    if (!s_sprReady) {
        s_lineSpr().setColorDepth(16);
        s_lampSpr().setColorDepth(16);
        const bool lineOk = (s_lineSpr().createSprite(LINE_W, LINE_H) != nullptr);
        const bool lampOk = (s_lampSpr().createSprite(LAMP_W, LAMP_H) != nullptr);
        s_sprReady        = lineOk && lampOk;
        if (s_sprReady) PanelGfx::useFont(s_lampSpr(), FONT_SMALL);
    }

    PanelGfx::clearPanel();
    drawChrome();

    // No self test on power-up: the panel starts blank and stays dark until MobiFlight runs
    // (see PanelGfx::isLit); MSG_SELF_TEST runs the demo on demand.
    clear();
}

void stop()
{
    if (s_sprReady) {
        s_lampSpr().unloadFont();
        s_lineSpr().deleteSprite();
        s_lampSpr().deleteSprite();
        s_sprReady = false;
    }
    // TFT_eSPI itself is deliberately not de-initialised; re-initialising it crashes.
}

void set(int16_t messageID, char *payload)
{
    // Real traffic from MobiFlight -- a value or a setting -- ends a running self test, and
    // clears the demo so nothing it showed lingers (selector names especially, which never
    // get overwritten if the selectors are not bound).
    if (s_selfTest && messageID != MSG_SELF_TEST) {
        s_selfTest = false;
        clearState();
    }

    if (messageID >= 0 && messageID < (int16_t)LAMP_COUNT) {
        const uint8_t v = payload ? (uint8_t)atoi(payload) : LS_OFF;
        if (s_lamp[messageID] != v) {
            s_lamp[messageID] = v;
            s_dirtyLamps |= (uint8_t)(1u << messageID);
        }
        return;
    }

    switch (messageID) {
    case MSG_LINE_TOP:
        setLine(0, payload);
        break;

    case MSG_LINE_BOTTOM:
        setLine(1, payload);
        break;

    case MSG_DC_SELECTOR:
        setSelector(s_dcSel, DC_COUNT, payload);
        break;

    case MSG_AC_SELECTOR:
        setSelector(s_acSel, AC_COUNT, payload);
        break;

    case MSG_ROTATION:
        PanelGfx::setRotation(payload ? (uint8_t)atoi(payload) : 1);
        PanelGfx::clearPanel();
        drawChrome();
        markAllDirty();
        break;

    case MSG_DIM:
        s_dim = payload && atoi(payload) != 0;
        markAllDirty();
        break;

    case MSG_LAMP_TEST:
        s_lampTest   = payload && atoi(payload) != 0;
        s_dirtyLamps = (uint8_t)((1u << LAMP_COUNT) - 1);
        break;

    case MSG_SELF_TEST:
        if (payload && atoi(payload) != 0) {
            s_selfTest  = true;
            s_demoIndex = 0;
            s_demoAt    = millis();
            applyDemo(0);
        }
        break;

    default:
        break;
    }
}

void update()
{
    // Keeps drawing while dark, so the panel reappears instantly showing current state.
    if (!s_sprReady) return;

    const uint32_t now = millis();

    if (s_selfTest && (now - s_demoAt) >= DEMO_DWELL_MS) {
        s_demoAt    = now;
        s_demoIndex = (uint8_t)((s_demoIndex + 1) % DEMO_COUNT);
        applyDemo(s_demoIndex);
    }

    if ((now - s_blinkAt) >= (BLINK_PERIOD_MS / 2)) {
        s_blinkAt = now;
        s_blinkOn = !s_blinkOn;
        for (uint8_t i = 0; i < LAMP_COUNT; ++i) {
            if (s_lamp[i] == LS_BLINK) s_dirtyLamps |= (uint8_t)(1u << i);
        }
    }

    for (uint8_t i = 0; i < 2; ++i) {
        if (s_dirtyLines & (1u << i)) drawLine(i);
    }
    s_dirtyLines = 0;

    for (uint8_t i = 0; i < LAMP_COUNT; ++i) {
        if (s_dirtyLamps & (1u << i)) drawLamp(i);
    }
    s_dirtyLamps = 0;

    if (s_dirtySel) drawSelectors();
}

} // namespace ElecPanel
