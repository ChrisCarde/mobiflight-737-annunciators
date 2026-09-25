#include "IrsPanel.h"
#include "Layout.h"
#include "Panel.h"
#include "PanelGfx.h"
#include "Theme.h"
#include <string.h>
#include <stdlib.h>

namespace IrsPanel
{

/* ---- Geometry -------------------------------------------------------------------
   Laid out like the real 737 IRS Mode Select Unit rather than the mockup's full-width
   bars. Every lamp is the same ~2.7:1 lens, 64x24 here. GPS / ILS / GLS sit centred
   across the top. Each IRU's four lamps form a 2x2 block -- ALIGN | ON DC over
   FAULT | DC FAIL -- with the L and R blocks side by side, as they sit above the two mode
   knobs on the aircraft.

   Fitting four lamps across 300px of content is what caps the lamp width at 64. The lamp
   group is centred vertically above the IRS title row, leaving space around it as the real
   unit does, where the knobs take the lower half.
---------------------------------------------------------------------------------- */
/* This is the sparsest panel in the set -- eleven small lamps on a screen sized for
   sixteen door lenses -- so on the larger screen the lamps take the full x1.5 the width
   allows, keeping the mode-select unit's 8:3 lamp shape, and the slack goes into the gaps
   so the group breathes the way the real unit does with its knobs below. */
#if ANNUN_PANEL_RES == 480
static const int16_t LAMP_W = 96, LAMP_H = 36;
static const int16_t TOP_GAP = 9;
static const int16_t IRU_HGAP = 8, IRU_VGAP = 13;
static const int16_t HDR_H = 15, HDR_GAP = 7, TOP_TO_HDR = 35;
static const int16_t TITLE_Y = 284, TITLE_H = 24;
static const int16_t AREA_Y = 12, TITLE_CLEARANCE = 11;
#else
static const int16_t LAMP_W = 64, LAMP_H = 24;
static const int16_t TOP_GAP = 7;
static const int16_t IRU_HGAP = 6, IRU_VGAP = 10;
static const int16_t HDR_H = 11, HDR_GAP = 5, TOP_TO_HDR = 26;
static const int16_t TITLE_Y = 213, TITLE_H = 18;
static const int16_t AREA_Y = 9, TITLE_CLEARANCE = 8;
#endif

static const int16_t CONTENT_X = LY::MARGIN;
static const int16_t CONTENT_W = LY::PANEL_W - 2 * LY::MARGIN; // 300 / 452
static const int16_t CONTENT_R = CONTENT_X + CONTENT_W;        // 310 / 466

static const int16_t TOP_X = CONTENT_X + (CONTENT_W - (3 * LAMP_W + 2 * TOP_GAP)) / 2; // 57 / 87

static const int16_t BLOCK_W   = 2 * LAMP_W + IRU_HGAP;   // 134 / 200
static const int16_t BLOCK_GAP = CONTENT_W - 2 * BLOCK_W; // 32 / 52, between the L and R blocks

static const int16_t AREA_BOTTOM = TITLE_Y - TITLE_CLEARANCE;
static const int16_t GROUP_H = LAMP_H + TOP_TO_HDR + HDR_H + HDR_GAP + 2 * LAMP_H + IRU_VGAP; // 124
static const int16_t TOP_Y   = AREA_Y + (AREA_BOTTOM - AREA_Y - GROUP_H) / 2;                  // 45
static const int16_t HDR_Y   = TOP_Y + LAMP_H + TOP_TO_HDR;                                     // 95
static const int16_t IRU_Y   = HDR_Y + HDR_H + HDR_GAP;                                         // 111

/* Same reasoning as the door panel: hand-derived geometry, checked at compile time. */
static const int16_t PANEL_W = LY::PANEL_W, PANEL_H = LY::PANEL_H;
static_assert(CONTENT_R <= PANEL_W, "IRS content is wider than the panel");
static_assert(TOP_X >= CONTENT_X && TOP_X + 3 * LAMP_W + 2 * TOP_GAP <= CONTENT_R,
              "IRS top row does not fit the content width");
static_assert(BLOCK_GAP >= IRU_HGAP, "L and R IRU blocks would run together");
static_assert(TOP_Y >= AREA_Y, "IRS lamp group is taller than the space above the title");
static_assert(IRU_Y + 2 * LAMP_H + IRU_VGAP <= AREA_BOTTOM, "IRS lamps overrun the title row");
static_assert(TITLE_Y + TITLE_H <= PANEL_H, "IRS title row overruns the panel");
static_assert(LY::fits(FONT_LARGE, "DC FAIL", LAMP_W, 3), "IRU lamp too narrow for DC FAIL");

static const char *IRU_LEGENDS[4] = {"ALIGN", "ON DC", "FAULT", "DC FAIL"};
static const char *TOP_LEGENDS[3] = {"GPS", "ILS", "GLS"};

/* Lamp indices: 0..2 top row, 3..6 L IRU, 7..10 R IRU. */
static const uint8_t IDX_L_ALIGN = 3;
static const uint8_t IDX_R_ALIGN = 7;

struct IrsDemo {
    uint16_t onMask;
    uint16_t blinkMask;
};

/* The seven demo states from the mockup, cycled on demand as a self test (MSG_SELF_TEST). */
static const IrsDemo DEMO[] = {
    {0, 0},                                               // power-up / off
    {(1u << 3) | (1u << 7), (1u << 3) | (1u << 7)},       // present position required
    {(1u << 3) | (1u << 7), 0},                           // aligning
    {(1u << 0), 0},                                       // NAV, normal
    {(1u << 1), 0},                                       // ATT mode
    {(1u << 0) | (1u << 5), 0},                           // L IRU fault
    {(1u << 4) | (1u << 8) | (1u << 10), 0},              // DC fail / on battery
};
static const uint8_t  DEMO_COUNT    = sizeof(DEMO) / sizeof(DEMO[0]);
static const uint32_t DEMO_DWELL_MS = 2500;

// Constructed on first use rather than at static-init time: PanelGfx owns the TFT_eSPI
// instance in another translation unit and its construction order is not guaranteed.
// Every IRS lamp is the same size, so one sprite renders them all.
static Gfx::Canvas &s_spr()
{
    static Gfx::Canvas spr = Gfx::Canvas(&PanelGfx::tft());
    return spr;
}
static bool s_sprReady = false;

static uint8_t  s_state[LAMP_COUNT];
static bool     s_dim      = false;
static bool     s_lampTest = false;

static uint16_t s_dirty = 0;

static bool     s_blinkOn   = true;
static uint32_t s_blinkAt   = 0;
static bool     s_selfTest  = false;
static uint8_t  s_demoIndex = 0;
static uint32_t s_demoAt    = 0;

static void markAllDirty()
{
    s_dirty = (uint16_t)((1u << LAMP_COUNT) - 1);
}

static void drawLamp(uint8_t i)
{
    if (!s_sprReady) return;

    const uint8_t st = s_lampTest ? LS_ON : s_state[i];
    const bool    isAlign = (i == IDX_L_ALIGN || i == IDX_R_ALIGN);

    LampMode mode = LAMP_OFF;
    if (st != LS_OFF) mode = isAlign ? LAMP_WHITE : LAMP_AMBER;

    LampStyle style = Theme::lamp(mode, s_dim);
    if (st == LS_BLINK && !s_blinkOn) {
        style.fg   = Theme::blend(style.fg, style.bg, BLINK_OFF_ALPHA);
        style.glow = false;
    }

    int16_t     x, y;
    const char *legend;
    if (i < 3) {
        x      = TOP_X + i * (LAMP_W + TOP_GAP);
        y      = TOP_Y;
        legend = TOP_LEGENDS[i];
    } else {
        const uint8_t slot = (uint8_t)((i - 3) % 4); // ALIGN, ON DC, FAULT, DC FAIL
        const uint8_t side = (uint8_t)((i - 3) / 4); // 0 = L IRU, 1 = R IRU
        x      = CONTENT_X + side * (BLOCK_W + BLOCK_GAP) + (slot % 2) * (LAMP_W + IRU_HGAP);
        y      = IRU_Y + (slot / 2) * (LAMP_H + IRU_VGAP);
        legend = IRU_LEGENDS[slot];
    }

    PanelGfx::renderLamp(s_spr(), style, FONT_LARGE, legend, nullptr, true, 0);
    PanelGfx::push(s_spr(), x, y);
}

static void drawChrome()
{
    // "L IRU" / "R IRU" column headers.
    for (uint8_t side = 0; side < 2; ++side) {
        const int16_t x = CONTENT_X + side * (BLOCK_W + BLOCK_GAP);
        PanelGfx::clearRect(x, HDR_Y, BLOCK_W, HDR_H);
        PanelGfx::drawSpacedText((int16_t)(x + 2), (int16_t)(HDR_Y + HDR_H / 2),
                                 side == 0 ? "L IRU" : "R IRU",
                                 Theme::IruLabel, 2);
    }

    // Bottom row: the panel title and a rule running to the right edge.
    const int16_t cy = TITLE_Y + TITLE_H / 2;
    PanelGfx::clearRect(CONTENT_X, TITLE_Y, CONTENT_W, TITLE_H);
    PanelGfx::drawSpacedText(CONTENT_X, cy, "IRS", Theme::HeaderFg, 2);
    const int16_t rx = CONTENT_X + PanelGfx::spacedTextWidth("IRS", 2) + 8;
    PanelGfx::drawRule(rx, cy, CONTENT_R - rx, Theme::Rule);
}

static void applyDemo(uint8_t index)
{
    const IrsDemo &d = DEMO[index];
    for (uint8_t i = 0; i < LAMP_COUNT; ++i) {
        if ((d.blinkMask >> i) & 1u)   s_state[i] = LS_BLINK;
        else if ((d.onMask >> i) & 1u) s_state[i] = LS_ON;
        else                           s_state[i] = LS_OFF;
    }
    markAllDirty();
}

void clear()
{
    memset(s_state, LS_OFF, sizeof(s_state));
    s_dim      = false;
    s_lampTest = false;
    s_selfTest = false;
    markAllDirty();
}

void init(uint8_t backlightPin)
{
    PanelGfx::begin(backlightPin);
    clear();

    if (!s_sprReady) {
        s_spr().setColorDepth(16);
        s_sprReady = (s_spr().createSprite(LAMP_W, LAMP_H) != nullptr);
        if (s_sprReady) PanelGfx::useFont(s_spr(), FONT_LARGE);
    }

    PanelGfx::clearPanel();
    drawChrome();

    // No self test on power-up. The panel starts with every lamp unlit and stays dark until
    // MobiFlight runs (see PanelGfx::isLit); MSG_SELF_TEST runs the demo on demand.
    s_selfTest = false;
    markAllDirty();
}

void stop()
{
    if (s_sprReady) {
        s_spr().unloadFont();
        s_spr().deleteSprite();
        s_sprReady = false;
    }
    // TFT_eSPI itself is deliberately not de-initialised; re-initialising it crashes.
}

void set(int16_t messageID, char *payload)
{
    if (s_selfTest) {
        s_selfTest = false;
        memset(s_state, LS_OFF, sizeof(s_state));
        markAllDirty();
    }

    if (messageID >= 0 && messageID < (int16_t)LAMP_COUNT) {
        const uint8_t v = payload ? (uint8_t)atoi(payload) : LS_OFF;
        if (s_state[messageID] != v) {
            s_state[messageID] = v;
            s_dirty |= (uint16_t)(1u << messageID);
        }
        return;
    }

    switch (messageID) {
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
        s_lampTest = payload && atoi(payload) != 0;
        markAllDirty();
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
            if (s_state[i] == LS_BLINK) s_dirty |= (uint16_t)(1u << i);
        }
    }

    if (s_dirty) {
        for (uint8_t i = 0; i < LAMP_COUNT; ++i) {
            if (s_dirty & (uint16_t)(1u << i)) drawLamp(i);
        }
        s_dirty = 0;
    }
}

} // namespace IrsPanel
